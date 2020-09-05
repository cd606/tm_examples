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

#include <grpcpp/grpcpp.h>
#ifdef _MSC_VER
#undef DELETE
#endif
#include <libetcd/rpc.grpc.pb.h>
#include <libetcd/kv.pb.h>

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
    ((int64_t, revision)) \
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

#define MapDataFields \
    ((RequestData, data)) \
    ((RequestID, nextID)) 

TM_BASIC_CBOR_CAPABLE_STRUCT_DEF(MapData, MapDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(MapData, MapDataFields);

class InMemoryChain {
public:
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
        return ItemType {0, "", theMap_[""].data};
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
        return ItemType {0, iter->first, iter->second.data};
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
        theMap_.insert({iter->second.nextID, MapData {std::move(toBeWritten.requestData), ""}}).first;
        return true;
    }
};

class EtcdChain {
private:
    std::shared_ptr<grpc::ChannelInterface> channel_;
    std::unique_ptr<etcdserverpb::KV::Stub> stub_;
    inline static const std::string CHAIN_PREFIX="shared_chain_test:";
    std::string headKey_;
public:
    using ItemType = RequestChainItem;
    EtcdChain(std::string const &headKey) : 
        channel_(grpc::CreateChannel("127.0.0.1:2379", grpc::InsecureChannelCredentials()))
        , stub_(etcdserverpb::KV::NewStub(channel_))
        , headKey_(headKey)
    {
    }
    ~EtcdChain() {}
    ItemType head(void *) {
        static const std::string emptyHeadDataStr = basic::bytedata_utils::RunSerializer<basic::CBOR<MapData>>::apply({MapData {}}); 

        etcdserverpb::TxnRequest txn;
        auto *cmp = txn.add_compare();
        cmp->set_result(etcdserverpb::Compare::GREATER);
        cmp->set_target(etcdserverpb::Compare::VERSION);
        cmp->set_key(CHAIN_PREFIX+headKey_);
        cmp->set_version(0);
        auto *action = txn.add_success();
        auto *get = action->mutable_request_range();
        get->set_key(CHAIN_PREFIX+headKey_);
        action = txn.add_failure();
        auto *put = action->mutable_request_put();
        put->set_key(CHAIN_PREFIX+headKey_);
        put->set_value(emptyHeadDataStr);
        action = txn.add_failure();
        get = action->mutable_request_range();
        get->set_key(CHAIN_PREFIX+headKey_);

        etcdserverpb::TxnResponse txnResp;
        grpc::ClientContext txnCtx;
        txnCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
        stub_->Txn(&txnCtx, txn, &txnResp);

        if (txnResp.succeeded()) {
            auto &kv = txnResp.responses(0).response_range().kvs(0);
            auto mapData = basic::bytedata_utils::RunDeserializer<basic::CBOR<MapData>>::apply(
                kv.value()
            );
            if (mapData) {
                return ItemType {kv.mod_revision(), headKey_, mapData->value.data};
            } else {
                return ItemType {};
            }
        } else {
            auto &kv = txnResp.responses(1).response_range().kvs(0);
            auto mapData = basic::bytedata_utils::RunDeserializer<basic::CBOR<MapData>>::apply(
                kv.value()
            );
            if (mapData) {
                return ItemType {kv.mod_revision(), headKey_, mapData->value.data};
            } else {
                return ItemType {};
            }
        }
    }
    std::optional<ItemType> fetchNext(ItemType const &current) {
        etcdserverpb::RangeRequest range;
        range.set_key(CHAIN_PREFIX+current.requestID);

        etcdserverpb::RangeResponse rangeResp;
        grpc::ClientContext rangeCtx;
        rangeCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
        stub_->Range(&rangeCtx, range, &rangeResp);

        auto &kv = rangeResp.kvs(0);
        auto mapData = basic::bytedata_utils::RunDeserializer<basic::CBOR<MapData>>::apply(
            kv.value()
        );
        if (mapData) {
            if (mapData->value.nextID != "") {
                auto nextID = mapData->value.nextID;
                etcdserverpb::RangeRequest range2;
                range2.set_key(CHAIN_PREFIX+nextID);

                etcdserverpb::RangeResponse rangeResp2;
                grpc::ClientContext rangeCtx2;
                rangeCtx2.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
                stub_->Range(&rangeCtx2, range2, &rangeResp2);

                auto &kv2 = rangeResp2.kvs(0);
                mapData = basic::bytedata_utils::RunDeserializer<basic::CBOR<MapData>>::apply(
                    kv2.value()
                );
                if (mapData) {
                    return ItemType {kv2.mod_revision(), nextID, mapData->value.data};
                } else {
                    return std::nullopt;
                }
            } else {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }
    bool appendAfter(ItemType const &current, ItemType &&toBeWritten) {
        etcdserverpb::TxnRequest txn;
        auto *cmp = txn.add_compare();
        cmp->set_result(etcdserverpb::Compare::EQUAL);
        cmp->set_target(etcdserverpb::Compare::MOD);
        cmp->set_key(CHAIN_PREFIX+current.requestID);
        cmp->set_mod_revision(current.revision);
        auto *action = txn.add_success();
        auto *put = action->mutable_request_put();
        put->set_key(CHAIN_PREFIX+current.requestID);
        put->set_value(basic::bytedata_utils::RunSerializer<basic::CBOR<MapData>>::apply(basic::CBOR<MapData> {MapData {current.requestData, toBeWritten.requestID}})); 
        action = txn.add_success();
        put = action->mutable_request_put();
        put->set_key(CHAIN_PREFIX+toBeWritten.requestID);
        put->set_value(basic::bytedata_utils::RunSerializer<basic::CBOR<MapData>>::apply(basic::CBOR<MapData> {MapData {std::move(toBeWritten.requestData), ""}})); 
        
        etcdserverpb::TxnResponse txnResp;
        grpc::ClientContext txnCtx;
        txnCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
        stub_->Txn(&txnCtx, txn, &txnResp);

        return txnResp.succeeded();
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
                return {true, {RequestChainItem {0, idStr, {std::move(x)}}}};
            } else if constexpr (std::is_same_v<T, Process>) {
                std::ostringstream oss;
                oss << "Appending request " << x;
                env->log(infra::LogLevel::Info, oss.str());
                return {true, {RequestChainItem {0, idStr, {std::move(x)}}}};
            } else {
                return {false, std::nullopt};
            }
        }, std::move(input.key().value));
    }
};

template <class A, class ClockImp, class Chain>
void run(typename A::EnvironmentType *env, Chain *chain, std::string const &part, std::function<void()> tc) {
    using TheEnvironment = typename A::EnvironmentType;

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

    
    auto keyify = A::template kleisli<RequestData>(basic::CommonFlowUtilComponents<A>::template keyify<RequestData>());
    r.registerAction("keyify", keyify);

    if (part == "" || part == "a-to-b") {
        auto transferImporter1 = ClockImp::template createVariableDurationRecurringClockImporter<RequestData>(
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
        r.registerImporter("transferImporter1", transferImporter1);
        r.execute(keyify, r.importItem(transferImporter1));
    }
    if (part == "" || part == "b-to-a") {
        auto transferImporter2 = ClockImp::template createVariableDurationRecurringClockImporter<RequestData>(
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
        r.registerImporter("transferImporter2", transferImporter2);
        r.execute(keyify, r.importItem(transferImporter2));
    }
    if (part == "" || part == "process") {
        auto processImporter = ClockImp::template createVariableDurationRecurringClockConstImporter<RequestData>(
            infra::withtime_utils::parseLocalTime("2020-01-01T10:00:00")
            , infra::withtime_utils::parseLocalTime("2020-01-01T11:00:00")
            , [](typename TheEnvironment::TimePointType const &tp) -> typename TheEnvironment::DurationType {
                return std::chrono::seconds(std::rand()%20+1);
            }
            , RequestData {{Process {}}}
        );
        r.registerImporter("processImporter", processImporter);
        r.execute(keyify, r.importItem(processImporter));
    }

    auto reqHandler = A::template fromAbstractOnOrderFacility<RequestData, bool>(new basic::simple_shared_chain::ChainWriter<A, Chain, StateFolder, RequestHandler<A>>(chain));
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
    bool useEtcd = (argc <= 2 || std::string(argv[2]) != "in-mem");
    std::string part = ((argc <= 3)?"":argv[3]);
    if (rt) {
        if (useEtcd) {
            EtcdChain etcdChain("2020-01-01-head");
            rtRun(&etcdChain, part);
        } else {
            InMemoryChain chain;
            rtRun(&chain, part);
        }
    } else {
        if (useEtcd) {
            EtcdChain etcdChain("2020-01-01-head");
            histRun(&etcdChain, part);
        } else {
            InMemoryChain chain;
            histRun(&chain, part);
        }
    }
    return 0;
}