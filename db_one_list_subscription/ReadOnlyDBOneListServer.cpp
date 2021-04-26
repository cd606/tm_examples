#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/complex_key_value_store_components/PreloadAllReadonlyServer.hpp>
#include <tm_kit/transport/complex_key_value_store_components/OnDemandReadonlyServer.hpp>

#include <soci/sqlite3/soci-sqlite3.h>

#include <boost/program_options.hpp>

#include <iostream>

#include "ReadOnlyDBOneListData.hpp"

using namespace dev::cd606::tm;

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
        transport::HeartbeatAndAlertComponent/*,
        transport::ServerSideSimpleIdentityCheckerComponent<
            std::string
            , DBQuery
        >*/
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;

    const std::string MY_ID_FOR_HEARTBEAT = "read_only_db_one_list_server";

    transport::HeartbeatAndAlertComponentInitializer<TheEnvironment,transport::rabbitmq::RabbitMQComponent>()
        (&env, MY_ID_FOR_HEARTBEAT, transport::ConnectionLocator::parse("127.0.0.1::guest:guest:amq.topic[durable=true]"));

    R r(&env);

    auto session = std::make_shared<soci::session>(
#ifdef _MSC_VER
        *soci::factory_sqlite3()
#else
        soci::sqlite3
#endif
        , vm["db_file"].as<std::string>()
    );  
    auto queryFacility = transport::complex_key_value_store_components::PreloadAllReadonlyServer<M>
        ::fullDataQueryFacility<DBKey, DBData>
        (
            session
            , "FROM test_table"
        );
    /*
    auto queryFacility = transport::complex_key_value_store_components::OnDemandReadonlyServer<M>
        ::fullDataQueryFacility<DBKey, DBData>
        (
            session
            , "FROM test_table"
        );
    */
    r.registerOnOrderFacility("queryFacility", queryFacility);
    transport::MultiTransportFacilityWrapper<R>::wrap
        <DBQuery,DBQueryResult>(
        r
        , "queryFacility"
        , transport::MultiTransportFacilityWrapper<R>::addIdentity(r, r.facilityConnector(queryFacility), "add_identity")
        , "rabbitmq://127.0.0.1::guest:guest:test_db_read_only_one_list_queue"
        , "server_wrapper/"
    );

    transport::attachHeartbeatAndAlertComponent(r, &env, MY_ID_FOR_HEARTBEAT+".heartbeat", std::chrono::seconds(1));
    
    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "read_only_db_one_list_server");
    r.finalize();

    env.setStatus("program", transport::HeartbeatMessage::Status::Good);
    env.sendAlert(MY_ID_FOR_HEARTBEAT+".alert", infra::LogLevel::Info, "Read-only DB one list server started");
    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "Read-only DB one list server started");

    infra::terminationController(infra::RunForever {&env});

    return 0;
}
