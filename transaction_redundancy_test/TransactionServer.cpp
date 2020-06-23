#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeMonad.hpp>

#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/transaction/ExclusiveSingleKeyAsyncWatchableTransactionHandlerComponent.hpp>
#include <tm_kit/basic/transaction/ExclusiveSingleKeyAsyncWatchableStorageTransactionBroker.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/redis/RedisComponent.hpp>
#include <tm_kit/transport/redis/RedisOnOrderFacility.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>

#include <grpcpp/grpcpp.h>
#ifdef _MSC_VER
#undef DELETE
#endif
#include <libetcd/rpc.grpc.pb.h>
#include <libetcd/kv.pb.h>

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

#include "TransactionInterface.hpp"

using namespace dev::cd606::tm;
using namespace test;

class THComponent : public basic::transaction::ExclusiveSingleKeyAsyncWatchableTransactionHandlerComponent<
    TransactionDataVersion
    , TransactionKey
    , TransactionData
    , TransactionDataDelta
    , TransactionVersionComparer
> {
private:
    std::shared_ptr<grpc::ChannelInterface> channel_;
    std::string connectionString_;
    std::function<void(std::string)> logger_;
    std::thread watchThread_;
    std::atomic<bool> running_;

    Callback *watchListener_;

    TransactionVersionComparer versionCmp_;

    std::mutex dataMutex_;
    using VersionedData = infra::VersionedData<
        TransactionDataVersion, TransactionData, TransactionVersionComparer
    >;
    VersionedData data_;
    int64_t revision_;

    inline static const std::string ACCOUNT_A_KEY = "trtest:A";
    inline static const std::string ACCOUNT_B_KEY = "trtest:B";
    inline static const std::string PENDING_TRANSFERS_KEY = "trtest:pending_transfers";
    inline static const std::string WATCH_RANGE_START = "trtest:";
    inline static const std::string WATCH_RANGE_END = "trtest;"; //semicolon follows colon in ASCII table
    inline static const size_t PENDING_TRANSFER_SIZE_LIMIT = 10;

    void updateState(mvccpb::Event::EventType eventType, mvccpb::KeyValue const &kv) {
        if (kv.key() == ACCOUNT_A_KEY) {
            if (eventType == mvccpb::Event::PUT) {
                auto x = basic::bytedata_utils::RunDeserializer<
                            basic::CBOR<uint32_t>
                        >::apply(kv.value());
                if (x && kv.version() > data_.version[0]) {
                    data_.version[0] = kv.version();
                    data_.data.accountA.amount = x->value;
                }
            } else if (eventType == mvccpb::Event::DELETE) {
                data_.version[0] = 0;
                data_.data.accountA.amount = 0;
            }
        } else if (kv.key() == ACCOUNT_B_KEY) {
            if (eventType == mvccpb::Event::PUT) {
                auto x = basic::bytedata_utils::RunDeserializer<
                            basic::CBOR<uint32_t>
                        >::apply(kv.value());
                if (x && kv.version() > data_.version[1]) {
                    data_.version[1] = kv.version();
                    data_.data.accountB.amount = x->value;
                }
            } else if (eventType == mvccpb::Event::DELETE) {
                data_.version[1] = 0;
                data_.data.accountB.amount = 0;
            }
        } else if (kv.key() == PENDING_TRANSFERS_KEY) {
            if (eventType == mvccpb::Event::PUT) {
                auto x = basic::bytedata_utils::RunDeserializer<TransferList>::apply(kv.value());
                if (x && kv.version() > data_.version[2]) {
                    data_.version[2] = kv.version();
                    data_.data.pendingTransfers = *x;
                }
            } else if (eventType == mvccpb::Event::DELETE) {
                data_.version[2] = 0;
                data_.data.pendingTransfers = TransferList {};
            }
        }
    }
    void printState(std::ostream &os) const {
        os << data_.data << " (version: " << data_.version << ")";
        os << " (revision:" << revision_ << ")";
    }
    void sendState() {
        VersionedData d = data_;
        watchListener_->onValueChange(
            TransactionKey {}
            , std::move(d.version)
            , std::optional<TransactionData> {std::move(d.data)}
        );
    }
    bool oldDataIsUpToDate(TransactionDataVersion const &version, TransactionData const &oldData) {
        std::lock_guard<std::mutex> _(dataMutex_);
        if (versionCmp_(data_.version, version) || versionCmp_(version, data_.version)) {
            return false;
        }
        return (data_.data == oldData);
    }
    basic::transaction::RequestDecision runTxn(etcdserverpb::TxnRequest const &txn) {
        etcdserverpb::TxnResponse resp;

        auto txnStub = etcdserverpb::KV::NewStub(channel_);
        grpc::ClientContext txnCtx;
        txnCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
        txnStub->Txn(&txnCtx, txn, &resp);

        if (resp.succeeded()) {
            return basic::transaction::RequestDecision::Success;
        } else {
            return basic::transaction::RequestDecision::FailurePrecondition;
        }
    }
    basic::transaction::RequestDecision handleTransfer(TransactionDataVersion const &version, TransactionData const &oldData, TransferData const &transferData, bool ignoreChecks) {
        if (!ignoreChecks) {
            if (!oldDataIsUpToDate(version, oldData)) {
                return basic::transaction::RequestDecision::FailurePrecondition;
            }
            if (oldData.pendingTransfers.items.size() >= PENDING_TRANSFER_SIZE_LIMIT) {
                return basic::transaction::RequestDecision::FailureConsistency;
            }
            int32_t total = transferData.amount;
            for (auto const &item : oldData.pendingTransfers.items) {
                total += item.amount;
            }
            if (total > 0) {
                if (oldData.accountA.amount < static_cast<uint32_t>(total)) {
                    return basic::transaction::RequestDecision::FailureConsistency;
                }
            } else if (total < 0) {
                if (oldData.accountB.amount < static_cast<uint32_t>(-total)) {
                    return basic::transaction::RequestDecision::FailureConsistency;
                }
            }
        }

        TransferList l {oldData.pendingTransfers};
        l.items.push_back(transferData);

        etcdserverpb::TxnRequest txn;
        if (ignoreChecks) {
            auto *cmp = txn.add_compare();
            cmp->set_result(etcdserverpb::Compare::EQUAL);
            cmp->set_target(etcdserverpb::Compare::VERSION);
            cmp->set_key(PENDING_TRANSFERS_KEY);
            cmp->set_version(version[2]);
        } else {
            auto *cmp = txn.add_compare();
            cmp->set_result(etcdserverpb::Compare::EQUAL);
            cmp->set_target(etcdserverpb::Compare::VERSION);
            cmp->set_key(ACCOUNT_A_KEY);
            cmp->set_version(version[0]);
            cmp = txn.add_compare();
            cmp->set_result(etcdserverpb::Compare::EQUAL);
            cmp->set_target(etcdserverpb::Compare::VERSION);
            cmp->set_key(ACCOUNT_B_KEY);
            cmp->set_version(version[1]);
            cmp = txn.add_compare();
            cmp->set_result(etcdserverpb::Compare::EQUAL);
            cmp->set_target(etcdserverpb::Compare::VERSION);
            cmp->set_key(PENDING_TRANSFERS_KEY);
            cmp->set_version(version[2]);
        }
        auto *action = txn.add_success();
        auto *put = action->mutable_request_put();
        put->set_key(PENDING_TRANSFERS_KEY);
        put->set_value(basic::bytedata_utils::RunSerializer<TransferList>::apply(l));

        return runTxn(txn);
    }
    basic::transaction::RequestDecision handleProcess(TransactionDataVersion const &version, TransactionData const &oldData, bool ignoreChecks) {
        if (!ignoreChecks) {
            if (!oldDataIsUpToDate(version, oldData)) {
                return basic::transaction::RequestDecision::FailurePrecondition;
            }
        }

        int32_t total = 0;
        for (auto const &item : oldData.pendingTransfers.items) {
            total += item.amount;
        }

        if (!ignoreChecks) {
            if (total > 0) {
                if (oldData.accountA.amount < static_cast<uint32_t>(total)) {
                    return basic::transaction::RequestDecision::FailureConsistency;
                }
            } else if (total < 0) {
                if (oldData.accountB.amount < static_cast<uint32_t>(-total)) {
                    return basic::transaction::RequestDecision::FailureConsistency;
                }
            }
        }

        if (oldData.pendingTransfers.items.empty()) {
            return basic::transaction::RequestDecision::Success;
        }

        basic::CBOR<uint32_t> newA {
            oldData.accountA.amount - total
        };
        basic::CBOR<uint32_t> newB {
            oldData.accountB.amount + total
        };
        TransferList l;

        etcdserverpb::TxnRequest txn;
        auto *cmp = txn.add_compare();
        cmp->set_result(etcdserverpb::Compare::EQUAL);
        cmp->set_target(etcdserverpb::Compare::VERSION);
        cmp->set_key(ACCOUNT_A_KEY);
        cmp->set_version(version[0]);
        cmp = txn.add_compare();
        cmp->set_result(etcdserverpb::Compare::EQUAL);
        cmp->set_target(etcdserverpb::Compare::VERSION);
        cmp->set_key(ACCOUNT_B_KEY);
        cmp->set_version(version[1]);
        cmp = txn.add_compare();
        cmp->set_result(etcdserverpb::Compare::EQUAL);
        cmp->set_target(etcdserverpb::Compare::VERSION);
        cmp->set_key(PENDING_TRANSFERS_KEY);
        cmp->set_version(version[2]);

        auto *action = txn.add_success();
        auto *put = action->mutable_request_put();
        put->set_key(ACCOUNT_A_KEY);
        put->set_value(basic::bytedata_utils::RunSerializer<basic::CBOR<uint32_t>>::apply(newA));
        action = txn.add_success();
        put = action->mutable_request_put();
        put->set_key(ACCOUNT_B_KEY);
        put->set_value(basic::bytedata_utils::RunSerializer<basic::CBOR<uint32_t>>::apply(newB));
        action = txn.add_success();
        put = action->mutable_request_put();
        put->set_key(PENDING_TRANSFERS_KEY);
        put->set_value(basic::bytedata_utils::RunSerializer<TransferList>::apply(l));

        return runTxn(txn);
    }
    basic::transaction::RequestDecision handleInject(TransactionDataVersion const &version, TransactionData const &oldData, InjectData const &injectData, bool ignoreChecks) {
        if (!ignoreChecks) {
            if (!oldDataIsUpToDate(version, oldData)) {
                return basic::transaction::RequestDecision::FailurePrecondition;
            }
        }

        basic::CBOR<uint32_t> newA {
            oldData.accountA.amount + injectData.accountAIncrement
        };
        basic::CBOR<uint32_t> newB {
            oldData.accountB.amount + injectData.accountBIncrement
        };

        etcdserverpb::TxnRequest txn;
        auto *cmp = txn.add_compare();
        cmp->set_result(etcdserverpb::Compare::EQUAL);
        cmp->set_target(etcdserverpb::Compare::VERSION);
        cmp->set_key(ACCOUNT_A_KEY);
        cmp->set_version(version[0]);
        cmp = txn.add_compare();
        cmp->set_result(etcdserverpb::Compare::EQUAL);
        cmp->set_target(etcdserverpb::Compare::VERSION);
        cmp->set_key(ACCOUNT_B_KEY);
        cmp->set_version(version[1]);

        auto *action = txn.add_success();
        auto *put = action->mutable_request_put();
        put->set_key(ACCOUNT_A_KEY);
        put->set_value(basic::bytedata_utils::RunSerializer<basic::CBOR<uint32_t>>::apply(newA));
        action = txn.add_success();
        put = action->mutable_request_put();
        put->set_key(ACCOUNT_B_KEY);
        put->set_value(basic::bytedata_utils::RunSerializer<basic::CBOR<uint32_t>>::apply(newB));

        return runTxn(txn);
    }
    void runWatchThread() {
        etcdserverpb::RangeRequest range;
        range.set_key(WATCH_RANGE_START);
        range.set_range_end(WATCH_RANGE_END);

        etcdserverpb::RangeResponse initResponse;
        grpc::Status initStatus;

        etcdserverpb::WatchRequest req;
        auto *r = req.mutable_create_request();
        r->set_key(WATCH_RANGE_START);
        r->set_range_end(WATCH_RANGE_END); 
        
        etcdserverpb::WatchResponse watchResponse;

        grpc::CompletionQueue queue;
        void *tag;
        bool ok; 

        bool wasShutdown = false;

        auto initStub = etcdserverpb::KV::NewStub(channel_);
        grpc::ClientContext initCtx;
        //All the requests in this app will die out after 24 hours
        //, this is to avoid too many dead requests in server.
        //(Please notice that if the deadline is set too early, then
        //the watch stream will also be cut off at that time point)
        initCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
        std::shared_ptr<
                grpc::ClientAsyncResponseReader<etcdserverpb::RangeResponse>
            > initData { initStub->AsyncRange(&initCtx, range, &queue) };
        initData->Finish(&initResponse, &initStatus, (void *)1);

        auto watchStub = etcdserverpb::Watch::NewStub(channel_);
        grpc::ClientContext watchCtx;
        watchCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
        
        std::shared_ptr<
            grpc::ClientAsyncReaderWriter<etcdserverpb::WatchRequest, etcdserverpb::WatchResponse>
        > watchStream { watchStub->AsyncWatch(&watchCtx, &queue, (void *)2) };

        while (running_) {
            auto status = queue.AsyncNext(&tag, &ok, std::chrono::system_clock::now()+std::chrono::milliseconds(1));
            if (!running_) {
                break;
            }
            if (status == grpc::CompletionQueue::SHUTDOWN) {
                wasShutdown = true;
                break;
            }
            if (status == grpc::CompletionQueue::TIMEOUT) {
                continue;
            }
            if (!ok) {
                break;
            }
            auto tagNum = reinterpret_cast<intptr_t>(tag);
            switch (tagNum) {
            case 1:
                {
                    if (initResponse.kvs_size() > 0) {
                        std::lock_guard<std::mutex> _(dataMutex_);
                        for (auto const &kv : initResponse.kvs()) {
                            updateState(mvccpb::Event::PUT, kv);
                        }
                        revision_ = initResponse.header().revision();
                        std::ostringstream oss;
                        printState(oss);
                        logger_(oss.str());
                        sendState();
                    }
                }
                break;
            case 2:
                watchStream->Write(req, (void *)3);
                watchStream->Read(&watchResponse, (void *)4);
                break;
            case 3:
                watchStream->WritesDone((void *)5);
                break;
            case 4:
                { 
                    if (watchResponse.events_size() > 0) {
                        std::lock_guard<std::mutex> _(dataMutex_);
                        for (auto const &ev : watchResponse.events()) {
                            updateState(ev.type(), ev.kv());
                        }
                        revision_ = watchResponse.header().revision();
                        std::ostringstream oss;
                        printState(oss);
                        logger_(oss.str());
                        sendState();
                    }
                }
                watchStream->Read(&watchResponse, (void *)4);  
                break;
            default:
                break;
            }
        }
        if (!wasShutdown) {
            grpc::Status status;
            watchStream->Finish(&status, (void *)6);
        }
    }
public:
    THComponent() : 
        channel_(), connectionString_(), logger_()
        , watchThread_(), running_(false)
        , watchListener_(nullptr), versionCmp_() 
        , dataMutex_(), data_(), revision_(0)
    {}
    THComponent(THComponent &&c) : 
        channel_(std::move(c.channel_))
        , connectionString_(std::move(c.connectionString_)), logger_(std::move(c.logger_))
        , watchThread_(), running_(false)
        , watchListener_(nullptr), versionCmp_() 
        , dataMutex_(), data_(), revision_(0)
    {}
    THComponent &operator=(THComponent &&c) {
        if (this != &c) {
            channel_ = std::move(c.channel_);
            connectionString_ = std::move(c.connectionString_);
            logger_ = std::move(c.logger_);
        }
        return *this;
    } 
    THComponent(std::string const &connectionString, std::function<void(std::string)> const &logger) 
        : channel_(), connectionString_(connectionString), logger_(logger)
        , watchThread_(), running_(false)
        , watchListener_(nullptr), versionCmp_()
        , dataMutex_(), data_(), revision_(0)
    {}
    virtual ~THComponent() {
        if (running_) {
            running_ = false;
            watchThread_.join();
        }
    }
    virtual bool exclusivelyBindTo(Callback *cb) override {
        if (!watchListener_) {
            watchListener_ = cb;
            return true;
        }
        return false;
    }
    virtual void setUpInitialWatching() override {
        channel_ = grpc::CreateChannel(connectionString_, grpc::InsecureChannelCredentials());     
        running_ = true;
        watchThread_ = std::thread(&THComponent::runWatchThread, this);
        watchThread_.detach();
        logger_("TH component started");
    }
    virtual void startWatchIfNecessary(std::string const &account, TransactionKey const &key) override {
    }
    virtual void discretionallyStopWatch(std::string const &account, TransactionKey const &key) override {
    }
    virtual basic::transaction::RequestDecision
        handleInsert(
            std::string const &account
            , TransactionKey const &key
            , TransactionData const &data
            , bool ignoreConsistencyCheckAsMuchAsPossible
        ) override {
        return basic::transaction::RequestDecision::FailurePrecondition;
    }
    virtual basic::transaction::RequestDecision
        handleUpdate(
            std::string const &account
            , TransactionKey const &key
            , infra::VersionedData<TransactionDataVersion, TransactionData, TransactionVersionComparer> const &oldData
            , TransactionDataDelta const &dataDelta
            , bool ignoreConsistencyCheckAsMuchAsPossible
        ) override {
        return std::visit(
            [this,&key,&oldData,&ignoreConsistencyCheckAsMuchAsPossible](auto const &delta) 
             -> basic::transaction::RequestDecision {
                using T = std::decay_t<decltype(delta)>;
                if constexpr (std::is_same_v<TransferRequest,T>) {
                    return handleTransfer(oldData.version, oldData.data, std::get<1>(delta), ignoreConsistencyCheckAsMuchAsPossible);
                } else if constexpr (std::is_same_v<ProcessRequest,T>) {
                    return handleProcess(oldData.version, oldData.data, ignoreConsistencyCheckAsMuchAsPossible);
                } else if constexpr (std::is_same_v<InjectRequest,T>) {  
                    return handleInject(oldData.version, oldData.data, std::get<1>(delta), ignoreConsistencyCheckAsMuchAsPossible);  
                }
            }
            , dataDelta
        );
    }
    virtual basic::transaction::RequestDecision
        handleDelete(
            std::string const &account
            , TransactionKey const &key
            , infra::VersionedData<TransactionDataVersion, TransactionData, TransactionVersionComparer> const &oldData
            , bool ignoreConsistencyCheckAsMuchAsPossible
        ) override {
        return basic::transaction::RequestDecision::FailurePrecondition;
    }
};

int main(int argc, char **argv) {
    using TI = basic::transaction::SingleKeyTransactionInterface<
        TransactionKey
        , TransactionData
        , TransactionDataVersion
        , transport::BoostUUIDComponent::IDType
        , TransactionDataSummary
        , TransactionDataDelta
        , TransactionVersionComparer
    >;
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::real_time_clock::ClockComponent>,
        transport::BoostUUIDComponent,
        transport::ServerSideSimpleIdentityCheckerComponent<
            std::string
            , TI::BasicFacilityInput>,
        transport::redis::RedisComponent,
        transport::HeartbeatAndAlertComponent,
        THComponent
    >;
    using M = infra::RealTimeMonad<TheEnvironment>;
    using R = infra::MonadRunner<M>;
    using TB = basic::transaction::ExclusiveSingleKeyAsyncWatchableStorageTransactionBroker<
        M
        , TransactionKey
        , TransactionData
        , TransactionDataVersion
        , TransactionDataSummary
        , TransactionDataCheckSummary
        , TransactionDataDelta
        , infra::ArrayComparerWithSkip<int64_t, 3>
    >;

    TheEnvironment env;
    env.THComponent::operator=(
        THComponent {"127.0.0.1:2379", [&env](std::string const &s) {
            env.log(infra::LogLevel::Info, s);
        }}
    );

    transport::HeartbeatAndAlertComponentInitializer<TheEnvironment,transport::redis::RedisComponent>()
        (&env, "db_subscription server", transport::ConnectionLocator::parse("127.0.0.1:4072"));

    R r(&env);
    auto facility = M::fromAbstractOnOrderFacility<TI::FacilityInput,TI::FacilityOutput>(
        new TB()
    );
    r.registerOnOrderFacility("facility", facility);

    transport::redis::RedisOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::wrapOnOrderFacility
        <TI::BasicFacilityInput,TI::FacilityOutput>(
        r
        , facility
        , transport::ConnectionLocator::parse("127.0.0.1:6379:::test_etcd_transaction_queue")
        , "transaction_server_wrapper_"
        , std::nullopt //no hook
    );
    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "transaction_redundancy_test_server");
    r.finalize();

    env.setStatus("program", transport::HeartbeatMessage::Status::Good);
    env.sendAlert("transaction_redundancy_test.server.info", infra::LogLevel::Info, "Transaction redundancy test server started");
    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "Transaction redundancy test server started");

    infra::terminationController(infra::RunForever {});

    return 0;
}