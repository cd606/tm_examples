#include "TransactionServerComponents.hpp"

std::optional<DI::OneDeltaUpdateItem> DSComponent::createDeltaUpdate(mvccpb::Event::EventType eventType, mvccpb::KeyValue const &kv, int64_t revision) {
    VersionSlice versionSlice;
    DataSlice dataSlice;

    if (kv.key() == LOCK_QUEUE_KEY) {
        lockQueueRevision_->store(revision);
        lockQueueVersion_->store(kv.version());
        return std::nullopt;
    } else if (kv.key() == StorageConstants::OVERALL_STAT_KEY) {
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

void DSComponent::runWatchThread() {
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
                    int64_t revision = watchResponse.header().revision();
                    for (auto const &kv : initResponse.kvs()) {
                        auto delta = createDeltaUpdate(mvccpb::Event::PUT, kv, revision);
                        if (delta) {
                            updates.push_back({std::move(*delta)});
                        }
                    }

                    /*
                    std::ostringstream oss;
                    oss << "[DSComponent] Got init query callback, total "
                        << initResponse.kvs_size()
                        << " key-value pairs (revision: " << revision << ")";
                    logger_(oss.str());*/

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
                    int64_t revision = watchResponse.header().revision();
                    for (auto const &ev : watchResponse.events()) {
                        auto delta = createDeltaUpdate(ev.type(), ev.kv(), revision);
                        if (delta) {
                            updates.push_back({std::move(*delta)});
                        }
                    }

                    /*
                    std::ostringstream oss;
                    oss << "[DSComponent] Got watch  callback, total "
                        << watchResponse.events_size()
                        << " events (revision: " << revision << ")";
                    logger_(oss.str());*/

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

void DSComponent::initialize(Callback *cb) {
    watchListener_ = cb;

    //This is to make sure there is a data entry
    //if the database has never been populated
    watchListener_->onUpdate(DI::Update {
        0
        , std::vector<DI::OneUpdateItem> {
            { DI::OneFullUpdateItem {
                Key {}
                , Version {}
                , Data {}
            } }
        }
    });

    running_ = true;
    watchThread_ = std::thread(&DSComponent::runWatchThread, this);
    watchThread_.detach();

    logger_("DS component started");
}

#if 0
int64_t THComponent::acquireSimpleLock() {
    {
        auto leaseStub = etcdserverpb::Lease::NewStub(channel_);
        grpc::ClientContext leaseCtx;
        leaseCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
        etcdserverpb::LeaseGrantRequest getLease;
        getLease.set_ttl(5);
        getLease.set_id(0);
        etcdserverpb::LeaseGrantResponse leaseResponse;
        leaseStub->LeaseGrant(&leaseCtx, getLease, &leaseResponse);

        leaseID_ = leaseResponse.id();
    }

    //int64_t ret;

    {
        auto lockStub = v3lockpb::Lock::NewStub(channel_);
        grpc::ClientContext lockCtx;
        lockCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
        v3lockpb::LockRequest getLock;
        getLock.set_name("trtestlock");
        getLock.set_lease(leaseID_);
        v3lockpb::LockResponse lockResponse;
        lockStub->Lock(&lockCtx, getLock, &lockResponse);

        lockKey_ = lockResponse.key();
        //ret = lockResponse.header().revision();
    }
    /*
    std::ostringstream oss;
    oss << "[THComponent::acquireSimpleLock] Acquired lock '"
        << lockKey_ << "' with lease ID " << leaseID_ << ", ret version " << ret;
    logger_(oss.str());*/

    return 0;
}

int64_t THComponent::releaseSimpleLock() {
    {
        auto lockStub = v3lockpb::Lock::NewStub(channel_);
        grpc::ClientContext lockCtx;
        lockCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
        v3lockpb::UnlockRequest unlock;
        unlock.set_key(lockKey_);
        v3lockpb::UnlockResponse unlockResponse;
        lockStub->Unlock(&lockCtx, unlock, &unlockResponse);
    }

    //int64_t ret;

    {
        auto leaseStub = etcdserverpb::Lease::NewStub(channel_);
        grpc::ClientContext leaseCtx;
        leaseCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
        etcdserverpb::LeaseRevokeRequest revokeLease;
        revokeLease.set_id(leaseID_);
        etcdserverpb::LeaseRevokeResponse revokeLeaseResponse;
        leaseStub->LeaseRevoke(&leaseCtx, revokeLease, &revokeLeaseResponse);

        //ret = revokeLeaseResponse.header().revision();
    }
    /*
    std::ostringstream oss;
    oss << "[THComponent::releaseSimpleLock] Released lock '"
        << lockKey_ << "' and lease ID " << leaseID_ << ", ret version " << ret;
    logger_(oss.str());*/

    lockKey_ = "";
    leaseID_ = 0;

    return 0;
}
#endif

int64_t THComponent::acquireCompundLock() {
    int64_t numVersion = 0;
    int64_t numRevision = 0;
    do {
        etcdserverpb::TxnRequest txn;
        auto *action = txn.add_success();
        auto *put = action->mutable_request_put();
        put->set_key(LOCK_NUMBER_KEY);
        action = txn.add_success();
        auto *get = action->mutable_request_range();
        get->set_key(LOCK_NUMBER_KEY);

        etcdserverpb::TxnResponse txnResp;
        grpc::ClientContext txnCtx;
        txnCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
        stub_->Txn(&txnCtx, txn, &txnResp);

        if (txnResp.succeeded() && txnResp.responses_size() == 2) {
            numVersion = txnResp.responses(1).response_range().kvs(0).version();
            numRevision = txnResp.header().revision();
        }
    } while (numVersion == 0);
    --numVersion;
    while (lockQueueVersion_->load() != numVersion) {
    }
    return std::max(lockQueueRevision_->load(), numRevision);
}

int64_t THComponent::releaseCompoundLock() {
    etcdserverpb::PutRequest p;
    p.set_key(LOCK_QUEUE_KEY);
    etcdserverpb::PutResponse putResp;
    grpc::ClientContext putCtx;
    putCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
    stub_->Put(&putCtx, p, &putResp);

    return putResp.header().revision();
}

TI::GlobalVersion THComponent::acquireLock(std::string const &account, TI::Key const &, TI::DataDelta const *) {
    switch (lockChoice_) {
    case LockChoice::None:
        return 0;
    case LockChoice::Simple:
        //return acquireSimpleLock();
        return 0;
    case LockChoice::Compound:
        return acquireCompundLock();
    default:
        return 0;
    }
    
}

TI::GlobalVersion THComponent::releaseLock(std::string const &account, TI::Key const &, TI::DataDelta const *) {
    switch (lockChoice_) {
    case LockChoice::None:
        return 0;
    case LockChoice::Simple:
        //return releaseSimpleLock();
        return 0;
    case LockChoice::Compound:
        return releaseCompoundLock();
    default:
        return 0;
    }
}

TI::TransactionResponse THComponent::handleInsert(std::string const &account, TI::Key const &key, TI::Data const &data) {
    //Since the whole database is represented as a list
    //, there will never be insert or delete on the "key" (void struct)
    return {0, basic::transaction::v2::RequestDecision::FailureConsistency};
}

TI::TransactionResponse THComponent::handleUpdate(std::string const &account, TI::Key const &key, std::optional<TI::VersionSlice> const &updateVersionSlice, TI::ProcessedUpdate const &dataDelta) {
    //auto t1 = std::chrono::steady_clock::now();
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

    grpc::ClientContext txnCtx;
    txnCtx.set_deadline(std::chrono::system_clock::now()+std::chrono::hours(24));
    stub_->Txn(&txnCtx, txn, &resp);

    if (resp.succeeded()) {
        /*
        auto t2 = std::chrono::steady_clock::now();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(t2-t1).count();
        logger_(std::string("Transaction took ")+std::to_string(micros)+" micros");
        */
        return {resp.header().revision(), basic::transaction::v2::RequestDecision::Success};
    } else {
        return {resp.header().revision(), basic::transaction::v2::RequestDecision::FailurePrecondition};
    }
}

TI::TransactionResponse THComponent::handleDelete(std::string const &account, TI::Key const &key, std::optional<TI::Version> const &versionToDelete) {
    //Since the whole database is represented as a list
    //, there will never be insert or delete on the "key" (void struct)
    return {0, basic::transaction::v2::RequestDecision::FailureConsistency};
}
