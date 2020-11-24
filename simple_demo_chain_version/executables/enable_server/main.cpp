#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/BasicWithTimeApp.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/utilities/transaction.h>
#include <rocksdb/utilities/transaction_db.h>

#ifdef _MSC_VER
#define ROCKSDB_NAMESPACE rocksdb
#endif

#include <boost/program_options.hpp>

#include <iostream>

#include "simple_demo_chain_version/enable_server_data/EnableServerTransactionData.hpp"

using namespace dev::cd606::tm;
using namespace simple_demo_chain_version::enable_server;

#define StorageFields \
    ((int64_t, version)) \
    ((bool, enabled))

TM_BASIC_CBOR_CAPABLE_STRUCT(Storage, StorageFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(Storage, StorageFields);

class DSComponent : public basic::transaction::v2::DataStreamEnvComponent<DI> {
private:
    ROCKSDB_NAMESPACE::TransactionDB *db_;
    std::function<void(std::string)> logger_;
    Callback *cb_;
public:
    DSComponent() : db_(nullptr), logger_() {
    }
    DSComponent(ROCKSDB_NAMESPACE::TransactionDB *db, std::function<void(std::string)> const &logger) : db_(db), logger_(logger) {
    }
    DSComponent(DSComponent &&c) : db_(c.db_), logger_(std::move(c.logger_)) {}
    DSComponent &operator=(DSComponent &&c) {
        if (this != &c) {
            db_ = c.db_;
            logger_ = std::move(c.logger_);
        }
        return *this;
    }
    virtual ~DSComponent() {}
    void initialize(Callback *cb) {
        cb_ = cb;
        Storage initialData = {0, true};
        std::string val;
        auto txn = db_->BeginTransaction(ROCKSDB_NAMESPACE::WriteOptions());
        auto status = txn->GetForUpdate(ROCKSDB_NAMESPACE::ReadOptions(), "enabled", &val);
        if (status.IsNotFound()) {
            val = basic::bytedata_utils::RunSerializer<Storage>::apply(initialData);
            txn->Put("enabled", val);
        } else {
            auto parsed = basic::bytedata_utils::RunDeserializer<Storage>::apply(val);
            if (!parsed) {
                val = basic::bytedata_utils::RunSerializer<Storage>::apply(initialData);
                txn->Put("enabled", val);
            } else {
                initialData = *parsed;
            }
        }
        txn->Commit();
        std::ostringstream oss;
        oss << "[DSComponent] loaded {version=" << initialData.version << ", enabled=" << initialData.enabled << "}";
        logger_(oss.str());
        cb_->onUpdate(DI::Update {
            initialData.version
            , std::vector<DI::OneUpdateItem> {
                DI::OneFullUpdateItem {
                    Key {}
                    , initialData.version
                    , initialData.enabled
                }
            }
        });
    }
    Callback *callback() const {
        return cb_;
    }
};

class THComponent : public basic::transaction::v2::TransactionEnvComponent<TI> {
private:
    ROCKSDB_NAMESPACE::TransactionDB *db_;
    std::function<void(std::string)> logger_;
    DSComponent *dsComponent_;
    std::mutex mutex_;
    ROCKSDB_NAMESPACE::Transaction *txn_;
    int64_t globalVersion_;

    void triggerCallback(TI::TransactionResponse const &resp, TI::Key const &key, TI::DataDelta const &dataDelta) {
        dsComponent_->callback()->onUpdate(DI::Update {
            resp.value.globalVersion
            , std::vector<DI::OneUpdateItem> {
                DI::OneDeltaUpdateItem {
                    key
                    , resp.value.globalVersion
                    , dataDelta
                }
            } 
        });
    }
public:
    THComponent() : db_(nullptr), logger_(), dsComponent_(nullptr), txn_(nullptr), globalVersion_(0) {
    }
    THComponent(ROCKSDB_NAMESPACE::TransactionDB *db, std::function<void(std::string)> const &logger, DSComponent *dsComponent) : db_(db), logger_(logger), dsComponent_(dsComponent), txn_(nullptr), globalVersion_(0) {
    }
    THComponent(THComponent &&c) : db_(c.db_), logger_(std::move(c.logger_)), dsComponent_(c.dsComponent_), txn_(nullptr), globalVersion_(c.globalVersion_) {}
    THComponent &operator=(THComponent &&c) {
        if (this != &c) {
            db_ = c.db_;
            dsComponent_ = c.dsComponent_;
            globalVersion_ = c.globalVersion_;
        }
        return *this;
    }
    virtual ~THComponent() {
    }
    TI::GlobalVersion acquireLock(std::string const &account, TI::Key const &, TI::DataDelta const *) override final {
        mutex_.lock();
        txn_ = db_->BeginTransaction(ROCKSDB_NAMESPACE::WriteOptions());
        std::string val;
        auto s = txn_->GetForUpdate(ROCKSDB_NAMESPACE::ReadOptions(), "enabled", &val);
        if (s.IsNotFound()) {
            globalVersion_ = 0;
        } else {
            auto data = basic::bytedata_utils::RunDeserializer<Storage>::apply(val);
            if (!data) {
                globalVersion_ = 0;
            } else {
                globalVersion_ = data->version;
            }
        }
        return globalVersion_;
    }
    TI::GlobalVersion releaseLock(std::string const &account, TI::Key const &, TI::DataDelta const *) override final {
        txn_->Commit();
        mutex_.unlock();
        return globalVersion_;
    }
    TI::TransactionResponse handleInsert(std::string const &account, TI::Key const &key, TI::Data const &data) override final {
        return {globalVersion_, basic::transaction::v2::RequestDecision::FailureConsistency};
    }
    //Please notice that in this setup, TI::ProcessedUpdate is simply TI::DataDelta
    TI::TransactionResponse handleUpdate(std::string const &account, TI::Key const &key, std::optional<TI::VersionSlice> const &updateVersionSlice, TI::ProcessedUpdate const &dataDelta) override final {
        Storage data {
            std::max((updateVersionSlice?*updateVersionSlice:0), globalVersion_)+1
            , dataDelta
        };
        txn_->Put("enabled", basic::bytedata_utils::RunSerializer<Storage>::apply(data));
        globalVersion_ = data.version;
        TI::TransactionResponse resp {data.version, basic::transaction::v2::RequestDecision::Success};
        triggerCallback(resp, key, dataDelta);
        return resp;
    }
    TI::TransactionResponse handleDelete(std::string const &account, TI::Key const &key, std::optional<TI::Version> const &versionToDelete) override final {
        return {globalVersion_, basic::transaction::v2::RequestDecision::FailureConsistency};
    }
};

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("db_path", po::value<std::string>(), "database path")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    if (!vm.count("db_path")) {
        std::cerr << "Please provide database path\n";
        return 1;
    }

    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::CrossGuidComponent,
        transport::ServerSideSimpleIdentityCheckerComponent<
            std::string
            , TI::Transaction>,
        transport::ServerSideSimpleIdentityCheckerComponent<
            std::string
            , GS::Input>,
        transport::rabbitmq::RabbitMQComponent,
        transport::HeartbeatAndAlertComponent,
        DSComponent,
        THComponent
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;

    ROCKSDB_NAMESPACE::Options options;
    ROCKSDB_NAMESPACE::TransactionDBOptions txn_options;
    options.IncreaseParallelism();
    options.OptimizeLevelStyleCompaction();
    options.create_if_missing = true;
    ROCKSDB_NAMESPACE::TransactionDB *db;
    auto status = ROCKSDB_NAMESPACE::TransactionDB::Open(
        options, txn_options, vm["db_path"].as<std::string>(), &db
    );
    if (!status.ok()) {
        std::cerr << "Can't open RocksDB file " << vm["db_path"].as<std::string>() << "\n";
        return 1;
    }

    env.DSComponent::operator=(DSComponent {
        db
        , [&env](std::string const &s) {
            env.log(infra::LogLevel::Info, s);
        }
    });
    env.THComponent::operator=(THComponent {
        db
        , [&env](std::string const &s) {
            env.log(infra::LogLevel::Info, s);
        }
        , static_cast<DSComponent *>(&env)
    });

    R r(&env);

    env.setLogFilePrefix("simple_demo_chain_version_enable_server_");
    transport::initializeHeartbeatAndAlertComponent
        (&env, "simple_demo_chain_version Enable Server", "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]");
    env.setStatus("program", transport::HeartbeatMessage::Status::Good);
    transport::attachHeartbeatAndAlertComponent(r, &env, "simple_demo_chain_version.enable_server.heartbeat", std::chrono::seconds(1));

    using TF = basic::transaction::v2::TransactionFacility<
        M, TI, DI, basic::transaction::v2::TransactionLoggingLevel::Verbose
        , std::equal_to<int64_t>
        , std::equal_to<int64_t>
        , CheckSummary
    >;
    auto dataStore = std::make_shared<TF::DataStore>();
    using DM = basic::transaction::v2::TransactionDeltaMerger<
        DI, false, M::PossiblyMultiThreaded
    >;
    auto transactionLogicCombinationRes = basic::transaction::v2::transactionLogicCombination<
        R, TI, DI, DM, basic::transaction::v2::SubscriptionLoggingLevel::Verbose
    >(
        r
        , "transaction_server_components"
        , new TF(dataStore)
    );

    transport::MultiTransportFacilityWrapper<R>::wrap
        <TI::Transaction,TI::TransactionResponse,DI::Update>(
        r
        , transactionLogicCombinationRes.transactionFacility
        , "rabbitmq://127.0.0.1::guest:guest:simple_demo_chain_version_enable_transaction_queue"
        , "transaction_wrapper/"
    );
    transport::MultiTransportFacilityWrapper<R>::wrap
        <GS::Input,GS::Output,GS::SubscriptionUpdate>(
        r
        , transactionLogicCombinationRes.subscriptionFacility
        , "rabbitmq://127.0.0.1::guest:guest:simple_demo_chain_version_enable_subscription_queue"
        , "subscription_wrapper/"
    );
    
    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "simple_demo_chain_version_enable_server");
    r.finalize();

    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "Simple demo chain version enable server started");

    infra::terminationController(infra::RunForever {&env});
}
