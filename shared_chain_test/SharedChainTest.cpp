#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/SinglePassIterationApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockImporter.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/etcd_shared_chain/EtcdChain.hpp>
#include <tm_kit/transport/redis_shared_chain/RedisChain.hpp>
#include <tm_kit/transport/lock_free_in_memory_shared_chain/LockFreeInMemoryChain.hpp>
#include <tm_kit/transport/lock_free_in_memory_shared_chain/LockFreeInBoostSharedMemoryChain.hpp>

using namespace dev::cd606::tm;

using AccountInfo = std::array<char,11>;
inline AccountInfo account(std::string const &x) {
    AccountInfo ret;
    std::memset(ret.data(), 0, 11);
    std::memcpy(ret.data(), x.c_str(), std::min<std::size_t>(10, x.length()));
    return ret;
}

#define TransferRequestFields \
    ((AccountInfo, from)) \
    ((AccountInfo, to)) \
    ((uint16_t, amount))

TM_BASIC_CBOR_CAPABLE_STRUCT(TransferRequest, TransferRequestFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(TransferRequest, TransferRequestFields);

TM_BASIC_CBOR_CAPABLE_EMPTY_STRUCT(Process);
TM_BASIC_CBOR_CAPABLE_EMPTY_STRUCT_SERIALIZE(Process);

//The reason we want to wrap it is because if we keep std::variant as the data type
//then there will be some issue using it as data flow object type in our applicatives
using DataOnChain = basic::CBOR<std::variant<TransferRequest, Process>>;

//The reason we use this instead of a std::string to hold lastSeenID is to
//facilitate printing as well as to make it trivially copiable. 
#ifdef _MSC_VER 
    #define LastSeenIDTypeFields \
        ((std::size_t, len)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::array<char,40>), id))
#else
    #define LastSeenIDTypeFields \
        ((std::size_t, len)) \
        (((std::array<char,40>), id))
#endif
#define StateFields \
    ((uint32_t, a)) \
    ((uint32_t, b)) \
    ((int32_t, a_pending)) \
    ((int32_t, b_pending)) \
    ((uint16_t, pendingRequestCount)) \
    ((LastSeenIDType, lastSeenID))

TM_BASIC_CBOR_CAPABLE_STRUCT(LastSeenIDType, LastSeenIDTypeFields);
TM_BASIC_CBOR_CAPABLE_STRUCT(State, StateFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(LastSeenIDType, LastSeenIDTypeFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(State, StateFields);

template <class Chain>
struct EnvValues {
    Chain *chain;
    std::string todayStr;
    bool dontLog;
    EnvValues() : chain(nullptr), todayStr(), dontLog(false) {}
    EnvValues(Chain *p, std::string const &s, bool d) : chain(p), todayStr(s), dontLog(d) {}
};

template <class Env, class Chain>
class StateFolder {
private:
    Env *env_;
public:
    using ResultType = State;
    State initialize(Env *env, Chain *chain) {
        env_ = env;
        if constexpr (Chain::SupportsExtraData) {
            auto val = chain->template loadExtraData<State>(
                env->value().todayStr+"-state"
            );
            if (val) {
                return *val;
            } else {
                return State {1000, 1000, 0, 0, 0, {0, ""}};
            }
        } else {
            return State {1000, 1000, 0, 0, 0, {0, ""}};
        }
    }
    static typename std::string chainIDForState(State const &s) {
        return std::string {s.lastSeenID.id.data(), s.lastSeenID.len};
    }
    std::optional<State> fold(State const &lastState, DataOnChain const &newInfo) {
        return std::visit([this,&lastState](auto const &x) -> std::optional<State> {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, TransferRequest>) {
                State newState = lastState;
                if (std::strcmp(x.from.data(), "a") == 0) {
                    newState.a_pending -= x.amount;
                } else if (std::strcmp(x.from.data(), "b") == 0) {
                    newState.b_pending -= x.amount;
                }
                if (std::strcmp(x.to.data(), "a") == 0) {
                    newState.a_pending += x.amount;
                } else if (std::strcmp(x.to.data(), "b") == 0) {
                    newState.b_pending += x.amount;
                }
                ++(newState.pendingRequestCount);
                /*
                std::ostringstream oss;
                oss << "[StateFolder " << this << "] processing " << x << " ==> " << newState;
                env_->log(infra::LogLevel::Info, oss.str());*/
                return newState;
            } else if constexpr (std::is_same_v<T, Process>) {
                State newState = lastState;
                newState.a += newState.a_pending;
                newState.a_pending = 0;
                newState.b += newState.b_pending;
                newState.b_pending = 0;
                newState.pendingRequestCount = 0;
                /*
                std::ostringstream oss;
                oss << "[StateFolder " << this << "] processing " << x << " ==> " << newState;
                env_->log(infra::LogLevel::Info, oss.str());*/
                return newState;
            } else {
                return std::nullopt;
            }
        }, newInfo.value);
    }
    State fold(State const &lastState, std::string_view const &storageIDView, typename Chain::DataType const *newInfo) {
        if (newInfo) {
            auto newState = fold(lastState, *newInfo);
            if (newState) {
                newState->lastSeenID.len = storageIDView.length();
                std::memset(newState->lastSeenID.id.data(), 0, 40);
                std::memcpy(newState->lastSeenID.id.data(), storageIDView.data(), storageIDView.length());
                return *newState;
            } else {
                return lastState;
            }
        } else {
            return lastState;
        }
    }
};

template <class App, class Chain>
class RequestHandler {
private:
    bool dontLog_ = false;
public:
    using ResponseType = bool;
    using InputType = DataOnChain;
    using Env = typename App::EnvironmentType;
    void initialize(Env *env, Chain *chain) {
        dontLog_ = env->value().dontLog;
    }
    std::tuple<ResponseType, std::optional<DataOnChain>> basicHandleInput(Env *env, typename App::template TimedDataType<typename App::template Key<InputType>> &&input, State const &currentState) {
        auto idStr = Env::id_to_string(input.value.id());
        return std::visit([this,env,&idStr,&currentState](auto &&x) -> std::tuple<ResponseType, std::optional<DataOnChain>> {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, TransferRequest>) {
                if (currentState.pendingRequestCount >= 10) {
                    if (!dontLog_) {
                        env->log(infra::LogLevel::Warning, "too many pending requests");
                    }
                    return {false, std::nullopt};
                }
                if ((std::strcmp(x.from.data(), "a") != 0 && std::strcmp(x.from.data(), "b") != 0) || (std::strcmp(x.to.data(), "a") != 0 && std::strcmp(x.to.data(), "b") != 0)) {
                    if (!dontLog_) {
                        env->log(infra::LogLevel::Warning, "can only transfer between a and b");
                    }
                    return {false, std::nullopt};
                }
                if (std::strcmp(x.from.data(), "a") == 0 && (static_cast<int64_t>(currentState.a)+static_cast<int64_t>(currentState.a_pending-x.amount) < 0)) {
                    if (!dontLog_) {
                        std::ostringstream oss;
                        oss << currentState;
                        env->log(infra::LogLevel::Warning, "not enough amount "+std::to_string(x.amount)+" in a, current state is "+oss.str());
                    }
                    return {false, std::nullopt};
                }
                if (std::strcmp(x.from.data(), "b") == 0 && (static_cast<int64_t>(currentState.b)+static_cast<int64_t>(currentState.b_pending-x.amount) < 0)) {
                    if (!dontLog_) {
                        std::ostringstream oss;
                        oss << currentState;
                        env->log(infra::LogLevel::Warning, "not enough amount "+std::to_string(x.amount)+" in b, current state is "+oss.str());
                    }
                    return {false, std::nullopt};
                }
                if (!dontLog_) {
                    std::ostringstream oss;
                    oss << "Appending request " << x << " with ID " << idStr << ", curent state is " << currentState;
                    env->log(infra::LogLevel::Info, oss.str());
                }
                return {true, DataOnChain {std::move(x)}};
            } else if constexpr (std::is_same_v<T, Process>) {
                if (!dontLog_) {
                    std::ostringstream oss;
                    oss << "Appending request " << x << " with ID " << idStr << ", curent state is " << currentState;
                    env->log(infra::LogLevel::Info, oss.str());
                }
                return {true, DataOnChain {std::move(x)}};
            } else {
                return {false, std::nullopt};
            }
        }, std::move(input.value.key().value));
    }
    std::tuple<ResponseType, std::optional<std::tuple<std::string, typename Chain::DataType>>> handleInput(Env *env, Chain *chain, typename App::template TimedDataType<typename App::template Key<InputType>> &&input, State const &currentState) {
        auto idStr = App::EnvironmentType::id_to_string(input.value.id());
        auto resp = basicHandleInput(env, std::move(input), currentState);
        if (std::get<1>(resp)) {
            return {
                std::get<0>(resp)
                , std::tuple<std::string, typename Chain::DataType> {
                    idStr
                    , std::move(*std::get<1>(resp))
                }
            };
        } else {
            return {std::get<0>(resp), std::nullopt};
        }
    }
};

template <class A, class ClockImp, class Chain>
void run(typename A::EnvironmentType *env, Chain *chain, std::string const &part, std::function<void()> tc, std::chrono::system_clock::time_point startTP, std::chrono::system_clock::time_point endTP) {
    using TheEnvironment = typename A::EnvironmentType;

    env->setLogFilePrefix("shared_chain_test_"+part, true);
    infra::AppRunner<A> r(env);

    std::srand(std::time(nullptr));
    
    auto readerClockImporter = ClockImp::template createRecurringClockImporter<basic::VoidStruct>(
        startTP
        , endTP
        , std::chrono::minutes(1)
        , [](typename TheEnvironment::TimePointType const &tp) {
            return basic::VoidStruct {};
        }
    );
    r.registerImporter("readerClockImporter", readerClockImporter);

    auto readerAction = basic::simple_shared_chain::ChainReader<A, Chain, StateFolder<TheEnvironment,Chain>>::action(env, chain);
    r.registerAction("readerAction", readerAction);

    auto printState = A::template pureExporter<State>(
        [env](State &&s) {
            if (!env->value().dontLog) {
                std::ostringstream oss;
                oss << "Current state: " << s;
                env->log(infra::LogLevel::Info, oss.str());
            }
        }
    );
    r.registerExporter("printState", printState);
    r.exportItem(printState, r.execute(readerAction, r.importItem(readerClockImporter)));

    if constexpr (Chain::SupportsExtraData) {
        if (part == "" || part == "process") {
            auto saveState = A::template pureExporter<State>(
                [env,chain](State &&s) {
                    chain->saveExtraData(env->value().todayStr+"-state", s);
                }
            );
            r.registerExporter("saveState", saveState);
            r.exportItem(saveState, r.actionAsSource(readerAction));
        }
    }
    
    auto keyify = A::template kleisli<DataOnChain>(basic::CommonFlowUtilComponents<A>::template keyify<DataOnChain>());
    r.registerAction("keyify", keyify);

    if (part == "" || part == "a-to-b") {
        auto transferImporter1 = ClockImp::template createVariableDurationRecurringClockImporter<DataOnChain>(
            startTP
            , endTP
            , [](typename TheEnvironment::TimePointType const &tp) -> typename TheEnvironment::DurationType {
                return std::chrono::seconds(std::rand()%5+1);
            }
            , [](typename TheEnvironment::TimePointType const &tp) -> DataOnChain {
                TransferRequest req;
                req.from = account("a");
                req.to = account("b");
                req.amount = (std::rand()%9+1)*100;
                return {{req}};
            }
        );
        r.registerImporter("transferImporter1", transferImporter1);
        r.execute(keyify, r.importItem(transferImporter1));
    }
    if (part == "" || part == "b-to-a") {
        auto transferImporter2 = ClockImp::template createVariableDurationRecurringClockImporter<DataOnChain>(
            startTP
            , endTP
            , [](typename TheEnvironment::TimePointType const &tp) -> typename TheEnvironment::DurationType {
                return std::chrono::seconds(std::rand()%5+1);
            }
            , [](typename TheEnvironment::TimePointType const &tp) -> DataOnChain {
                TransferRequest req;
                req.from = account("b");
                req.to = account("a");
                req.amount = (std::rand()%9+1)*100;
                return {{req}};
            }
        );
        r.registerImporter("transferImporter2", transferImporter2);
        r.execute(keyify, r.importItem(transferImporter2));
    }
    if (part == "" || part == "process") {
        auto processImporter = ClockImp::template createVariableDurationRecurringClockConstImporter<DataOnChain>(
            startTP
            , endTP
            , [](typename TheEnvironment::TimePointType const &tp) -> typename TheEnvironment::DurationType {
                return std::chrono::seconds(std::rand()%20+1);
            }
            , DataOnChain {{Process {}}}
        );
        r.registerImporter("processImporter", processImporter);
        r.execute(keyify, r.importItem(processImporter));
    }

    auto reqHandler = basic::simple_shared_chain::ChainWriter<A, Chain, StateFolder<TheEnvironment,Chain>, RequestHandler<A,Chain>>::onOrderFacility(chain);
    r.registerOnOrderFacility("reqHandler", reqHandler);

    r.placeOrderWithFacilityAndForget(r.actionAsSource(keyify), reqHandler);

    r.finalize();

    tc();
}

template <class Chain>
void histRun(Chain *chain, std::string const &part, std::string const &todayStr, bool dontLog) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>, false>,
        transport::CrossGuidComponent,
        infra::ConstValueHolderComponent<EnvValues<Chain>>
    >;
    using ClockImp = basic::single_pass_iteration_clock::ClockImporter<TheEnvironment>;
    using A = infra::SinglePassIterationApp<TheEnvironment>;
    TheEnvironment env;  
    env.infra::template ConstValueHolderComponent<EnvValues<Chain>>::operator=(
        infra::ConstValueHolderComponent<EnvValues<Chain>> {chain, todayStr, dontLog}
    );  
    std::chrono::steady_clock::time_point tp1, tp2;
    if (dontLog) {
        tp1 = std::chrono::steady_clock::now();
    }
    run<A,ClockImp,Chain>(&env, chain, part, []() {
        infra::terminationController(infra::ImmediatelyTerminate {});
    }
    , infra::withtime_utils::parseLocalTime(todayStr+"T10:00:00")
    , infra::withtime_utils::parseLocalTime(todayStr+"T11:00:00"));
    if (dontLog) {
        tp2 = std::chrono::steady_clock::now();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(tp2-tp1).count();
        std::cerr << "Used time: " << micros << " micros\n";
    }
}

template <class Chain>
void simRun(Chain *chain, std::string const &part, std::string const &todayStr) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::CrossGuidComponent,
        infra::ConstValueHolderComponent<EnvValues<Chain>>
    >;
    using ClockImp = basic::real_time_clock::ClockImporter<TheEnvironment>;
    using A = infra::RealTimeApp<TheEnvironment>;
    TheEnvironment env;
    auto clockSettings = TheEnvironment::clockSettingsWithStartPointCorrespondingToNextAlignment(
        1
        , todayStr+"T10:00"
        , 10.0
    );
    env.basic::real_time_clock::ClockComponent::operator=(
        basic::real_time_clock::ClockComponent(clockSettings)
    );
    env.infra::template ConstValueHolderComponent<EnvValues<Chain>>::operator=(
        infra::ConstValueHolderComponent<EnvValues<Chain>> {chain, todayStr, false}
    );
    run<A,ClockImp,Chain>(&env, chain, part, [&env,todayStr]() {
        infra::terminationController(infra::TerminateAtTimePoint {
            env.actualTime(
                infra::withtime_utils::parseLocalTime(todayStr+"T11:00:01")
            )
        });
    }
    , infra::withtime_utils::parseLocalTime(todayStr+"T10:00:00")
    , infra::withtime_utils::parseLocalTime(todayStr+"T11:00:00"));
}

template <class Chain>
void rtRun(Chain *chain, std::string const &part, std::string const &todayStr) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::CrossGuidComponent,
        infra::ConstValueHolderComponent<EnvValues<Chain>>
    >;
    using ClockImp = basic::real_time_clock::ClockImporter<TheEnvironment>;
    using A = infra::RealTimeApp<TheEnvironment>;
    TheEnvironment env;
    env.infra::template ConstValueHolderComponent<EnvValues<Chain>>::operator=(
        infra::ConstValueHolderComponent<EnvValues<Chain>> {chain, todayStr, false}
    );
    run<A,ClockImp,Chain>(&env, chain, part, []() {
        infra::terminationController(infra::TerminateAfterDuration {
            std::chrono::minutes(61)
        });
    }
    , env.now()+std::chrono::seconds(5)
    , env.now()+std::chrono::hours(1)+std::chrono::seconds(5));
}

int main(int argc, char **argv) {
    if (argc > 1 && std::string(argv[1]) == "help") {
        std::cout << "Usage: shared_chain_test (rt|hist|sim) (etcd1|etcd2|redis|in-mem|lock-free-in-mem|lock-free-in-shared-mem) [a-to-b|b-to-a|process]\n";
        return 0;
    }
    enum {
        Hist, Sim, RT, HistNoLog
    } mode;
    if (argc <= 1) {
        mode = RT;
    } else if (std::string(argv[1]) == "hist") {
        mode = Hist;
    } else if (std::string(argv[1]) == "histNoLog") {
        mode = HistNoLog;
    } else if (std::string(argv[1]) == "sim") {
        mode = Sim;
    } else {
        mode = RT;
    }
    enum {
        Etcd1, Etcd2, Redis, InMem, LockFreeInMem, LockFreeInSharedMem
    } chainChoice;
    if (argc <= 2) {
        chainChoice = Etcd1; 
    } else if (std::string(argv[2]) == "redis") {
        chainChoice = Redis;
    } else if (std::string(argv[2]) == "in-mem") {
        chainChoice = InMem;
    } else if (std::string(argv[2]) == "lock-free-in-mem") {
        chainChoice = LockFreeInMem;
    } else if (std::string(argv[2]) == "lock-free-in-shared-mem") {
        chainChoice = LockFreeInSharedMem;
    } else if (std::string(argv[2]) == "etcd2") {
        chainChoice = Etcd2;
    } else {
        chainChoice = Etcd1;
    }
    std::string part = ((argc <= 3)?"":argv[3]);
    switch (mode) {
    case RT:
        if (chainChoice == InMem || chainChoice == LockFreeInMem) {
            std::cerr << "RT run cannot use in-mem chain unless it is in-shared-mem\n";
            return 1;
        }
        if (chainChoice == LockFreeInSharedMem) {
            std::string today = infra::withtime_utils::localTimeString(std::chrono::system_clock::now()).substr(0,10);
            transport::lock_free_in_memory_shared_chain::LockFreeInBoostSharedMemoryChain<
                DataOnChain
                , transport::lock_free_in_memory_shared_chain::BoostSharedMemoryChainFastRecoverSupport::ByOffset
                , transport::lock_free_in_memory_shared_chain::BoostSharedMemoryChainExtraDataProtectionStrategy::MutexProtected
            > sharedMemChain {today+"-chain", 10*1024*1024};
            rtRun(&sharedMemChain, part, today);
        } else 
        if (chainChoice == Redis) {
            std::string today = infra::withtime_utils::localTimeString(std::chrono::system_clock::now()).substr(0,10);
            transport::redis_shared_chain::RedisChain<DataOnChain> redisChain {
                    transport::redis_shared_chain::RedisChainConfiguration()
                        .HeadKey(today+"-head")
                };
            rtRun(&redisChain, part, today);
        } else {
            std::string today = infra::withtime_utils::localTimeString(std::chrono::system_clock::now()).substr(0,10);
            transport::etcd_shared_chain::EtcdChain<DataOnChain> etcdChain {
                transport::etcd_shared_chain::EtcdChainConfiguration()
                    .HeadKey(today+"-head")
                    .SaveDataOnSeparateStorage(chainChoice == Etcd2)
                    .DuplicateFromRedis(true)
                    .RedisTTLSeconds(20)
                    .AutomaticallyDuplicateToRedis(true)
                    .UseWatchThread(true)
            };
            rtRun(&etcdChain, part, today);
        }
        break;
    case Sim:
        if (chainChoice == InMem || chainChoice == LockFreeInMem) {
            std::cerr << "Sim run cannot use in-mem chain unless it is in-shared-mem\n";
            return 1;
        }
        if (chainChoice == LockFreeInSharedMem) {
            transport::lock_free_in_memory_shared_chain::LockFreeInBoostSharedMemoryChain<
                DataOnChain
                , transport::lock_free_in_memory_shared_chain::BoostSharedMemoryChainFastRecoverSupport::ByOffset
                , transport::lock_free_in_memory_shared_chain::BoostSharedMemoryChainExtraDataProtectionStrategy::MutexProtected
            > sharedMemChain {"2020-01-01-chain", 10*1024*1024};
            simRun(&sharedMemChain, part, "2020-01-01");
        } else 
        if (chainChoice == Redis) {
            transport::redis_shared_chain::RedisChain<DataOnChain> redisChain {
                transport::redis_shared_chain::RedisChainConfiguration()
                    .HeadKey("2020-01-01-head")
            };
            simRun(&redisChain, part, "2020-01-01");
        } else {
            transport::etcd_shared_chain::EtcdChain<DataOnChain> etcdChain {
                transport::etcd_shared_chain::EtcdChainConfiguration()
                    .HeadKey("2020-01-01-head")
                    .SaveDataOnSeparateStorage(chainChoice == Etcd2)
                    .DuplicateFromRedis(true)
                    .RedisTTLSeconds(20)
                    .AutomaticallyDuplicateToRedis(true)
                    .UseWatchThread(true)
            };
            simRun(&etcdChain, part, "2020-01-01");
        }
        break;
    case Hist:
    case HistNoLog:
        switch (chainChoice) {
        case Etcd1:
            {
                transport::etcd_shared_chain::EtcdChain<DataOnChain> etcdChain {
                    transport::etcd_shared_chain::EtcdChainConfiguration()
                        .HeadKey("2020-01-01-head")
                        .SaveDataOnSeparateStorage(false)
                        .DuplicateFromRedis(false)
                        .UseWatchThread(false)
                };
                histRun(&etcdChain, part, "2020-01-01", (mode == HistNoLog));
            }
            break;
        case Etcd2:
            {
                transport::etcd_shared_chain::EtcdChain<DataOnChain> etcdChain {
                    transport::etcd_shared_chain::EtcdChainConfiguration()
                        .HeadKey("2020-01-01-head")
                        .SaveDataOnSeparateStorage(true)
                        .DuplicateFromRedis(false)
                        .UseWatchThread(false)
                };
                histRun(&etcdChain, part, "2020-01-01", (mode == HistNoLog));
            }
            break;
        case Redis:
            {
                transport::redis_shared_chain::RedisChain<DataOnChain> redisChain {
                    transport::redis_shared_chain::RedisChainConfiguration()
                        .HeadKey("2020-01-01-head")
                };
                histRun(&redisChain, part, "2020-01-01", (mode == HistNoLog));
            }
            break;
        case InMem:
            {
                transport::etcd_shared_chain::InMemoryChain<DataOnChain> chain;
                histRun(&chain, part, "2020-01-01", (mode == HistNoLog));
            }
            break;
        case LockFreeInMem:
            {
                transport::lock_free_in_memory_shared_chain::LockFreeInMemoryChain<DataOnChain> chain;
                histRun(&chain, part, "2020-01-01", (mode == HistNoLog));
            }
            break;
        case LockFreeInSharedMem:
            {
                transport::lock_free_in_memory_shared_chain::LockFreeInBoostSharedMemoryChain<
                    DataOnChain
                    , transport::lock_free_in_memory_shared_chain::BoostSharedMemoryChainFastRecoverSupport::ByName
                    , transport::lock_free_in_memory_shared_chain::BoostSharedMemoryChainExtraDataProtectionStrategy::Unsafe
                > chain {"2020-01-01-chain", 10*1024*1024};
                histRun(&chain, part, "2020-01-01", (mode == HistNoLog));
            }
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return 0;
}
