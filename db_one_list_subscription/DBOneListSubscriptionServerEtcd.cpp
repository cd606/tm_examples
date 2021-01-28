#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/BasicWithTimeApp.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/transaction/v2/TransactionLogicCombination.hpp>
#include <tm_kit/basic/transaction/named_value_store/DataModel.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>
#include <tm_kit/transport/named_value_store_components/EtcdNamedValueStoreComponents.hpp>

#include "DBData.hpp"

#include <boost/program_options.hpp>

#include <iostream>

using namespace dev::cd606::tm;
using namespace db_one_list_subscription;

using DI = basic::transaction::named_value_store::DI<db_data>;
using TI = basic::transaction::named_value_store::TI<db_data>;
using GS = basic::transaction::named_value_store::GS<boost::uuids::uuid,db_data>;

using Data = basic::transaction::named_value_store::Collection<db_data>;

using DSComponent = transport::named_value_store_components::etcd::DSComponent<db_data>;
using THComponent = transport::named_value_store_components::etcd::THComponent<db_data>;

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("storagePrefix", po::value<std::string>(), "storage prefix")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    if (!vm.count("storagePrefix")) {
        std::cerr << "Please provide storage prefix\n";
        return 1;
    }

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
        DSComponent,
        THComponent
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;

    auto channel = grpc::CreateChannel("127.0.0.1:2379", grpc::InsecureChannelCredentials());
    env.DSComponent::operator=(DSComponent {
        channel, [&env](std::string const &s) {
            env.log(infra::LogLevel::Info, s);
        }, vm["storagePrefix"].as<std::string>()
    });
    env.THComponent::operator=(THComponent {
        transport::named_value_store_components::etcd::LockChoice::None
        , channel, [&env](std::string const &s) {
            env.log(infra::LogLevel::Info, s);
        }, vm["storagePrefix"].as<std::string>()
    });

    transport::HeartbeatAndAlertComponentInitializer<TheEnvironment,transport::rabbitmq::RabbitMQComponent>()
        (&env, "db_one_list_subscription server", transport::ConnectionLocator::parse("127.0.0.1::guest:guest:amq.topic[durable=true]"));

    R r(&env);

    using TF = basic::transaction::named_value_store::TF<M, db_data>;
    auto dataStore = std::make_shared<TF::DataStore>();
    using DM = basic::transaction::named_value_store::DM<M, db_data>;
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
        , "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_transaction_queue"
        , "transaction_wrapper/"
    );
    transport::MultiTransportFacilityWrapper<R>::wrap
        <GS::Input,GS::Output,GS::SubscriptionUpdate>(
        r
        , transactionLogicCombinationRes.subscriptionFacility
        , "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_subscription_queue"
        , "subscription_wrapper/"
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

    infra::terminationController(infra::RunForever {&env});

    return 0;
}