#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/BasicWithTimeMonad.hpp>
#include <tm_kit/infra/RealTimeMonad.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/transaction/FileBackedSingleVersionProviderComponent.hpp>
#include <tm_kit/basic/transaction/SingleKeyLocalTransactionHandlerComponent.hpp>
#include <tm_kit/basic/transaction/SingleKeyLocalStorageTransactionBroker.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQOnOrderFacility.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>

#include "dbData.pb.h"
#include "DBDataEq.hpp"

#include <soci/soci.h>
#include <soci/sqlite3/soci-sqlite3.h>

#include <boost/program_options.hpp>

#include <iostream>

using namespace dev::cd606::tm;
using namespace db_subscription;

class THComponent : public basic::transaction::SingleKeyLocalTransactionHandlerComponent<
    std::string, db_data
> {
private:
    std::mutex mutex_;
    std::unique_ptr<soci::session> session_;
    std::function<void(std::string)> logger_;
public:
    THComponent() : mutex_(), session_(), logger_() {
    }
    THComponent(std::unique_ptr<soci::session> &&session, std::function<void(std::string)> const &logger) : mutex_(), session_(std::move(session)), logger_(logger) {
    }
    THComponent(THComponent &&c) : mutex_(), session_(std::move(c.session_)), logger_(std::move(c.logger_)) {}
    THComponent &operator=(THComponent &&c) {
        if (this != &c) {
            std::lock_guard<std::mutex> _(mutex_);
            session_ = std::move(c.session_);
            logger_ = std::move(c.logger_);
        }
        return *this;
    }
    virtual ~THComponent() {
    }
    virtual bool handleInsert(
            std::string const &account
            , std::string const &key
            , db_data const &data
        ) override final {
        std::lock_guard<std::mutex> _(mutex_);
        if (session_) {
            (*session_) << "INSERT INTO test_table(name, value1, value2) VALUES(:name, :val1, :val2)"
                        , soci::use(key, "name")
                        , soci::use(data.value1(), "val1")
                        , soci::use(data.value2(), "val2");
            return true;
        } else {
            return false;
        }
    }
    virtual bool handleUpdate(
        std::string const &account
        , std::string const &key
        , db_data const &data
    ) override final {
        std::lock_guard<std::mutex> _(mutex_);
        if (session_) {
            (*session_) << "UPDATE test_table SET value1 = :val1, value2 = :val2 WHERE name = :name"
                        , soci::use(key, "name")
                        , soci::use(data.value1(), "val1")
                        , soci::use(data.value2(), "val2");
            return true;
        } else {
            return false;
        }
    }
    virtual bool handleDelete(
        std::string const &account
        , std::string const &key
    ) override final {
        std::lock_guard<std::mutex> _(mutex_);
        if (session_) {
            (*session_) << "DELETE FROM test_table WHERE name = :name"
                        , soci::use(key, "name");
            return true;
        } else {
            return false;
        }
    }
    virtual std::vector<std::tuple<std::string,db_data>> loadInitialData() override final {
        std::vector<std::tuple<std::string,db_data>> ret;
        std::lock_guard<std::mutex> _(mutex_);
        if (session_) {
            soci::rowset<soci::row> res = 
                session_->prepare << "SELECT name, value1, value2 FROM test_table";
            for (auto const &r : res) {
                db_data item;
                item.set_value1(r.get<int>(1));
                item.set_value2(r.get<std::string>(2));
                ret.push_back({r.get<std::string>(0), std::move(item)});
            }
        }
        std::ostringstream oss;
        oss << "[THComponent] loaded " << ret.size() << " rows";
        logger_(oss.str());
        return ret;
    }
};

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("version_file", po::value<std::string>(), "version file")
        ("db_file", po::value<std::string>(), "database file")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    if (!vm.count("version_file") || !vm.count("db_file")) {
        std::cerr << "Please provide version file and database file\n";
        return 1;
    }

    using TI = basic::transaction::SingleKeyTransactionInterface<
        std::string
        , db_data
        , int64_t
        , transport::BoostUUIDComponent::IDType
    >;
    using VP = basic::transaction::FileBackedSingleVersionProviderComponent<
        std::string
        , db_data
    >;
    
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::real_time_clock::ClockComponent>,
        transport::BoostUUIDComponent,
        transport::ServerSideSimpleIdentityCheckerComponent<
            std::string
            , TI::BasicFacilityInput>,
        transport::rabbitmq::RabbitMQComponent,
        transport::HeartbeatAndAlertComponent,
        VP,
        THComponent
    >;
    using M = infra::RealTimeMonad<TheEnvironment>;
    using R = infra::MonadRunner<M>;
    using TB = basic::transaction::SingleKeyLocalStorageTransactionBroker<
        M
        , std::string
        , db_data
        , int64_t
        , db_data
        , DBDataEq
    >;

    TheEnvironment env;
    env.VP::operator=(VP {vm["version_file"].as<std::string>()});
    env.THComponent::operator=(THComponent {
        std::make_unique<soci::session>(
#ifdef _MSC_VER
            *soci::factory_sqlite3()
#else
            soci::sqlite3
#endif
            , vm["db_file"].as<std::string>())
        , [&env](std::string const &s) {
            env.log(infra::LogLevel::Info, s);
        }
    });

    transport::HeartbeatAndAlertComponentInitializer<TheEnvironment,transport::rabbitmq::RabbitMQComponent>()
        (&env, "db_subscription server", transport::ConnectionLocator::parse("127.0.0.1::guest:guest:amq.topic[durable=true]"));

    R r(&env);
    auto facility = M::fromAbstractOnOrderFacility<TI::FacilityInput,TI::FacilityOutput>(
        new TB()
    );
    r.registerOnOrderFacility("facility", facility);
    transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::wrapOnOrderFacility
        <TI::BasicFacilityInput,TI::FacilityOutput>(
        r
        , facility
        , transport::ConnectionLocator::parse("127.0.0.1::guest:guest:test_db_cmd_queue")
        , "db_subscription_server_wrapper_"
        , std::nullopt //no hook
    );
    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "db_subscription_server");
    r.finalize();

    env.setStatus("program", transport::HeartbeatMessage::Status::Good);
    env.sendAlert("db_subscription.server.info", infra::LogLevel::Info, "DB subscription server started");
    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "DB subscription server started");

    infra::terminationController(infra::RunForever {});
}