#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeMonad.hpp>

#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/transaction/SingleKeyAsyncWatchableTransactionHandlerComponent.hpp>
#include <tm_kit/basic/transaction/SingleKeyAsyncWatchableStorageTransactionBroker.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/redis/RedisComponent.hpp>
#include <tm_kit/transport/redis/RedisOnOrderFacility.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>

#include <grpcpp/grpcpp.h>
#include <libetcd/rpc.grpc.pb.h>
#include <libetcd/kv.pb.h>

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

#include "TransactionInterface.hpp"

using namespace dev::cd606::tm;
using namespace test;

class THComponent : public basic::transaction::SingleKeyAsyncWatchableTransactionHandlerComponent<
    transport::BoostUUIDComponent::IDType
    , TransactionDataVersion
    , TransactionKey
    , TransactionData
    , TransactionDataSummary
    , TransactionDataDelta
> {
private:
    std::shared_ptr<grpc::ChannelInterface> channel_;
    std::string connectionString_;
    std::function<void(std::string)> logger_;
    std::thread transactionThread_, watchThread_;
    std::atomic<bool> running_;
    std::mutex transactionQueueMutex_;
    std::condition_variable transactionQueueCond_;
    using TransactionQueue = std::deque<std::tuple<
        transport::BoostUUIDComponent::IDType, TransactionDataVersion, TransactionDataDelta, bool, Callback *
    >>;
    std::array<TransactionQueue, 2> transactionQueues_;
    size_t transactionQueueIncomingIndex_;
    infra::ArrayComparerWithSkip<int64_t, 3> versionCmp_;
    std::mutex watchListenerMutex_;
    std::vector<Callback *> watchListeners_;

    std::mutex dataMutex_;
    using AccountVarType = infra::VersionedData<int64_t, std::optional<basic::CBOR<uint32_t>>>;
    using PendingTransferVarType = infra::VersionedData<int64_t, std::optional<TransferList>>;
    AccountVarType accountA_, accountB_;
    PendingTransferVarType pendingTransfers_;
    int64_t revision_;

    inline static const std::string ACCOUNT_A_KEY = "trtest:A";
    inline static const std::string ACCOUNT_B_KEY = "trtest:B";
    inline static const std::string PENDING_TRANSFERS_KEY = "trtest:pending_transfers";
    inline static const std::string WATCH_RANGE_START = "trtest:";
    inline static const std::string WATCH_RANGE_END = "trtest;"; //semicolon follows colon in ASCII table
    inline static const size_t PENDING_TRANSFER_SIZE_LIMIT = 10;
private:
    void updateState(mvccpb::Event::EventType eventType, mvccpb::KeyValue const &kv) {
        if (kv.key() == ACCOUNT_A_KEY) {
            if (eventType == mvccpb::Event::PUT) {
                auto x = basic::bytedata_utils::RunDeserializer<
                            basic::CBOR<uint32_t>
                        >::apply(kv.value());
                infra::withtime_utils::updateVersionedData(
                    accountA_
                    , AccountVarType {
                        (x ? kv.version() : 0)
                        , std::move(x)
                    }
                );
            } else if (eventType == mvccpb::Event::DELETE) {
                infra::withtime_utils::updateVersionedData(
                    accountA_
                    , AccountVarType {
                        0
                        , std::nullopt
                    }
                );
            }
        } else if (kv.key() == ACCOUNT_B_KEY) {
            if (eventType == mvccpb::Event::PUT) {
                auto x = basic::bytedata_utils::RunDeserializer<
                            basic::CBOR<uint32_t>
                        >::apply(kv.value());
                infra::withtime_utils::updateVersionedData(
                    accountB_
                    , AccountVarType {
                        (x ? kv.version() : 0)
                        , std::move(x)
                    }
                );
            } else if (eventType == mvccpb::Event::DELETE) {
                infra::withtime_utils::updateVersionedData(
                    accountB_
                    , AccountVarType {
                        0
                        , std::nullopt
                    }
                );
            }
        } else if (kv.key() == PENDING_TRANSFERS_KEY) {
            if (eventType == mvccpb::Event::PUT) {
                auto x = basic::bytedata_utils::RunDeserializer<TransferList>::apply(kv.value());
                infra::withtime_utils::updateVersionedData(
                    pendingTransfers_
                    , PendingTransferVarType {
                        (x ? kv.version() : 0)
                        , std::move(x)
                    }
                );
            } else if (eventType == mvccpb::Event::DELETE) {
                infra::withtime_utils::updateVersionedData(
                    pendingTransfers_
                    , PendingTransferVarType {
                        0
                        , std::nullopt
                    }
                );
            }
        }
    }
    void printState(std::ostream &os) const {
        os << "{accountA: {version:" << accountA_.version << ",value:";
        if (accountA_.data) {
            os << accountA_.data->value;
        } else {
            os << "(none)";
        }
        os << ", accountB: {version:" << accountB_.version << ",value:";
        if (accountB_.data) {
            os << accountB_.data->value;
        } else {
            os << "(none)";
        }
        os << ", pendingTransfers: {version:" << pendingTransfers_.version<< ",value:";
        if (pendingTransfers_.data) {
            os << "[";
            for (auto const &item : pendingTransfers_.data->items) {
                os << item << ' ';
            }
            os << "]";
        } else {
            os << "(none)";
        }
        os << "} (revision:" << revision_ << ")";
    }
    void innerSendState(Callback *cb) {
        cb->onValueChange(
            TransactionKey {}
            , TransactionDataVersion {
                accountA_.version, accountB_.version, pendingTransfers_.version
            }
            , TransactionData {
                AccountData {
                    accountA_.data
                    ? accountA_.data->value
                    : 0}
                , AccountData {
                    accountB_.data
                    ? accountB_.data->value
                    : 0}
                , (
                    pendingTransfers_.data
                    ? *(pendingTransfers_.data)
                    : TransferList {std::vector<TransferData> {}}
                )
            }
        );
    }
    void sendState() {
        std::lock_guard<std::mutex> _(watchListenerMutex_);
        for (auto const &l : watchListeners_) {
            innerSendState(l);
        }
    }
    basic::transaction::RequestDecision tryTransfer(std::optional<TransactionDataVersion> const &version, TransferData const &transferData, bool ignoreChecks) {
        std::lock_guard<std::mutex> _(dataMutex_);
        TransactionDataVersion myVersion {
            accountA_.version, accountB_.version, pendingTransfers_.version
        };
        if (version) {
            if (versionCmp_(myVersion, *version) || versionCmp_(*version, myVersion)) {
                return basic::transaction::RequestDecision::FailurePrecondition;
            }
        }
        int32_t total = transferData.amount;
        if (pendingTransfers_.data) {
            if (!ignoreChecks && pendingTransfers_.data->items.size() >= PENDING_TRANSFER_SIZE_LIMIT) {
                return basic::transaction::RequestDecision::FailureConsistency;
            }
            for (auto const &item : pendingTransfers_.data->items) {
                total += item.amount;
            }
        }
        
        if (!ignoreChecks) {
            if (total > 0) {
                if (!accountA_.data || accountA_.data->value < static_cast<uint32_t>(total)) {
                    return basic::transaction::RequestDecision::FailureConsistency;
                }
            } else if (total < 0) {
                if (!accountB_.data || accountB_.data->value < static_cast<uint32_t>(-total)) {
                    return basic::transaction::RequestDecision::FailureConsistency;
                }
            }
        }
        if (total == 0) {
            return basic::transaction::RequestDecision::Success;
        }

        TransferList l;
        if (pendingTransfers_.data) {
            l = *(pendingTransfers_.data);
        }
        l.items.push_back(transferData);

        etcdserverpb::TxnRequest txn;
        auto *cmp = txn.add_compare();
        cmp->set_result(etcdserverpb::Compare::EQUAL);
        cmp->set_target(etcdserverpb::Compare::VERSION);
        cmp->set_key(ACCOUNT_A_KEY);
        cmp->set_version(accountA_.version);
        cmp = txn.add_compare();
        cmp->set_result(etcdserverpb::Compare::EQUAL);
        cmp->set_target(etcdserverpb::Compare::VERSION);
        cmp->set_key(ACCOUNT_B_KEY);
        cmp->set_version(accountB_.version);
        cmp = txn.add_compare();
        cmp->set_result(etcdserverpb::Compare::EQUAL);
        cmp->set_target(etcdserverpb::Compare::VERSION);
        cmp->set_key(PENDING_TRANSFERS_KEY);
        cmp->set_version(pendingTransfers_.version);
        auto *action = txn.add_success();
        auto *put = action->mutable_request_put();
        put->set_key(PENDING_TRANSFERS_KEY);
        put->set_value(basic::bytedata_utils::RunSerializer<TransferList>::apply(l));
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
    void handleTransfer(transport::BoostUUIDComponent::IDType id, TransactionDataVersion const &oldVersion, TransferData const &transferData, bool ignoreChecks, Callback *cb) {
        if (ignoreChecks) {
            basic::transaction::RequestDecision res;
            while ((res = tryTransfer(std::nullopt, transferData, ignoreChecks)) != basic::transaction::RequestDecision::Success) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            cb->onRequestDecision(id, res);
        } else {
            cb->onRequestDecision(id, tryTransfer(oldVersion, transferData, false));
        }
    }
    basic::transaction::RequestDecision tryProcess(std::optional<TransactionDataVersion> const &version, bool ignoreChecks) {
        std::lock_guard<std::mutex> _(dataMutex_);
        TransactionDataVersion myVersion {
            accountA_.version, accountB_.version, pendingTransfers_.version
        };
        if (version) {
            if (versionCmp_(myVersion, *version) || versionCmp_(*version, myVersion)) {
                return basic::transaction::RequestDecision::FailurePrecondition;
            }
        }
        if (!pendingTransfers_.data) {
            return basic::transaction::RequestDecision::Success;
        }
        int32_t total = 0;
        for (auto const &item : pendingTransfers_.data->items) {
            total += item.amount;
        }
        if (!ignoreChecks) {
            if (total > 0) {
                if (!accountA_.data || accountA_.data->value < static_cast<uint32_t>(total)) {
                    return basic::transaction::RequestDecision::FailureConsistency;
                }
            } else if (total < 0) {
                if (!accountB_.data || accountB_.data->value < static_cast<uint32_t>(-total)) {
                    return basic::transaction::RequestDecision::FailureConsistency;
                }
            }
        }
        if (total == 0) {
            return basic::transaction::RequestDecision::Success;
        }
        
        basic::CBOR<uint32_t> newA {
            static_cast<uint32_t>(accountA_.data ? static_cast<int32_t>(accountA_.data->value)-total : total)
        };
        basic::CBOR<uint32_t> newB {
            static_cast<uint32_t>(accountB_.data ? static_cast<int32_t>(accountB_.data->value)+total : total)
        };
        TransferList l;

        etcdserverpb::TxnRequest txn;
        auto *cmp = txn.add_compare();
        cmp->set_result(etcdserverpb::Compare::EQUAL);
        cmp->set_target(etcdserverpb::Compare::VERSION);
        cmp->set_key(ACCOUNT_A_KEY);
        cmp->set_version(accountA_.version);
        cmp = txn.add_compare();
        cmp->set_result(etcdserverpb::Compare::EQUAL);
        cmp->set_target(etcdserverpb::Compare::VERSION);
        cmp->set_key(ACCOUNT_B_KEY);
        cmp->set_version(accountB_.version);
        cmp = txn.add_compare();
        cmp->set_result(etcdserverpb::Compare::EQUAL);
        cmp->set_target(etcdserverpb::Compare::VERSION);
        cmp->set_key(PENDING_TRANSFERS_KEY);
        cmp->set_version(pendingTransfers_.version);
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
    void handleProcess(transport::BoostUUIDComponent::IDType id, TransactionDataVersion const &oldVersion, bool ignoreChecks, Callback *cb) {
        if (ignoreChecks) {
            basic::transaction::RequestDecision res;
            while ((res = tryProcess(std::nullopt, true)) != basic::transaction::RequestDecision::Success) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            cb->onRequestDecision(id, res);
        } else {
            cb->onRequestDecision(id, tryProcess(oldVersion, false));
        }
    }
    basic::transaction::RequestDecision tryInject(std::optional<TransactionDataVersion> const &version, InjectData const &injectData, bool ignoreChecks) {
        std::lock_guard<std::mutex> _(dataMutex_);
        TransactionDataVersion myVersion {
            accountA_.version, accountB_.version, pendingTransfers_.version
        };
        if (version) {
            if (versionCmp_(myVersion, *version) || versionCmp_(*version, myVersion)) {
                return basic::transaction::RequestDecision::FailurePrecondition;
            }
        }

        basic::CBOR<uint32_t> newA {
            accountA_.data ? accountA_.data->value+injectData.accountAIncrement : injectData.accountAIncrement
        };
        basic::CBOR<uint32_t> newB {
            accountB_.data ? accountB_.data->value+injectData.accountBIncrement : injectData.accountBIncrement
        };

        etcdserverpb::TxnRequest txn;
        auto *cmp = txn.add_compare();
        cmp->set_result(etcdserverpb::Compare::EQUAL);
        cmp->set_target(etcdserverpb::Compare::VERSION);
        cmp->set_key(ACCOUNT_A_KEY);
        cmp->set_version(accountA_.version);
        cmp = txn.add_compare();
        cmp->set_result(etcdserverpb::Compare::EQUAL);
        cmp->set_target(etcdserverpb::Compare::VERSION);
        cmp->set_key(ACCOUNT_B_KEY);
        cmp->set_version(accountB_.version);
        cmp = txn.add_compare();
        cmp->set_result(etcdserverpb::Compare::EQUAL);
        cmp->set_target(etcdserverpb::Compare::VERSION);
        cmp->set_key(PENDING_TRANSFERS_KEY);
        cmp->set_version(pendingTransfers_.version);
        auto *action = txn.add_success();
        auto *put = action->mutable_request_put();
        put->set_key(ACCOUNT_A_KEY);
        put->set_value(basic::bytedata_utils::RunSerializer<basic::CBOR<uint32_t>>::apply(newA));
        action = txn.add_success();
        put = action->mutable_request_put();
        put->set_key(ACCOUNT_B_KEY);
        put->set_value(basic::bytedata_utils::RunSerializer<basic::CBOR<uint32_t>>::apply(newB));
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
    void handleInject(transport::BoostUUIDComponent::IDType id, TransactionDataVersion const &oldVersion, InjectData const &injectData, bool ignoreChecks, Callback *cb) {
        if (ignoreChecks) {
            basic::transaction::RequestDecision res;
            while ((res = tryInject(std::nullopt, injectData, true)) != basic::transaction::RequestDecision::Success) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            cb->onRequestDecision(id, res);
        } else {
            cb->onRequestDecision(id, tryInject(oldVersion, injectData, false));
        }
    }
    void handleTransaction(transport::BoostUUIDComponent::IDType id, TransactionDataVersion const &oldVersion, TransactionDataDelta const &delta, bool ignoreChecks, Callback *cb) {
        std::visit([this,id,&oldVersion,ignoreChecks,cb](auto const &tr) {
            using T = std::decay_t<decltype(tr)>;
            if constexpr (std::is_same_v<TransferRequest, T>) {
                handleTransfer(id, oldVersion, std::get<1>(tr), ignoreChecks, cb);
            } else if constexpr (std::is_same_v<ProcessRequest, T>) {
                handleProcess(id, oldVersion, ignoreChecks, cb);
            } else if constexpr (std::is_same_v<InjectRequest, T>) {
                handleInject(id, oldVersion, std::get<1>(tr), ignoreChecks, cb);
            }
        }, delta);
    }
    void runTransactionThread() {
        while (running_) {
            std::unique_lock<std::mutex> lock(transactionQueueMutex_);
            transactionQueueCond_.wait_for(lock, std::chrono::milliseconds(1));
            if (!running_) {
                lock.unlock();
                break;
            }
            if (transactionQueues_[transactionQueueIncomingIndex_].empty()) {
                lock.unlock();
                continue;
            }
            int processingIndex = transactionQueueIncomingIndex_;
            transactionQueueIncomingIndex_ = 1-transactionQueueIncomingIndex_;
            lock.unlock();

            {
                while (!transactionQueues_[processingIndex].empty()) {
                    auto const &t = transactionQueues_[processingIndex].front();
                    handleTransaction(
                        std::get<0>(t)
                        , std::get<1>(t)
                        , std::get<2>(t)
                        , std::get<3>(t)
                        , std::get<4>(t)
                    );
                    transactionQueues_[processingIndex].pop_front();
                }
            }
        }
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
        , transactionThread_(), watchThread_(), running_(false)
        , transactionQueueMutex_(), transactionQueueCond_() 
        , transactionQueues_(), transactionQueueIncomingIndex_(0)
        , versionCmp_()
        , watchListenerMutex_(), watchListeners_()
        , dataMutex_()
        , accountA_({0, std::nullopt}), accountB_({0, std::nullopt})
        , pendingTransfers_({0, std::nullopt}), revision_(0)
    {}
    THComponent(THComponent &&c) : 
        channel_(std::move(c.channel_))
        , connectionString_(std::move(c.connectionString_)), logger_(std::move(c.logger_))
        , transactionThread_(), watchThread_(), running_(false)
        , transactionQueueMutex_(), transactionQueueCond_()
        , transactionQueues_(), transactionQueueIncomingIndex_(0)
        , versionCmp_()
        , watchListenerMutex_(), watchListeners_()
        , dataMutex_()
        , accountA_({0, std::nullopt}), accountB_({0, std::nullopt})
        , pendingTransfers_({0, std::nullopt}), revision_(0)
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
        , transactionThread_(), watchThread_(), running_(false)
        , transactionQueueMutex_(), transactionQueueCond_()
        , transactionQueues_(), transactionQueueIncomingIndex_(0)
        , versionCmp_()
        , watchListenerMutex_(), watchListeners_()
        , dataMutex_()
        , accountA_({0, std::nullopt}), accountB_({0, std::nullopt})
        , pendingTransfers_({0, std::nullopt}), revision_(0)
    {}
    virtual ~THComponent() {
        if (running_) {
            running_ = false;
            transactionThread_.join();
            watchThread_.join();
        }
    }

    virtual void initialize() override final {
        channel_ = grpc::CreateChannel(connectionString_, grpc::InsecureChannelCredentials());     
        running_ = true;
        transactionThread_ = std::thread(&THComponent::runTransactionThread, this);
        watchThread_ = std::thread(&THComponent::runWatchThread, this);
        transactionThread_.detach();
        watchThread_.detach();
        logger_("TH component started");
    }
    virtual void
        handleInsert(
            transport::BoostUUIDComponent::IDType requestID
            , std::string const &account
            , TransactionKey const &key
            , TransactionData const &data
            , bool ignoreConsistencyCheckAsMuchAsPossible
            , Callback *callback
        ) override final
    {
        callback->onRequestDecision(requestID, basic::transaction::RequestDecision::FailurePermission);
    }
    virtual void
        handleUpdate(
            transport::BoostUUIDComponent::IDType requestID
            , std::string const &account
            , TransactionKey const &key
            , TransactionDataVersion const &oldVersion
            , TransactionDataSummary const &oldDataSummary
            , TransactionDataDelta const &dataDelta
            , bool ignoreConsistencyCheckAsMuchAsPossible
            , Callback *callback
        ) override final
    {
        if (running_) {
            {
                std::lock_guard<std::mutex> _(transactionQueueMutex_);
                transactionQueues_[transactionQueueIncomingIndex_].push_back({
                    requestID, oldVersion, dataDelta, ignoreConsistencyCheckAsMuchAsPossible, callback
                });
            }
            transactionQueueCond_.notify_one();
        }
    }
    virtual void
        handleDelete(
            transport::BoostUUIDComponent::IDType requestID
            , std::string const &account
            , TransactionKey const &key
            , TransactionDataVersion const &oldVersion
            , TransactionDataSummary const &oldDataSummary
            , bool ignoreConsistencyCheckAsMuchAsPossible
            , Callback *callback
        ) override final
    {
        callback->onRequestDecision(requestID, basic::transaction::RequestDecision::FailurePermission);
    }
    virtual void
        startWatch(
            std::string const &account
            , TransactionKey const &key
            , Callback *callback
        ) override final
    {
        std::lock_guard<std::mutex> _(watchListenerMutex_);
        watchListeners_.push_back(callback);
        innerSendState(callback);
    }
    virtual void 
        stopWatch(
            TransactionKey const &key
            , Callback *callback
        ) override final
    {
        std::lock_guard<std::mutex> _(watchListenerMutex_);
        watchListeners_.erase(std::remove(watchListeners_.begin(), watchListeners_.end(), callback), watchListeners_.end());
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
        , infra::ArrayComparerWithSkip<int64_t, 3>
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
    using TB = basic::transaction::SingleKeyAsyncWatchableStorageTransactionBroker<
        M
        , TransactionKey
        , TransactionData
        , TransactionDataVersion
        , TransactionDataSummary
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