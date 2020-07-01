#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeMonad.hpp>

#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/transaction/TransactionServer.hpp>

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

#include <boost/algorithm/string.hpp>

#include "TransactionInterface.hpp"

using namespace dev::cd606::tm;
using namespace test;

using DI = basic::transaction::current::DataStreamInterface<
    int64_t
    , Key
    , Version
    , Data
    , VersionSlice
    , DataSlice
    , std::less<int64_t>
    , CompareVersion
>;

using TI = basic::transaction::current::TransactionInterface<
    int64_t
    , Key
    , Version
    , Data
    , DataSummary
    , VersionSlice
    , Command
    , DataSlice
>;

using GS = basic::transaction::current::GeneralSubscriberTypes<
    boost::uuids::uuid, DI
>;

struct StorageConstants {
    inline static const std::string OVERALL_STAT_KEY = "trtest:overall_stat";
    inline static const std::string PENDING_TRANSFERS_KEY = "trtest:pending_transfers";
    inline static const std::string ACCOUNT_KEY_PREFIX = "trtest:accounts:";
};

class DSComponent : public basic::transaction::current::DataStreamEnvComponent<DI> {
private:
    std::shared_ptr<grpc::ChannelInterface> channel_;
    std::function<void(std::string)> logger_;
    std::thread watchThread_;
    std::atomic<bool> running_;

    Callback *watchListener_;

    inline static const std::string WATCH_RANGE_START = "trtest:";
    inline static const std::string WATCH_RANGE_END = "trtest;"; //semicolon follows colon in ASCII table  

    std::optional<DI::OneDeltaUpdateItem> createDeltaUpdate(mvccpb::Event::EventType eventType, mvccpb::KeyValue const &kv) {
        VersionSlice versionSlice;
        DataSlice dataSlice;

        if (kv.key() == StorageConstants::OVERALL_STAT_KEY) {
            if (eventType == mvccpb::Event::PUT) {
                versionSlice.overallStat = kv.version();
                auto s = basic::bytedata_utils::RunDeserializer<
                    basic::CBOR<OverallStat>
                >::apply(kv.value());
                if (s) {
                    dataSlice.overallStat = s->value;
                } else {
                    dataSlice.overallStat = OverallStat {0};
                }
            } else {
                versionSlice.overallStat = kv.version();
                dataSlice.overallStat = OverallStat {0};
            }
            return DI::OneDeltaUpdateItem {
                Key {}
                , std::move(versionSlice)
                , std::move(dataSlice)
            };
        } else if (kv.key() == StorageConstants::PENDING_TRANSFERS_KEY) {
            if (eventType == mvccpb::Event::PUT) {
                versionSlice.pendingTransfers = kv.version();
                auto s = basic::bytedata_utils::RunDeserializer<
                    basic::CBOR<TransferList>
                >::apply(kv.value());
                if (s) {
                    dataSlice.pendingTransfers = s->value;
                } else {
                    dataSlice.pendingTransfers = TransferList {};
                }
            } else {
                versionSlice.pendingTransfers = kv.version();
                dataSlice.pendingTransfers = TransferList {};
            }
            return DI::OneDeltaUpdateItem {
                Key {}
                , std::move(versionSlice)
                , std::move(dataSlice)
            };
        } else if (boost::starts_with(kv.key(), StorageConstants::ACCOUNT_KEY_PREFIX)) {
            std::string accountName = kv.key().substr(StorageConstants::ACCOUNT_KEY_PREFIX.length());
            if (eventType == mvccpb::Event::PUT) {
                versionSlice.accounts[accountName] = kv.version();
                auto s = basic::bytedata_utils::RunDeserializer<
                    basic::CBOR<AccountData>
                >::apply(kv.value());
                if (s) {
                    dataSlice.accounts[accountName] = s->value;
                } else {
                    dataSlice.accounts[accountName] = std::nullopt;
                }
            } else {
                versionSlice.accounts[accountName] = kv.version();
                dataSlice.accounts[accountName] = std::nullopt;
            }
            return DI::OneDeltaUpdateItem {
                Key {}
                , std::move(versionSlice)
                , std::move(dataSlice)
            };
        } else {
            return std::nullopt;
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
                        std::vector<DI::OneUpdateItem> updates;
                        for (auto const &kv : initResponse.kvs()) {
                            auto delta = createDeltaUpdate(mvccpb::Event::PUT, kv);
                            if (delta) {
                                updates.push_back(*delta);
                            }
                        }
                        int64_t revision = watchResponse.header().revision();

                        std::ostringstream oss;
                        oss << "[DSComponent] Got init query callback, total "
                            << initResponse.kvs_size()
                            << " key-value pairs (revision: " << revision << ")";
                        logger_(oss.str());

                        watchListener_->onUpdate(DI::Update {
                            revision
                            , std::move(updates)
                        });
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
                        std::vector<DI::OneUpdateItem> updates;
                        for (auto const &ev : watchResponse.events()) {
                            auto delta = createDeltaUpdate(ev.type(), ev.kv());
                            if (delta) {
                                updates.push_back(*delta);
                            }
                        }
                        int64_t revision = watchResponse.header().revision();

                        std::ostringstream oss;
                        oss << "[DSComponent] Got watch  callback, total "
                            << watchResponse.events_size()
                            << " events (revision: " << revision << ")";
                        logger_(oss.str());

                        watchListener_->onUpdate(DI::Update {
                            revision
                            , std::move(updates)
                        });
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
    DSComponent()
        : channel_(), logger_(), watchThread_(), running_(false)
        , watchListener_(nullptr)
    {}
    DSComponent &operator=(DSComponent &&c) {
        if (this != &c) {
            //only copy these!
            channel_ = std::move(c.channel_);
            logger_ = std::move(c.logger_);
        }
        return *this;
    } 
    DSComponent(std::shared_ptr<grpc::ChannelInterface> const &channel, std::function<void(std::string)> const &logger) 
        : channel_(channel), logger_(logger)
        , watchThread_(), running_(false)
        , watchListener_(nullptr)
    {}
    virtual ~DSComponent() {
        if (running_) {
            running_ = false;
            watchThread_.join();
        }
    }
    void initialize(Callback *cb) {
        watchListener_ = cb;

        //This is to make sure there is a data entry
        //if the database has never been populated
        watchListener_->onUpdate(DI::Update {
            0
            , std::vector<DI::OneUpdateItem> {
                DI::OneFullUpdateItem {
                    Key {}
                    , Version {}
                    , Data {}
                }
            }
        });

        running_ = true;
        watchThread_ = std::thread(&DSComponent::runWatchThread, this);
        watchThread_.detach();

        logger_("DS component started");
    }
};

class THComponent : public basic::transaction::current::TransactionEnvComponent<TI> {
private:
    std::shared_ptr<grpc::ChannelInterface> channel_;
    std::function<void(std::string)> logger_;

    inline static const std::string LOCK_NUMBER_KEY = "trtest:lock_number";
    inline static const std::string LOCK_QUEUE_KEY = "trtest:lock_key";
public:
    THComponent()
        : channel_(), logger_()
    {}
    THComponent &operator=(THComponent &&c) {
        if (this != &c) {
            //only copy these!
            channel_ = std::move(c.channel_);
            logger_ = std::move(c.logger_);
        }
        return *this;
    } 
    THComponent(std::shared_ptr<grpc::ChannelInterface> const &channel, std::function<void(std::string)> const &logger) 
        : channel_(channel), logger_(logger)
    {}
    virtual ~THComponent() {
    }
    TI::GlobalVersion acquireLock(std::string const &account, TI::Key const &name) override final {
        int64_t numVersion = 0;

        auto kvStub = etcdserverpb::KV::NewStub(channel_);

        while (true) {
            //This is a spinlock, we hope we can obtain the lock number quickly
            numVersion = 0;

            etcdserverpb::RangeRequest getNum;
            getNum.set_key(LOCK_NUMBER_KEY);

            etcdserverpb::RangeResponse numResponse;
            grpc::ClientContext kvCtx;
            kvCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
            kvStub->Range(&kvCtx, getNum, &numResponse);
            if (numResponse.kvs_size() > 0) {
                numVersion = numResponse.kvs(0).version();
            }
            
            etcdserverpb::TxnRequest txn;
            auto *cmp = txn.add_compare();
            cmp->set_result(etcdserverpb::Compare::EQUAL);
            cmp->set_target(etcdserverpb::Compare::VERSION);
            cmp->set_key(LOCK_NUMBER_KEY);
            cmp->set_version(numVersion);
            auto *action = txn.add_success();
            auto *put = action->mutable_request_put();
            put->set_key(LOCK_NUMBER_KEY);

            etcdserverpb::TxnResponse putResp;
            grpc::ClientContext txnCtx;
            txnCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
            kvStub->Txn(&txnCtx, txn, &putResp);

            if (putResp.succeeded()) {
                std::ostringstream oss;
                oss << "Acquiring lock with number " << numVersion;
                logger_(oss.str());
                break;
            }
        }

        int64_t ret = 0;

        while (true) {
            int64_t queueVersion = 0;

            etcdserverpb::RangeRequest getQueue;
            getQueue.set_key(LOCK_QUEUE_KEY);

            etcdserverpb::RangeResponse queueResponse;
            grpc::ClientContext kvCtx;
            kvCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
            kvStub->Range(&kvCtx, getQueue, &queueResponse);
            if (queueResponse.kvs_size() > 0) {
                queueVersion = queueResponse.kvs(0).version();
            }

            if (queueVersion == numVersion) {
                ret = queueResponse.header().revision();
                break;
            }

            //here since someone else is holding the lock
            //, we will sleep somewhat
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        std::ostringstream oss;
        oss << "Lock acquired with number " << numVersion;
        logger_(oss.str());

        return ret;
    }
    TI::GlobalVersion releaseLock(std::string const &account, TI::Key const &name) override final {
        int64_t queueVersion = 0;

        auto kvStub = etcdserverpb::KV::NewStub(channel_);
        
        int64_t ret = 0;

        while (true) {
            //This is a spinlock, we hope we can increase the queue number quickly
            queueVersion = 0;

            etcdserverpb::RangeRequest getQueue;
            getQueue.set_key(LOCK_QUEUE_KEY);

            etcdserverpb::RangeResponse queueResponse;
            grpc::ClientContext kvCtx;
            kvCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
            kvStub->Range(&kvCtx, getQueue, &queueResponse);
            if (queueResponse.kvs_size() > 0) {
                queueVersion = queueResponse.kvs(0).version();
            }
            
            etcdserverpb::TxnRequest txn;
            auto *cmp = txn.add_compare();
            cmp->set_result(etcdserverpb::Compare::EQUAL);
            cmp->set_target(etcdserverpb::Compare::VERSION);
            cmp->set_key(LOCK_QUEUE_KEY);
            cmp->set_version(queueVersion);
            auto *action = txn.add_success();
            auto *put = action->mutable_request_put();
            put->set_key(LOCK_QUEUE_KEY);
        
            etcdserverpb::TxnResponse putResp;
            grpc::ClientContext txnCtx;
            txnCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
            kvStub->Txn(&txnCtx, txn, &putResp);

            if (putResp.succeeded()) {
                ret = putResp.header().revision();
                break;
            }
        }

        std::ostringstream oss;
        oss << "Released lock";
        logger_(oss.str());

        return ret;
    }
    TI::TransactionResponse handleInsert(std::string const &account, TI::Key const &key, TI::Data const &data) override final {
        //Since the whole database is represented as a list
        //, there will never be insert or delete on the "key" (void struct)
        return {0, basic::transaction::v2::RequestDecision::FailureConsistency};
    }
    TI::TransactionResponse handleUpdate(std::string const &account, TI::Key const &key, std::optional<TI::VersionSlice> const &updateVersionSlice, TI::ProcessedUpdate const &dataDelta) override final {
        etcdserverpb::TxnRequest txn;
        if (updateVersionSlice) {
            if (updateVersionSlice->overallStat) {
                auto *cmp = txn.add_compare();
                cmp->set_result(etcdserverpb::Compare::EQUAL);
                cmp->set_target(etcdserverpb::Compare::VERSION);
                cmp->set_key(StorageConstants::OVERALL_STAT_KEY);
                cmp->set_version(*(updateVersionSlice->overallStat));
            }
            for (auto const &item : updateVersionSlice->accounts) {
                if (item.second) {
                    auto *cmp = txn.add_compare();
                    cmp->set_result(etcdserverpb::Compare::EQUAL);
                    cmp->set_target(etcdserverpb::Compare::VERSION);
                    cmp->set_key(StorageConstants::ACCOUNT_KEY_PREFIX+item.first);
                    cmp->set_version(*(item.second));
                }
            }
            if (updateVersionSlice->pendingTransfers) {
                auto *cmp = txn.add_compare();
                cmp->set_result(etcdserverpb::Compare::EQUAL);
                cmp->set_target(etcdserverpb::Compare::VERSION);
                cmp->set_key(StorageConstants::PENDING_TRANSFERS_KEY);
                cmp->set_version(*(updateVersionSlice->pendingTransfers));
            }
        }
        if (dataDelta.overallStat) {
            auto *action = txn.add_success();
            auto *put = action->mutable_request_put();
            put->set_key(StorageConstants::OVERALL_STAT_KEY);
            put->set_value(basic::bytedata_utils::RunSerializer<basic::CBOR<OverallStat>>::apply({*(dataDelta.overallStat)}));
        }
        for (auto const &item : dataDelta.accounts) {
            auto *action = txn.add_success();
            if (item.second) {
                auto *put = action->mutable_request_put();
                put->set_key(StorageConstants::ACCOUNT_KEY_PREFIX+item.first);
                put->set_value(basic::bytedata_utils::RunSerializer<basic::CBOR<AccountData>>::apply({*(item.second)}));
            } else {
                auto *del = action->mutable_request_delete_range();
                del->set_key(StorageConstants::ACCOUNT_KEY_PREFIX+item.first);
            }
        }
        if (dataDelta.pendingTransfers) {
            auto *action = txn.add_success();
            auto *put = action->mutable_request_put();
            put->set_key(StorageConstants::PENDING_TRANSFERS_KEY);
            put->set_value(basic::bytedata_utils::RunSerializer<basic::CBOR<TransferList>>::apply({*(dataDelta.pendingTransfers)}));
        }

        etcdserverpb::TxnResponse resp;

        auto txnStub = etcdserverpb::KV::NewStub(channel_);
        grpc::ClientContext txnCtx;
        txnCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
        txnStub->Txn(&txnCtx, txn, &resp);

        if (resp.succeeded()) {
            return {resp.header().revision(), basic::transaction::v2::RequestDecision::Success};
        } else {
            return {resp.header().revision(), basic::transaction::v2::RequestDecision::FailurePrecondition};
        }
    }
    TI::TransactionResponse handleDelete(std::string const &account, TI::Key const &key, std::optional<TI::Version> const &versionToDelete) override final {
        //Since the whole database is represented as a list
        //, there will never be insert or delete on the "key" (void struct)
        return {0, basic::transaction::v2::RequestDecision::FailureConsistency};
    }
};

int main(int argc, char **argv) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::real_time_clock::ClockComponent>,
        transport::BoostUUIDComponent,
        transport::ServerSideSimpleIdentityCheckerComponent<
            std::string
            , TI::Transaction>,
        transport::ServerSideSimpleIdentityCheckerComponent<
            std::string
            , GS::Input>,
        transport::redis::RedisComponent,
        transport::HeartbeatAndAlertComponent,
        DSComponent,
        THComponent
    >;
    using M = infra::RealTimeMonad<TheEnvironment>;
    using R = infra::MonadRunner<M>;

    TheEnvironment env;

    auto channel = grpc::CreateChannel("127.0.0.1:2379", grpc::InsecureChannelCredentials());
    env.DSComponent::operator=(DSComponent {
        channel, [&env](std::string const &s) {
            env.log(infra::LogLevel::Info, s);
        }
    });
    env.THComponent::operator=(THComponent {
        channel, [&env](std::string const &s) {
            env.log(infra::LogLevel::Info, s);
        }
    });
    
    transport::HeartbeatAndAlertComponentInitializer<TheEnvironment,transport::redis::RedisComponent>()
        (&env, "transaction redundancy test server", transport::ConnectionLocator::parse("127.0.0.1:6379"));

    R r(&env);

    using TF = basic::transaction::v2::TransactionFacility<
        M, TI, DI
        , CheckVersion
        , CheckVersionSlice
        , CheckSummary
        , ProcessCommandOnLocalData
    >;
    auto dataStore = std::make_shared<TF::DataStore>();
    using DM = basic::transaction::current::TransactionDeltaMerger<
        DI, false, M::PossiblyMultiThreaded
        , ApplyVersionSlice
        , ApplyDataSlice
    >;
    TF tf(dataStore);
    tf.setDeltaProcessor(ProcessCommandOnLocalData([&env](std::string const &s) {
        env.log(infra::LogLevel::Warning, s);
    }));
    auto transactionLogicCombinationRes = basic::transaction::v2::transactionLogicCombination<
        R, TI, DI, DM
    >(
        r
        , "transaction_server_components"
        , &tf
    );

    transport::redis::RedisOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::wrapOnOrderFacilityWithExternalEffects
        <TI::Transaction,TI::TransactionResponse,DI::Update>(
        r
        , transactionLogicCombinationRes.transactionFacility
        , transport::ConnectionLocator::parse("127.0.0.1:6379:::test_etcd_transaction_queue")
        , "transaction_wrapper_"
        , std::nullopt //no hook
    );
    transport::redis::RedisOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::wrapLocalOnOrderFacility
        <GS::Input,GS::Output,GS::SubscriptionUpdate>(
        r
        , transactionLogicCombinationRes.subscriptionFacility
        , transport::ConnectionLocator::parse("127.0.0.1:6379:::test_etcd_subscription_queue")
        , "subscription_wrapper_"
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