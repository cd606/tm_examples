#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/SinglePassIterationApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/simple_shared_chain/ChainReader.hpp>
#include <tm_kit/basic/simple_shared_chain/ChainWriter.hpp>
#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockImporter.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>

using namespace dev::cd606::tm;

#define TransferRequestFields \
    ((std::string, from)) \
    ((std::string, to)) \
    ((uint16_t, amount))

TM_BASIC_CBOR_CAPABLE_STRUCT(TransferRequest, TransferRequestFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(TransferRequest, TransferRequestFields);

using Process = basic::ConstType<1>;

using RequestData = basic::CBOR<std::variant<TransferRequest, Process>>;
using RequestID = std::string;

#define RequestChainItemFields \
    ((RequestID, requestID)) \
    ((RequestData, requestData)) 

TM_BASIC_CBOR_CAPABLE_STRUCT_DEF(RequestChainItem, RequestChainItemFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(RequestChainItem, RequestChainItemFields);

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

class InMemoryChain {
public:
    struct MapData {
        RequestChainItem data;
        RequestID nextID;

        MapData() : data(), nextID() {}
        MapData(RequestChainItem &&d) : data(std::move(d)), nextID() {}
    };
    using TheMap = std::unordered_map<RequestID, MapData>;
    using ItemType = RequestChainItem;
private:
    TheMap theMap_;
    std::mutex mutex_;
public:
    InMemoryChain() : theMap_({{"", MapData {}}}), mutex_() {
    }
    ItemType head(void *) {
        std::lock_guard<std::mutex> _(mutex_);
        return theMap_[""].data;
    }
    std::optional<ItemType> fetchNext(ItemType const &current) {
        std::lock_guard<std::mutex> _(mutex_);
        auto iter = theMap_.find(current.requestID);
        if (iter == theMap_.end()) {
            return std::nullopt;
        }
        if (iter->second.nextID == "") {
            return std::nullopt;
        }
        iter = theMap_.find(iter->second.nextID);
        if (iter == theMap_.end()) {
            return std::nullopt;
        }
        return {iter->second.data};
    }
    bool appendAfter(ItemType const &current, ItemType &&toBeWritten) {
        std::lock_guard<std::mutex> _(mutex_);
        if (theMap_.find(toBeWritten.requestID) != theMap_.end()) {
            return false;
        }
        auto iter = theMap_.find(current.requestID);
        if (iter == theMap_.end()) {
            return false;
        }
        if (iter->second.nextID != "") {
            return false;
        }
        iter->second.nextID = toBeWritten.requestID;
        theMap_.insert({iter->second.nextID, MapData {std::move(toBeWritten)}}).first;
        return true;
    }
};

class StateFolder {
public:
    using ResultType = State;
    State initialize(void *) {
        return State::initState();
    } 
    State fold(State const &lastState, RequestChainItem const &newInfo) {
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
        }, newInfo.requestData.value);
    }
};

template <class App>
class RequestHandler {
public:
    using ResponseType = bool;
    using InputType = RequestData;
    using Env = typename App::EnvironmentType;
    void initialize(Env *) {
    }
    std::tuple<ResponseType, std::optional<RequestChainItem>> handleInput(Env *env, typename App::Key<InputType> &&input, State const &currentState) {
        auto idStr = Env::id_to_string(input.id());
        return std::visit([env,&idStr,&currentState](auto &&x) -> std::tuple<ResponseType, std::optional<RequestChainItem>> {
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
                return {true, {RequestChainItem {idStr, {std::move(x)}}}};
            } else if constexpr (std::is_same_v<T, Process>) {
                std::ostringstream oss;
                oss << "Appending request " << x;
                env->log(infra::LogLevel::Info, oss.str());
                return {true, {RequestChainItem {idStr, {std::move(x)}}}};
            } else {
                return {false, std::nullopt};
            }
        }, std::move(input.key().value));
    }
};

void histRun() {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>, false>,
        transport::CrossGuidComponent
    >;
    using A = infra::SinglePassIterationApp<TheEnvironment>;

    TheEnvironment env;
    infra::AppRunner<A> r(&env);

    InMemoryChain chain;

    std::srand(std::time(nullptr));
    
    auto readerClockImporter = basic::single_pass_iteration_clock::ClockImporter<TheEnvironment>::createRecurringClockImporter<basic::VoidStruct>(
        infra::withtime_utils::parseLocalTime("2020-01-01T10:00:00")
        , infra::withtime_utils::parseLocalTime("2020-01-01T11:00:00")
        , std::chrono::minutes(1)
        , [](typename TheEnvironment::TimePointType const &tp) {
            return basic::VoidStruct {};
        }
    );
    r.registerImporter("readerClockImporter", readerClockImporter);

    auto readerAction = A::liftMaybe<basic::VoidStruct>(basic::simple_shared_chain::ChainReader<A, InMemoryChain, StateFolder>(&env, &chain));
    r.registerAction("readerAction", readerAction);

    auto printState = A::pureExporter<State>(
        [&env](State &&s) {
            std::ostringstream oss;
            oss << "Current state: " << s;
            env.log(infra::LogLevel::Info, oss.str());
        }
    );
    r.registerExporter("printState", printState);
    r.exportItem(printState, r.execute(readerAction, r.importItem(readerClockImporter)));

    auto transferImporter1 = basic::single_pass_iteration_clock::ClockImporter<TheEnvironment>::createVariableDurationRecurringClockImporter<RequestData>(
        infra::withtime_utils::parseLocalTime("2020-01-01T10:00:00")
        , infra::withtime_utils::parseLocalTime("2020-01-01T11:00:00")
        , [](typename TheEnvironment::TimePointType const &tp) -> typename TheEnvironment::DurationType {
            return std::chrono::seconds(std::rand()%5+1);
        }
        , [](typename TheEnvironment::TimePointType const &tp) -> RequestData {
            TransferRequest req;
            req.from = "a";
            req.to = "b";
            req.amount = (std::rand()%9+1)*100;
            return {{req}};
        }
    );
    auto transferImporter2 = basic::single_pass_iteration_clock::ClockImporter<TheEnvironment>::createVariableDurationRecurringClockImporter<RequestData>(
        infra::withtime_utils::parseLocalTime("2020-01-01T10:00:00")
        , infra::withtime_utils::parseLocalTime("2020-01-01T11:00:00")
        , [](typename TheEnvironment::TimePointType const &tp) -> typename TheEnvironment::DurationType {
            return std::chrono::seconds(std::rand()%5+1);
        }
        , [](typename TheEnvironment::TimePointType const &tp) -> RequestData {
            TransferRequest req;
            req.from = "b";
            req.to = "a";
            req.amount = (std::rand()%9+1)*100;
            return {{req}};
        }
    );
    auto processImporter = basic::single_pass_iteration_clock::ClockImporter<TheEnvironment>::createVariableDurationRecurringClockConstImporter<RequestData>(
        infra::withtime_utils::parseLocalTime("2020-01-01T10:00:00")
        , infra::withtime_utils::parseLocalTime("2020-01-01T11:00:00")
        , [](typename TheEnvironment::TimePointType const &tp) -> typename TheEnvironment::DurationType {
            return std::chrono::seconds(std::rand()%20+1);
        }
        , RequestData {{Process {}}}
    );
    r.registerImporter("transferImporter1", transferImporter1);
    r.registerImporter("transferImporter2", transferImporter2);
    r.registerImporter("processImporter", processImporter);

    auto keyify = A::kleisli<RequestData>(basic::CommonFlowUtilComponents<A>::keyify<RequestData>());
    r.registerAction("keyify", keyify);

    r.execute(keyify, r.importItem(transferImporter1));
    r.execute(keyify, r.importItem(transferImporter2));
    r.execute(keyify, r.importItem(processImporter));

    auto reqHandler = A::fromAbstractOnOrderFacility<RequestData, bool>(new basic::simple_shared_chain::ChainWriter<A, InMemoryChain, StateFolder, RequestHandler<A>>(&chain));
    r.registerOnOrderFacility("reqHandler", reqHandler);

    r.placeOrderWithFacilityAndForget(r.actionAsSource(keyify), reqHandler);

    r.finalize();
}

int main(int argc, char **argv) {
    histRun();
    return 0;
}