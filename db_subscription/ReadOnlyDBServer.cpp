#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/CalculationsOnInit.hpp>
#include <tm_kit/basic/StructFieldInfoUtils.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>

#include <boost/hana/functional/curry.hpp>

#include <soci/soci.h>
#include <soci/sqlite3/soci-sqlite3.h>

#include <boost/program_options.hpp>

#include <iostream>

#include "ReadOnlyDBData.hpp"

using namespace dev::cd606::tm;

using DBDataStorage = std::unordered_map<DBQuery, DBData, basic::struct_field_info_utils::StructFieldInfoBasedHash<DBQuery>>;

DBDataStorage loadDBData(std::string const &dbFile, std::function<void(infra::LogLevel, std::string const &)> logger) {
    auto session = std::make_shared<soci::session>(
#ifdef _MSC_VER
        *soci::factory_sqlite3()
#else
        soci::sqlite3
#endif
        , dbFile
    );
    using Q = basic::struct_field_info_utils::StructFieldInfoBasedDataFiller<DBQuery>;
    using D = basic::struct_field_info_utils::StructFieldInfoBasedDataFiller<DBData>;
    soci::rowset<soci::row> res = 
        session->prepare << ("SELECT "+Q::commaSeparatedFieldNames()+", "+D::commaSeparatedFieldNames()+" FROM test_table");
    DBDataStorage ret;
    for (auto const &r : res) {
        DBQuery q = Q::retrieveData(r, 0);
        DBData d = D::retrieveData(r, Q::FieldCount);
        ret.insert({q, d});
    }
    std::ostringstream oss;
    oss << "[loadDBData] loaded " << ret.size() << " rows";
    logger(infra::LogLevel::Info, oss.str());
    return ret;
}

DBQueryResult doQuery(DBDataStorage const &storage, DBQuery const &query) {
    auto iter = storage.find(query);
    if (iter == storage.end()) {
        return {std::nullopt};
    } else {
        return {iter->second};
    }
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
        (&env, "read_only_db_server.heartbeat", transport::ConnectionLocator::parse("127.0.0.1::guest:guest:amq.topic[durable=true]"));

    R r(&env);

    /*
    auto importer = basic::importerOfValueCalculatedOnInit<M>(
        boost::hana::curry<2>(&loadDBData)(vm["db_file"].as<std::string>())
    );
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
    auto queryFacility = basic::onOrderFacilityUsingInternallyPreCalculatedValue<M,DBQuery>(
        boost::hana::curry<2>(&loadDBData)(vm["db_file"].as<std::string>())
        , [](DBDataStorage const &storage, DBQuery const &query) -> DBQueryResult {
            auto iter = storage.find(query);
            if (iter == storage.end()) {
                return {std::nullopt};
            } else {
                return {iter->second};
            }
        }
    );
    r.registerOnOrderFacility("queryFacility", queryFacility);
    transport::MultiTransportFacilityWrapper<R>::wrap
        //<DBQuery,DBQueryResult,DBDataStorage>(
        <DBQuery,DBQueryResult>(
        r
        , queryFacility
        , "rabbitmq://127.0.0.1::guest:guest:test_db_read_only_queue"
        , "server_wrapper/"
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
