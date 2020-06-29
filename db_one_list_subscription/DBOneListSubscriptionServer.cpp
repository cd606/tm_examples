#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/BasicWithTimeMonad.hpp>
#include <tm_kit/infra/RealTimeMonad.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/transaction/v2/TransactionLogicCombination.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQOnOrderFacility.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>

#include "TransactionHelpers.hpp"

#include <soci/soci.h>
#include <soci/sqlite3/soci-sqlite3.h>

#include <boost/program_options.hpp>

#include <iostream>

using namespace dev::cd606::tm;
using namespace db_one_list_subscription;

using DI = basic::transaction::v2::DataStreamInterface<
    int64_t
    , Key
    , int64_t
    , Data
    , int64_t
    , DataDelta
>;

using TI = basic::transaction::v2::TransactionInterface<
    int64_t
    , Key
    , int64_t
    , Data
    , DataSummary
    , int64_t
    , DataDelta
>;

using GS = basic::transaction::v2::GeneralSubscriberTypes<
    boost::uuids::uuid, DI
>;

class DSComponent : public basic::transaction::v2::DataStreamEnvComponent<DI> {
private:
    std::shared_ptr<soci::session> session_;
    std::function<void(std::string)> logger_;
    Callback *cb_;
public:
    DSComponent() : session_(), logger_() {
    }
    DSComponent(std::shared_ptr<soci::session> const &session, std::function<void(std::string)> const &logger) : session_(session), logger_(logger) {
    }
    DSComponent(DSComponent &&c) : session_(std::move(c.session_)), logger_(std::move(c.logger_)) {}
    DSComponent &operator=(DSComponent &&c) {
        if (this != &c) {
            session_ = std::move(c.session_);
            logger_ = std::move(c.logger_);
        }
        return *this;
    }
    virtual ~DSComponent() {}
    void initialize(Callback *cb) {
        cb_ = cb;
        Data initialData;
        soci::rowset<soci::row> res = 
            session_->prepare << "SELECT name, amount, stat FROM test_table";
        for (auto const &r : res) {
            db_key key;
            key.name = r.get<std::string>(0);
            db_data data;
            data.amount = r.get<int>(1);
            data.stat = r.get<double>(2);
            initialData.insert({key, data});
        }
        std::ostringstream oss;
        oss << "[DSComponent] loaded " << initialData.size() << " rows";
        logger_(oss.str());
        cb_->onUpdate(DI::Update {
            0
            , std::vector<DI::OneUpdateItem> {
                DI::OneFullUpdateItem {
                    Key {}
                    , 0
                    , std::move(initialData)
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
    std::shared_ptr<soci::session> session_;
    std::function<void(std::string)> logger_;
    std::atomic<int64_t> globalVersion_;
    DSComponent *dsComponent_;

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
    THComponent() : session_(), logger_(), globalVersion_(0), dsComponent_(nullptr) {
    }
    THComponent(std::shared_ptr<soci::session> const &session, std::function<void(std::string)> const &logger, DSComponent *dsComponent) : session_(session), logger_(logger), globalVersion_(0), dsComponent_(dsComponent) {
    }
    THComponent(THComponent &&c) : session_(std::move(c.session_)), logger_(std::move(c.logger_)), globalVersion_(c.globalVersion_.load()), dsComponent_(c.dsComponent_) {}
    THComponent &operator=(THComponent &&c) {
        if (this != &c) {
            session_ = std::move(c.session_);
            logger_ = std::move(c.logger_);
            globalVersion_ = c.globalVersion_.load();
            dsComponent_ = c.dsComponent_;
        }
        return *this;
    }
    virtual ~THComponent() {
    }
    TI::GlobalVersion acquireLock(std::string const &account, TI::Key const &name) override final {
        return globalVersion_;
    }
    TI::GlobalVersion releaseLock(std::string const &account, TI::Key const &name) override final {
        return globalVersion_;
    }
    TI::TransactionResponse handleInsert(std::string const &account, TI::Key const &key, TI::Data const &data) override final {
        //Since the whole database is represented as a list
        //, there will never be insert or delete on the "key" (void struct)
        return {globalVersion_, basic::transaction::v2::RequestDecision::FailureConsistency};
    }
    //Please notice that in this setup, TI::ProcessedUpdate is simply TI::DataDelta
    TI::TransactionResponse handleUpdate(std::string const &account, TI::Key const &key, std::optional<TI::VersionSlice> const &updateVersionSlice, TI::ProcessedUpdate const &dataDelta) override final {
        if (session_) {
            std::set<std::string> namesToDelete;
            std::vector<std::string> namesToInsert;
            std::vector<int> amountsToInsert;
            std::vector<double> statsToInsert;
            for (auto const &key : dataDelta.deletes.keys) {
                namesToDelete.insert(key.name);
            }
            for (auto const &item: dataDelta.inserts_updates.items) {
                namesToDelete.insert(item.key.name);
                namesToInsert.push_back(item.key.name);
                amountsToInsert.push_back(item.data.amount);
                statsToInsert.push_back(item.data.stat);
            }
            if (!namesToDelete.empty()) {
                std::vector<std::string> namesToDeleteVec { namesToDelete.begin(), namesToDelete.end() };
                soci::statement delStmt = (session_->prepare 
                            << "DELETE FROM test_table WHERE name = :nm"
                                , soci::use(namesToDeleteVec, "nm"));
                delStmt.execute(true);
            }
            if (!namesToInsert.empty()) {
                soci::statement insStmt = (session_->prepare
                            << "INSERT INTO test_table(name, amount, stat) VALUES (:nm, :amt, :st)"
                                , soci::use(namesToInsert, "nm")
                                , soci::use(amountsToInsert, "amt")
                                , soci::use(statsToInsert, "st"));
                insStmt.execute(true);
            }
            TI::TransactionResponse resp {++globalVersion_, basic::transaction::v2::RequestDecision::Success};
            triggerCallback(resp, key, dataDelta);
            return resp;
        } else {
            return {globalVersion_, basic::transaction::v2::RequestDecision::FailurePermission};
        }
    }
    TI::TransactionResponse handleDelete(std::string const &account, TI::Key const &key, std::optional<TI::Version> const &versionToDelete) override final {
        //Since the whole database is represented as a list
        //, there will never be insert or delete on the "key" (void struct)
        return {globalVersion_, basic::transaction::v2::RequestDecision::FailureConsistency};
    }
};

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("db_file", po::value<std::string>(), "database file")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    if (!vm.count("db_file")) {
        std::cerr << "Please provide database file\n";
        return 1;
    }

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
        transport::rabbitmq::RabbitMQComponent,
        transport::HeartbeatAndAlertComponent,
        DSComponent,
        THComponent
    >;
    using M = infra::RealTimeMonad<TheEnvironment>;
    using R = infra::MonadRunner<M>;

    TheEnvironment env;

    auto session = std::make_shared<soci::session>(
#ifdef _MSC_VER
        *soci::factory_sqlite3()
#else
        soci::sqlite3
#endif
        , vm["db_file"].as<std::string>()
    );

    env.DSComponent::operator=(DSComponent {
        session
        , [&env](std::string const &s) {
            env.log(infra::LogLevel::Info, s);
        }
    });
    env.THComponent::operator=(THComponent {
        session
        , [&env](std::string const &s) {
            env.log(infra::LogLevel::Info, s);
        }
        , static_cast<DSComponent *>(&env)
    });

    transport::HeartbeatAndAlertComponentInitializer<TheEnvironment,transport::rabbitmq::RabbitMQComponent>()
        (&env, "db_one_list_subscription server", transport::ConnectionLocator::parse("127.0.0.1::guest:guest:amq.topic[durable=true]"));

    R r(&env);

    using TF = basic::transaction::v2::TransactionFacility<
        M, TI, DI
        , std::equal_to<int64_t>
        , std::equal_to<int64_t>
        , CheckSummary
    >;
    auto dataStore = std::make_shared<TF::DataStore>();
    using DM = basic::transaction::v2::TransactionDeltaMerger<
        DI, false, M::PossiblyMultiThreaded
        , basic::transaction::v2::TriviallyMerge<int64_t, int64_t>
        , ApplyDelta
    >;
    auto transactionLogicCombinationRes = basic::transaction::v2::transactionLogicCombination<
        R, TI, DI, DM
    >(
        r
        , "transaction_server_components"
        , new TF(dataStore)
    );

    transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::wrapOnOrderFacilityWithExternalEffects
        <TI::Transaction,TI::TransactionResponse,DI::Update>(
        r
        , transactionLogicCombinationRes.transactionFacility
        , transport::ConnectionLocator::parse("127.0.0.1::guest:guest:test_db_one_list_cmd_transaction_queue")
        , "transaction_wrapper_"
        , std::nullopt //no hook
    );
    transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::wrapLocalOnOrderFacility
        <GS::Input,GS::Output,GS::SubscriptionUpdate>(
        r
        , transactionLogicCombinationRes.subscriptionFacility
        , transport::ConnectionLocator::parse("127.0.0.1::guest:guest:test_db_one_list_cmd_subscription_queue")
        , "subscription_wrapper_"
        , std::nullopt //no hook
    );
    
    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "db_one_list_subscription_server");
    r.finalize();

    env.setStatus("program", transport::HeartbeatMessage::Status::Good);
    env.sendAlert("db_one_list_subscription.server.info", infra::LogLevel::Info, "DB one list subscription server started");
    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "DB one list subscription server started");

    std::ostringstream oss;
    oss << "Total " << dataStore->dataMap_.size() << " items in the data store";
    env.log(infra::LogLevel::Info, oss.str());

    infra::terminationController(infra::RunForever {});

    return 0;
}