#ifndef TRANSACTION_SERVER_COMPONENTS_HPP_
#define TRANSACTION_SERVER_COMPONENTS_HPP_

#include <tm_kit/basic/transaction/TransactionServer.hpp>
#include <tm_kit/transport/BoostUUIDComponent.hpp>

#include "TransactionInterface.hpp"

#include <grpcpp/grpcpp.h>
#ifdef _MSC_VER
#undef DELETE
#endif
#include <libetcd/rpc.grpc.pb.h>
#include <libetcd/kv.pb.h>
#include "v3lock.pb.h"
#include "v3lock.grpc.pb.h"

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

#include <boost/algorithm/string.hpp>

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

    inline static const std::string LOCK_QUEUE_KEY = "trtest:lock_queue";
    std::atomic<int64_t> *lockQueueVersion_, *lockQueueRevision_;

    std::optional<DI::OneDeltaUpdateItem> createDeltaUpdate(mvccpb::Event::EventType eventType, mvccpb::KeyValue const &kv, int64_t revision);
    void runWatchThread();
public:
    DSComponent()
        : channel_(), logger_(), watchThread_(), running_(false)
        , watchListener_(nullptr)
        , lockQueueVersion_(nullptr), lockQueueRevision_(nullptr)
    {}
    DSComponent &operator=(DSComponent &&c) {
        if (this != &c) {
            //only copy these!
            channel_ = std::move(c.channel_);
            logger_ = std::move(c.logger_);
            lockQueueVersion_ = std::move(c.lockQueueVersion_);
            lockQueueRevision_ = std::move(c.lockQueueRevision_);
        }
        return *this;
    } 
    DSComponent(std::shared_ptr<grpc::ChannelInterface> const &channel, std::function<void(std::string)> const &logger, std::atomic<int64_t> *lockQueueVersion, std::atomic<int64_t> *lockQueueRevision) 
        : channel_(channel), logger_(logger)
        , watchThread_(), running_(false)
        , watchListener_(nullptr)
        , lockQueueVersion_(lockQueueVersion)
        , lockQueueRevision_(lockQueueRevision)
    {}
    virtual ~DSComponent() {
        if (running_) {
            running_ = false;
            watchThread_.join();
        }
    }
    void initialize(Callback *cb);
};

class THComponent : public basic::transaction::current::TransactionEnvComponent<TI> {
public:
    //"None" means no inter-process lock is applied
    //"Simple" means using etcd server-side lock. 
    // This lock guarantees low-level progress and 
    // is robust against stopping failure (because
    // it comes with a lease). Because this is not
    // supported by the offscale-libetcd-cpp library
    // , we had to generate the grpc bindings from
    // protobuf file.
    //"Compound" means using a two-integer lock that
    // we maintain by ourselves. This lock is lockout-free
    // , but is not robust against stopping failure. 
    // lock_helpers/LockHelper.ts is a tool to 
    // manage the lock from the outside.
    enum class LockChoice {
        None 
        , Simple 
        , Compound
    };
private:
    LockChoice lockChoice_;
    std::shared_ptr<grpc::ChannelInterface> channel_;
    std::function<void(std::string)> logger_;

    inline static const std::string LOCK_NUMBER_KEY = "trtest:lock_number";
    inline static const std::string LOCK_QUEUE_KEY = "trtest:lock_queue";
    std::atomic<int64_t> const *lockQueueVersion_;
    std::atomic<int64_t> const *lockQueueRevision_;

    std::unique_ptr<etcdserverpb::KV::Stub> stub_;

    int64_t leaseID_ = 0;
    std::string lockKey_ = "";

    int64_t acquireSimpleLock();
    int64_t releaseSimpleLock();
    int64_t acquireCompundLock();
    int64_t releaseCompoundLock();
public:
    THComponent()
        : lockChoice_(LockChoice::None), channel_(), logger_(), lockQueueVersion_(nullptr), lockQueueRevision_(nullptr), stub_()
    {}
    THComponent &operator=(THComponent &&c) {
        if (this != &c) {
            //only copy these!
            lockChoice_ = c.lockChoice_;
            channel_ = std::move(c.channel_);
            logger_ = std::move(c.logger_);
            lockQueueVersion_ = std::move(c.lockQueueVersion_);
            lockQueueRevision_ = std::move(c.lockQueueRevision_);
            stub_ = std::move(c.stub_);
        }
        return *this;
    } 
    THComponent(LockChoice lockChoice, std::shared_ptr<grpc::ChannelInterface> const &channel, std::function<void(std::string)> const &logger, std::atomic<int64_t> const *lockQueueVersion, std::atomic<int64_t> const *lockQueueRevision) 
        : lockChoice_(lockChoice), channel_(channel), logger_(logger), lockQueueVersion_(lockQueueVersion), lockQueueRevision_(lockQueueRevision), stub_(etcdserverpb::KV::NewStub(channel_))
    {}
    virtual ~THComponent() {
    }
    TI::GlobalVersion acquireLock(std::string const &account, TI::Key const &, TI::DataDelta const *) override final;
    TI::GlobalVersion releaseLock(std::string const &account, TI::Key const &, TI::DataDelta const *) override final;
    TI::TransactionResponse handleInsert(std::string const &account, TI::Key const &key, TI::Data const &data) override final;
    TI::TransactionResponse handleUpdate(std::string const &account, TI::Key const &key, std::optional<TI::VersionSlice> const &updateVersionSlice, TI::ProcessedUpdate const &dataDelta) override final;
    TI::TransactionResponse handleDelete(std::string const &account, TI::Key const &key, std::optional<TI::Version> const &versionToDelete) override final;
};

#endif