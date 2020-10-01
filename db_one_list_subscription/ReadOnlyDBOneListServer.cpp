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

#include <boost/hana/functional/curry.hpp>

#include <soci/soci.h>
#include <soci/sqlite3/soci-sqlite3.h>

#include <boost/program_options.hpp>

#include <iostream>

#include "ReadOnlyDBOneListData.hpp"

using namespace dev::cd606::tm;

DBQueryResult loadDBData(std::string const &dbFile, std::function<void(infra::LogLevel, std::string const &)> logger) {
    auto session = std::make_shared<soci::session>(
#ifdef _MSC_VER
        *soci::factory_sqlite3()
#else
        soci::sqlite3
#endif
        , dbFile
    );    
    soci::rowset<soci::row> res = 
        session->prepare << "SELECT name, amount, stat FROM test_table";
    DBQueryResult ret;
    for (auto const &r : res) {
        ret.value.push_back(DBData {
            r.get<std::string>(0)
            , r.get<int>(1)
            , r.get<double>(2)
        });
    }
    std::ostringstream oss;
    oss << "[loadDBData] loaded " << ret.value.size() << " rows";
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
        transport::HeartbeatAndAlertComponent
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;

    transport::HeartbeatAndAlertComponentInitializer<TheEnvironment,transport::rabbitmq::RabbitMQComponent>()
        (&env, "read_only_db_one_list_server.heartbeat", transport::ConnectionLocator::parse("127.0.0.1::guest:guest:amq.topic[durable=true]"));

    R r(&env);

    /*
    auto importer = basic::importerOfValueCalculatedOnInit<M>(
        boost::hana::curry<2>(&loadDBData)(vm["db_file"].as<std::string>())
    );
    auto queryFacility = basic::localOnOrderFacilityReturningPreCalculatedValue<M,DBQuery,DBQueryResult>();
    r.registerImporter("importer", importer);
    r.registerLocalOnOrderFacility("queryFacility", queryFacility);
    r.connect(r.importItem(importer), r.localFacilityAsSink(queryFacility));
    */
    auto queryFacility = basic::onOrderFacilityReturningInternallyPreCalculatedValue<M,DBQuery>(
        boost::hana::curry<2>(&loadDBData)(vm["db_file"].as<std::string>())
    );
    r.registerOnOrderFacility("queryFacility", queryFacility);
    /*
    transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>::wrapLocalOnOrderFacility
        <DBQuery,DBQueryResult,DBQueryResult>(
    */
    transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>::wrapOnOrderFacility
        <DBQuery,DBQueryResult>(
        r
        , queryFacility
        , transport::ConnectionLocator::parse("127.0.0.1::guest:guest:test_db_read_only_one_list_queue")
        , "server_wrapper_"
        , std::nullopt //no hook
    );
    
    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "read_only_db_one_list_server");
    r.finalize();

    env.setStatus("program", transport::HeartbeatMessage::Status::Good);
    env.sendAlert("read_only_db_one_list_server.alert", infra::LogLevel::Info, "Read-only DB one list server started");
    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "Read-only DB one list server started");

    infra::terminationController(infra::RunForever {&env});

    return 0;
}
