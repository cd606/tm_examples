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
#include <tm_kit/transport/etcd_shared_chain/EtcdSharedChain.hpp>

using namespace dev::cd606::tm;

#define TransferRequestFields \
    ((std::string, from)) \
    ((std::string, to)) \
    ((uint16_t, amount))

TM_BASIC_CBOR_CAPABLE_STRUCT(TransferRequest, TransferRequestFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(TransferRequest, TransferRequestFields);

TM_BASIC_CBOR_CAPABLE_EMPTY_STRUCT(Process);
TM_BASIC_CBOR_CAPABLE_EMPTY_STRUCT_SERIALIZE(Process);

//The reason we want to wrap it is because if we keep std::variant as the data type
//then there will be some issue using it as data flow object type in our applicatives
using DataOnChain = basic::SingleLayerWrapper<std::variant<TransferRequest, Process>>;

#define StateFields \
    ((uint32_t, a)) \
    ((uint32_t, b)) \
    ((int32_t, a_pending)) \
    ((int32_t, b_pending)) \
    ((uint16_t, pendingRequestCount)) \
    ((std::string, lastSeenID))

TM_BASIC_CBOR_CAPABLE_STRUCT(State, StateFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(State, StateFields);

template <class Chain>
struct EnvValues {
    Chain *chain;
    std::string todayStr;
    EnvValues() : chain(nullptr), todayStr() {}
    EnvValues(Chain *p, std::string const &s) : chain(p), todayStr(s) {}
};

template <class Env>
class StateFolder {
private:
    Env *env_;
public:
    using ResultType = State;
    State initialize(Env *env) {
        env_ = env;
        if constexpr (std::is_convertible_v<Env *, infra::ConstValueHolderComponent<EnvValues<transport::etcd_shared_chain::InMemoryChain<DataOnChain>>> *>) {
            return State {1000, 1000, 0, 0, 0, ""};
        } else if constexpr (std::is_convertible_v<Env *, infra::ConstValueHolderComponent<EnvValues<transport::etcd_shared_chain::EtcdChain<DataOnChain>>> *>) {
            auto val = env->value().chain->template loadExtraData<State>(
                env->value().todayStr+"-state"
            );
            if (val) {
                return *val;
            } else {
                return State {1000, 1000, 0, 0, 0, ""};
            }
        } else {
            throw "StateFolder initialization error, environment is not recognized";
        }
    }
    static std::string const &chainIDForValue(State const &s) {
        return s.lastSeenID;
    }
    State fold(State const &lastState, transport::etcd_shared_chain::ChainItem<DataOnChain> const &newInfo) {
        auto id = newInfo.id;
        return std::visit([this,&lastState,&id](auto const &x) -> State {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, TransferRequest>) {
                State newState = lastState;
                if (x.from == "a") {
                    newState.a_pending -= x.amount;
                } else if (x.from == "b") {
                    newState.b_pending -= x.amount;
                }
                if (x.to == "a") {
                    newState.a_pending += x.amount;
                } else if (x.to == "b") {
                    newState.b_pending += x.amount;
                }
                ++(newState.pendingRequestCount);
                newState.lastSeenID = id;
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
                newState.lastSeenID = id;
                /*
                std::ostringstream oss;
                oss << "[StateFolder " << this << "] processing " << x << " ==> " << newState;
                env_->log(infra::LogLevel::Info, oss.str());*/
                return newState;
            } else {
                return lastState;
            }
        }, newInfo.data.value);
    }
};

template <class App>
class RequestHandler {
public:
    using ResponseType = bool;
    using InputType = DataOnChain;
    using Env = typename App::EnvironmentType;
    void initialize(Env *) {
    }
    std::tuple<ResponseType, std::optional<transport::etcd_shared_chain::ChainItem<DataOnChain>>> handleInput(Env *env, typename App::template Key<InputType> &&input, State const &currentState) {
        auto idStr = Env::id_to_string(input.id());
        return std::visit([env,&idStr,&currentState](auto &&x) -> std::tuple<ResponseType, std::optional<transport::etcd_shared_chain::ChainItem<DataOnChain>>> {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, TransferRequest>) {
                if (currentState.pendingRequestCount >= 10) {
                    env->log(infra::LogLevel::Warning, "too many pending requests");
                    return {false, std::nullopt};
                }
                if ((x.from != "a" && x.from != "b") || (x.to != "a" && x.to != "b")) {
                    env->log(infra::LogLevel::Warning, "can only transfer between a and b");
                    return {false, std::nullopt};
                }
                if (x.from == "a" && (static_cast<int64_t>(currentState.a)+static_cast<int64_t>(currentState.a_pending-x.amount) < 0)) {
                    std::ostringstream oss;
                    oss << currentState;
                    env->log(infra::LogLevel::Warning, "not enough amount "+std::to_string(x.amount)+" in a, current state is "+oss.str());
                    return {false, std::nullopt};
                }
                if (x.from == "b" && (static_cast<int64_t>(currentState.b)+static_cast<int64_t>(currentState.b_pending-x.amount) < 0)) {
                    std::ostringstream oss;
                    oss << currentState;
                    env->log(infra::LogLevel::Warning, "not enough amount "+std::to_string(x.amount)+" in b, current state is "+oss.str());
                    return {false, std::nullopt};
                }
                std::ostringstream oss;
                oss << "Appending request " << x << " with ID " << idStr << ", curent state is " << currentState;
                env->log(infra::LogLevel::Info, oss.str());
                return {true, {transport::etcd_shared_chain::ChainItem<DataOnChain> {0, idStr, {std::move(x)}}}};
            } else if constexpr (std::is_same_v<T, Process>) {
                std::ostringstream oss;
                oss << "Appending request " << x << " with ID " << idStr << ", curent state is " << currentState;
                env->log(infra::LogLevel::Info, oss.str());
                return {true, {transport::etcd_shared_chain::ChainItem<DataOnChain> {0, idStr, {std::move(x)}}}};
            } else {
                return {false, std::nullopt};
            }
        }, std::move(input.key().value));
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

    auto readerAction = A::template liftMaybe<basic::VoidStruct>(basic::simple_shared_chain::ChainReader<A, Chain, StateFolder<TheEnvironment>>(env, chain));
    r.registerAction("readerAction", readerAction);

    auto printState = A::template pureExporter<State>(
        [env](State &&s) {
            std::ostringstream oss;
            oss << "Current state: " << s;
            env->log(infra::LogLevel::Info, oss.str());
        }
    );
    r.registerExporter("printState", printState);
    r.exportItem(printState, r.execute(readerAction, r.importItem(readerClockImporter)));

    if (part == "" || part == "process") {
        auto saveState = A::template pureExporter<State>(
            [env,chain](State &&s) {
                chain->saveExtraData(env->value().todayStr+"-state", s);
            }
        );
        r.registerExporter("saveState", saveState);
        r.exportItem(saveState, r.actionAsSource(readerAction));
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
                req.from = "a";
                req.to = "b";
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
                req.from = "b";
                req.to = "a";
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

    auto reqHandler = A::template fromAbstractOnOrderFacility<DataOnChain, bool>(new basic::simple_shared_chain::ChainWriter<A, Chain, StateFolder<TheEnvironment>, RequestHandler<A>>(chain));
    r.registerOnOrderFacility("reqHandler", reqHandler);

    r.placeOrderWithFacilityAndForget(r.actionAsSource(keyify), reqHandler);

    r.finalize();

    tc();
}

template <class Chain>
void histRun(Chain *chain, std::string const &part, std::string const &todayStr) {
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
        infra::ConstValueHolderComponent<EnvValues<Chain>> {chain, todayStr}
    );  
    run<A,ClockImp,Chain>(&env, chain, part, []() {
        infra::terminationController(infra::ImmediatelyTerminate {});
    }
    , infra::withtime_utils::parseLocalTime(todayStr+"T10:00:00")
    , infra::withtime_utils::parseLocalTime(todayStr+"T11:00:00"));
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
        infra::ConstValueHolderComponent<EnvValues<Chain>> {chain, todayStr}
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
        infra::ConstValueHolderComponent<EnvValues<Chain>> {chain, todayStr}
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
        std::cout << "Usage: shared_chain_test (rt|hist|sim) (etcd1|etcd2|in-mem) [a-to-b|b-to-a|process]\n";
        return 0;
    }
    enum {
        Hist, Sim, RT
    } mode;
    if (argc <= 1) {
        mode = RT;
    } else if (std::string(argv[1]) == "hist") {
        mode = Hist;
    } else if (std::string(argv[1]) == "sim") {
        mode = Sim;
    } else {
        mode = RT;
    }
    enum {
        Etcd1, Etcd2, InMem
    } chainChoice;
    if (argc <= 2) {
        chainChoice = Etcd1; 
    } else if (std::string(argv[2]) == "in-mem") {
        chainChoice = InMem;
    } else if (std::string(argv[2]) == "etcd2") {
        chainChoice = Etcd2;
    } else {
        chainChoice = Etcd1;
    }
    std::string part = ((argc <= 3)?"":argv[3]);
    switch (mode) {
    case RT:
        if (chainChoice == InMem) {
            std::cerr << "RT run cannot use in-mem chain\n";
            return 1;
        }
        {
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
        if (chainChoice == InMem) {
            std::cerr << "Sim run cannot use in-mem chain\n";
            return 1;
        }
        {
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
                histRun(&etcdChain, part, "2020-01-01");
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
                histRun(&etcdChain, part, "2020-01-01");
            }
            break;
        case InMem:
            {
                transport::etcd_shared_chain::InMemoryChain<DataOnChain> chain;
                histRun(&chain, part, "2020-01-01");
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
