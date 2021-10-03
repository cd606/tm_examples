#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/BasicWithTimeApp.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>
#include <tm_kit/transport/complex_key_value_store_components/SingleTablePerItemTransactionServer.hpp>

#include "ReadOnlyDBData.hpp"

#include <soci/sqlite3/soci-sqlite3.h>

#include <boost/program_options.hpp>

#include <iostream>

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

    using TransactionServer =
        transport::complex_key_value_store_components::SingleTablePerItemTransactionServer<
            DBKey, DBData
        >;
    using TI = TransactionServer::TI;
    using DI = TransactionServer::DI;
    using GS = TransactionServer::GS<boost::uuids::uuid>;

    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
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
        TransactionServer::DSComponent,
        TransactionServer::THComponent
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
    TransactionServer::initializeEnvironment(&env, session, "test_table");

    transport::HeartbeatAndAlertComponentInitializer<TheEnvironment,transport::rabbitmq::RabbitMQComponent>()
        (&env, "db_subscription server", transport::ConnectionLocator::parse("127.0.0.1::guest:guest:amq.topic[durable=true]"));

    R r(&env);
    auto transactionLogicCombinationRes = TransactionServer::setupTransactionServer(r, "transaction_server_components");
    
    transport::MultiTransportFacilityWrapper<R>::wrapWithProtocol
        <basic::CBOR,TI::Transaction,TI::TransactionResponse,DI::Update>(
        r
        , transactionLogicCombinationRes.transactionFacility
        , "rabbitmq://127.0.0.1::guest:guest:test_db_cmd_transaction_queue_2"
        , "transaction_wrapper/"
    );
    transport::MultiTransportFacilityWrapper<R>::wrapWithProtocol
        <basic::CBOR,GS::Input,GS::Output,GS::SubscriptionUpdate>(
        r
        , transactionLogicCombinationRes.subscriptionFacility
        , "rabbitmq://127.0.0.1::guest:guest:test_db_cmd_subscription_queue_2"
        , "subscription_wrapper/"
    );
    
    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "db_subscription_server");
    r.finalize();

    env.setStatus("program", transport::HeartbeatMessage::Status::Good);
    env.sendAlert("db_subscription.versionless_server.info", infra::LogLevel::Info, "Versionless DB subscription server started");
    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "Versionless DB subscription server started");

    infra::terminationController(infra::RunForever {&env});

    return 0;
}