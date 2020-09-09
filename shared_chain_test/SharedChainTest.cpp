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

using Process = basic::ConstType<1>;

//The reason we want to wrap it is because if we keep std::variant as the data type
//then there will be some issue using it as data flow object type in our applicatives
using DataOnChain = basic::SingleLayerWrapper<std::variant<TransferRequest, Process>>;

struct State {
    uint32_t a, b;
    int32_t a_pending, b_pending;
    uint16_t pendingRequestCount;

    State() : a(0), b(0), a_pending(0), b_pending(0), pendingRequestCount(0) {}
    static State initState() {
        State s;
        s.a = 1000;
        s.b = 1000;
        return s;
    }
};

inline std::ostream &operator<<(std::ostream &os, State const &s) {
    os << "{a=" << s.a << ",b=" << s.b << ",a_pending=" << s.a_pending
        << ",b_pending=" << s.b_pending << ",pendingRequestCount=" << s.pendingRequestCount
        << "}";
    return os;
}

class StateFolder {
public:
    using ResultType = State;
    State initialize(void *) {
        return State::initState();
    } 
    State fold(State const &lastState, transport::etcd_shared_chain::ChainItem<DataOnChain> const &newInfo) {
        return std::visit([&lastState](auto const &x) -> State {
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
                return newState;
            } else if constexpr (std::is_same_v<T, Process>) {
                State newState = lastState;
                newState.a += newState.a_pending;
                newState.a_pending = 0;
                newState.b += newState.b_pending;
                newState.b_pending = 0;
                newState.pendingRequestCount = 0;
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
                    env->log(infra::LogLevel::Warning, "not enough amount "+std::to_string(x.amount)+" in a");
                    return {false, std::nullopt};
                }
                if (x.from == "b" && (static_cast<int64_t>(currentState.b)+static_cast<int64_t>(currentState.b_pending-x.amount) < 0)) {
                    env->log(infra::LogLevel::Warning, "not enough amount "+std::to_string(x.amount)+" in b");
                    return {false, std::nullopt};
                }
                std::ostringstream oss;
                oss << "Appending request " << x;
                env->log(infra::LogLevel::Info, oss.str());
                return {true, {transport::etcd_shared_chain::ChainItem<DataOnChain> {0, idStr, {std::move(x)}}}};
            } else if constexpr (std::is_same_v<T, Process>) {
                std::ostringstream oss;
                oss << "Appending request " << x;
                env->log(infra::LogLevel::Info, oss.str());
                return {true, {transport::etcd_shared_chain::ChainItem<DataOnChain> {0, idStr, {std::move(x)}}}};
            } else {
                return {false, std::nullopt};
            }
        }, std::move(input.key().value));
    }
};

template <class A, class ClockImp, class Chain>
void run(typename A::EnvironmentType *env, Chain *chain, std::string const &part, std::function<void()> tc) {
    using TheEnvironment = typename A::EnvironmentType;

    env->setLogFilePrefix("shared_chain_test_"+part, true);
    infra::AppRunner<A> r(env);

    std::srand(std::time(nullptr));
    
    auto readerClockImporter = ClockImp::template createRecurringClockImporter<basic::VoidStruct>(
        infra::withtime_utils::parseLocalTime("2020-01-01T10:00:00")
        , infra::withtime_utils::parseLocalTime("2020-01-01T11:00:00")
        , std::chrono::minutes(1)
        , [](typename TheEnvironment::TimePointType const &tp) {
            return basic::VoidStruct {};
        }
    );
    r.registerImporter("readerClockImporter", readerClockImporter);

    auto readerAction = A::template liftMaybe<basic::VoidStruct>(basic::simple_shared_chain::ChainReader<A, Chain, StateFolder>(env, chain));
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
    
    auto keyify = A::template kleisli<DataOnChain>(basic::CommonFlowUtilComponents<A>::template keyify<DataOnChain>());
    r.registerAction("keyify", keyify);

    if (part == "" || part == "a-to-b") {
        auto transferImporter1 = ClockImp::template createVariableDurationRecurringClockImporter<DataOnChain>(
            infra::withtime_utils::parseLocalTime("2020-01-01T10:00:00")
            , infra::withtime_utils::parseLocalTime("2020-01-01T11:00:00")
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
            infra::withtime_utils::parseLocalTime("2020-01-01T10:00:00")
            , infra::withtime_utils::parseLocalTime("2020-01-01T11:00:00")
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
            infra::withtime_utils::parseLocalTime("2020-01-01T10:00:00")
            , infra::withtime_utils::parseLocalTime("2020-01-01T11:00:00")
            , [](typename TheEnvironment::TimePointType const &tp) -> typename TheEnvironment::DurationType {
                return std::chrono::seconds(std::rand()%20+1);
            }
            , DataOnChain {{Process {}}}
        );
        r.registerImporter("processImporter", processImporter);
        r.execute(keyify, r.importItem(processImporter));
    }

    auto reqHandler = A::template fromAbstractOnOrderFacility<DataOnChain, bool>(new basic::simple_shared_chain::ChainWriter<A, Chain, StateFolder, RequestHandler<A>>(chain));
    r.registerOnOrderFacility("reqHandler", reqHandler);

    r.placeOrderWithFacilityAndForget(r.actionAsSource(keyify), reqHandler);

    r.finalize();

    tc();
}

template <class Chain>
void histRun(Chain *chain, std::string const &part) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>, false>,
        transport::CrossGuidComponent
    >;
    using ClockImp = basic::single_pass_iteration_clock::ClockImporter<TheEnvironment>;
    using A = infra::SinglePassIterationApp<TheEnvironment>;
    TheEnvironment env;    
    run<A,ClockImp,Chain>(&env, chain, part, []() {
        infra::terminationController(infra::ImmediatelyTerminate {});
    });
}

template <class Chain>
void rtRun(Chain *chain, std::string const &part) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::CrossGuidComponent
    >;
    using ClockImp = basic::real_time_clock::ClockImporter<TheEnvironment>;
    using A = infra::RealTimeApp<TheEnvironment>;
    TheEnvironment env;
    auto clockSettings = TheEnvironment::clockSettingsWithStartPointCorrespondingToNextAlignment(
        1
        , "2020-01-01T10:00"
        , 10.0
    );
    env.basic::real_time_clock::ClockComponent::operator=(
        basic::real_time_clock::ClockComponent(clockSettings)
    );
    run<A,ClockImp,Chain>(&env, chain, part, [&env]() {
        infra::terminationController(infra::TerminateAtTimePoint {
            env.actualTime(
                infra::withtime_utils::parseLocalTime("2020-01-01T11:00:01")
            )
        });
    });
}

int main(int argc, char **argv) {
    bool rt = (argc == 1 || std::string(argv[1]) != "hist");
    std::string chainChoice = (argc<=2)?"etcd1":std::string(argv[2]);
    std::string part = ((argc <= 3)?"":argv[3]);
    if (rt) {
        if (chainChoice == "in-mem") {
            transport::etcd_shared_chain::InMemoryChain<DataOnChain> chain;
            rtRun(&chain, part);
        } else if (chainChoice == "etcd1") {
            transport::etcd_shared_chain::EtcdChain<DataOnChain> etcdChain {
                transport::etcd_shared_chain::EtcdChainConfiguration()
                    .HeadKey("2020-01-01-head")
                    .SaveDataOnSeparateStorage(false)
                    .DuplicateFromRedis(true)
                    .RedisTTLSeconds(20)
                    .AutomaticallyDuplicateToRedis(true)
            };
            rtRun(&etcdChain, part);
        } else {
            transport::etcd_shared_chain::EtcdChain<DataOnChain> etcdChain {
                transport::etcd_shared_chain::EtcdChainConfiguration()
                    .HeadKey("2020-01-01-head")
                    .SaveDataOnSeparateStorage(true)
                    .DuplicateFromRedis(true)
                    .RedisTTLSeconds(20)
                    .AutomaticallyDuplicateToRedis(true)
            };
            rtRun(&etcdChain, part);
        }
    } else {
        if (chainChoice == "in-mem") {
            transport::etcd_shared_chain::InMemoryChain<DataOnChain> chain;
            histRun(&chain, part);
        } else if (chainChoice == "etcd1") {
            transport::etcd_shared_chain::EtcdChain<DataOnChain> etcdChain {
                transport::etcd_shared_chain::EtcdChainConfiguration()
                    .HeadKey("2020-01-01-head")
                    .SaveDataOnSeparateStorage(false)
                    .DuplicateFromRedis(false)
            };
            histRun(&etcdChain, part);
        } else {
            transport::etcd_shared_chain::EtcdChain<DataOnChain> etcdChain {
                transport::etcd_shared_chain::EtcdChainConfiguration()
                    .HeadKey("2020-01-01-head")
                    .SaveDataOnSeparateStorage(true)
                    .DuplicateFromRedis(false)
            };
            histRun(&etcdChain, part);
        }
    }
    return 0;
}
