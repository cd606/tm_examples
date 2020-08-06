#ifndef LOCAL_TRANSACTION_SERVER_COMPONENTS_HPP_
#define LOCAL_TRANSACTION_SERVER_COMPONENTS_HPP_

#include "TransactionInterface.hpp"

using namespace dev::cd606::tm;
using namespace test;

class LocalDSComponent : public basic::transaction::current::DataStreamEnvComponent<DI> {
private:
    Callback *watchListener_;
    std::function<void(std::string)> logger_;
public:
    LocalDSComponent()
        : watchListener_(nullptr), logger_()
    {}
    LocalDSComponent(std::function<void(std::string)> const &logger) 
        : watchListener_(nullptr), logger_(logger)
    {}
    LocalDSComponent &operator=(LocalDSComponent &&d)  {
        if (this != &d) {
            watchListener_ = std::move(d.watchListener_);
            logger_ = std::move(d.logger_);
        }
        return *this;
    }
    virtual ~LocalDSComponent() {
    }
    void initialize(Callback *cb);
    void pushUpdate(DI::Update &&update);
};

class LocalTHComponent : public basic::transaction::current::TransactionEnvComponent<TI> {
private:
    LocalDSComponent *ds_;
    std::function<void(std::string)> logger_;
    int64_t counter_;
public:
    LocalTHComponent()
        : ds_(nullptr), logger_(), counter_(0)
    {}
    LocalTHComponent &operator=(LocalTHComponent &&c) {
        if (this != &c) {
            ds_ = std::move(c.ds_);
            logger_ = std::move(c.logger_);
            counter_ = std::move(c.counter_);
        }
        return *this;
    } 
    LocalTHComponent(LocalDSComponent *ds, std::function<void(std::string)> const &logger) 
        : ds_(ds), logger_(logger), counter_(0)
    {}
    virtual ~LocalTHComponent() {
    }
    TI::GlobalVersion acquireLock(std::string const &account, TI::Key const &, TI::DataDelta const *) override final {
        return 0;
    }
    TI::GlobalVersion releaseLock(std::string const &account, TI::Key const &, TI::DataDelta const *) override final {
        return 0;
    }
    TI::TransactionResponse handleInsert(std::string const &account, TI::Key const &key, TI::Data const &data) override final {
        return {0, basic::transaction::v2::RequestDecision::FailureConsistency};
    }
    TI::TransactionResponse handleUpdate(std::string const &account, TI::Key const &key, std::optional<TI::VersionSlice> const &updateVersionSlice, TI::ProcessedUpdate const &dataDelta) override final;
    TI::TransactionResponse handleDelete(std::string const &account, TI::Key const &key, std::optional<TI::Version> const &versionToDelete) override final {
        return {0, basic::transaction::v2::RequestDecision::FailureConsistency};
    }
};

#endif