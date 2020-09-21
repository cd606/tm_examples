#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/CalculationsOnInit.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQOnOrderFacility.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>

#include <soci/soci.h>
#include <soci/sqlite3/soci-sqlite3.h>

#include <boost/program_options.hpp>

#include <iostream>

#include "ReadOnlyDBData.hpp"

using namespace dev::cd606::tm;

using DBDataStorage = std::unordered_map<std::string, DBData>;

DBDataStorage loadDBData(std::shared_ptr<soci::session> session, std::function<void(infra::LogLevel, std::string const &)> logger) {
    soci::rowset<soci::row> res = 
        session->prepare << "SELECT name, value1, value2 FROM test_table";
    DBDataStorage ret;
    for (auto const &r : res) {
        ret.insert({
            r.get<std::string>(0)
            , DBData {
                r.get<int>(1)
                , r.get<std::string>(2)
            }
        });
    }
    std::ostringstream oss;
    oss << "[loadDBData] loaded " << ret.size() << " rows";
    logger(infra::LogLevel::Info, oss.str());
    return ret;
}

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
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::CrossGuidComponent,
        transport::rabbitmq::RabbitMQComponent,
        transport::HeartbeatAndAlertComponent,
        infra::ConstValueHolderComponent<std::shared_ptr<soci::session>>
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;

    auto session = std::make_shared<soci::session>(
#ifdef _MSC_VER
        *soci::factory_sqlite3()
#else
        soci::sqlite3
#endif
        , vm["db_file"].as<std::string>()
    );
    env.infra::ConstValueHolderComponent<std::shared_ptr<soci::session>>::operator=(
        infra::ConstValueHolderComponent<std::shared_ptr<soci::session>> {
            session
        }
    );

    transport::HeartbeatAndAlertComponentInitializer<TheEnvironment,transport::rabbitmq::RabbitMQComponent>()
        (&env, "read_only_db_server.heartbeat", transport::ConnectionLocator::parse("127.0.0.1::guest:guest:amq.topic[durable=true]"));

    R r(&env);
    
    /*
    auto importer = basic::importerOfValueCalculatedOnInit<M,std::shared_ptr<soci::session>>(&loadDBData);
    auto queryFacility = basic::localOnOrderFacilityUsingPreCalculatedValue<M,DBQuery,DBDataStorage>(
        [](DBDataStorage const &storage, DBQuery const &query) -> DBQueryResult {
            auto iter = storage.find(query.name);
            if (iter == storage.end()) {
                return {std::nullopt};
            } else {
                return {iter->second};
            }
        }
    );
    r.registerImporter("importer", importer);
    r.registerLocalOnOrderFacility("queryFacility", queryFacility);
    r.connect(r.importItem(importer), r.localFacilityAsSink(queryFacility));
    */
    auto queryFacility = basic::onOrderFacilityUsingInternallyPreCalculatedValue<M,DBQuery,std::shared_ptr<soci::session>>(
        &loadDBData
        , [](DBDataStorage const &storage, DBQuery const &query) -> DBQueryResult {
            auto iter = storage.find(query.name);
            if (iter == storage.end()) {
                return {std::nullopt};
            } else {
                return {iter->second};
            }
        }
    );
    r.registerOnOrderFacility("queryFacility", queryFacility);
    /*
    transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>::WithoutIdentity::wrapLocalOnOrderFacility
        <DBQuery,DBQueryResult,DBDataStorage>(
    */
    transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>::WithoutIdentity::wrapOnOrderFacility
        <DBQuery,DBQueryResult>(
        r
        , queryFacility
        , transport::ConnectionLocator::parse("127.0.0.1::guest:guest:test_db_read_only_queue")
        , "server_wrapper_"
        , std::nullopt //no hook
    );
    
    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "read_only_db_server");
    r.finalize();

    env.setStatus("program", transport::HeartbeatMessage::Status::Good);
    env.sendAlert("read_only_db_server.alert", infra::LogLevel::Info, "Read-only DB server started");
    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "Read-only DB server started");

    infra::terminationController(infra::RunForever {&env});

    return 0;
}
