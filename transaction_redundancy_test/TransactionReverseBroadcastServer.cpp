#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/AppRunnerUtils.hpp>

#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/redis/RedisComponent.hpp>
#include <tm_kit/transport/redis/RedisOnOrderFacility.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>
#include <tm_kit/transport/HeartbeatMessageToRemoteFacilityCommand.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListener.hpp>

#include <boost/program_options.hpp>

#include "TransactionServerComponents.hpp"

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("lock_choice", po::value<std::string>(), "lock choice (none, simple or compound)")
        ("queue_name", po::value<std::string>(), "queue name for processing subscription commands")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    THComponent::LockChoice lockChoice = THComponent::LockChoice::None;
    if (vm.count("lock_choice")) {
        auto choiceStr = vm["lock_choice"].as<std::string>();
        if (choiceStr == "simple") {
            std::cerr << "Simple lock support has been removed\n";
            return 1;
            //lockChoice = THComponent::LockChoice::Simple;
        } else if (choiceStr == "compound") {
            lockChoice = THComponent::LockChoice::Compound;
        }
    }

    if (!vm.count("queue_name")) {
        std::cerr << "Please provide queue name\n";
        return 1;
    }
    std::string queueName = vm["queue_name"].as<std::string>();

    std::string broadcastChannel = "redis://127.0.0.1:6379";
    auto parsedBroadcastChannel = *transport::parseMultiTransportBroadcastChannel(broadcastChannel);
    std::string broadcastTopic = "etcd_test.transaction_commands";

    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::real_time_clock::ClockComponent>,
        transport::BoostUUIDComponent,
        transport::ServerSideSimpleIdentityCheckerComponent<
            std::string
            , GS::Input>,
        transport::redis::RedisComponent,
        transport::HeartbeatAndAlertComponent,
        DSComponent,
        THComponent
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    std::atomic<int64_t> compoundLockQueueVersion=0, compoundLockQueueRevision=0;

    auto channel = grpc::CreateChannel("127.0.0.1:2379", grpc::InsecureChannelCredentials());
    env.DSComponent::operator=(DSComponent {
        channel, [&env](std::string const &s) {
            env.log(infra::LogLevel::Info, s);
        }, &compoundLockQueueVersion, &compoundLockQueueRevision
    });
    env.THComponent::operator=(THComponent {
        lockChoice, channel, [&env](std::string const &s) {
            env.log(infra::LogLevel::Info, s);
        }, &compoundLockQueueVersion, &compoundLockQueueRevision
    });
    
    transport::HeartbeatAndAlertComponentInitializer<TheEnvironment,transport::redis::RedisComponent>()
        (&env, "transaction redundancy test reverse broadcast server", transport::ConnectionLocator::parse("127.0.0.1:6379"));

    R r(&env);

    using TE = basic::transaction::v2::TransactionHandlingExporter<
        M, TI, DI
        , CheckVersion
        , CheckVersionSlice
        , CheckSummary
        , ProcessCommandOnLocalData
    >;
    auto dataStore = std::make_shared<TE::DataStore>();
    using DM = basic::transaction::current::TransactionDeltaMerger<
        DI, false, M::PossiblyMultiThreaded
        , ApplyVersionSlice
        , ApplyDataSlice
    >;
    TE te(dataStore);
    te.setDeltaProcessor(ProcessCommandOnLocalData([&env](std::string const &s) {
        env.log(infra::LogLevel::Warning, s);
    }));
    auto transactionLogicCombinationRes = basic::transaction::v2::silentTransactionLogicCombination<
        R, TI, DI, DM
    >(
        r
        , "transaction_server_components"
        , &te
    );

    auto broadcastKey = M::constFirstPushKeyImporter(
        transport::MultiTransportBroadcastListenerInput { 
            transport::MultiTransportBroadcastListenerAddSubscription {
                std::get<0>(parsedBroadcastChannel)
                , std::get<1>(parsedBroadcastChannel)
                , broadcastTopic
            }
        }
    );
    r.registerImporter("broadcastKey", broadcastKey);
    auto discardTopic = M::template liftPure<basic::TypedDataWithTopic<basic::CBOR<TI::TransactionWithAccountInfo>>>(
        [](basic::TypedDataWithTopic<basic::CBOR<TI::TransactionWithAccountInfo>> &&d) {
            return std::move(d.content.value);
        }
    );
    r.registerAction("discardTopic", discardTopic);
    auto transactionInput = basic::AppRunnerUtilComponents<R>::importWithTrigger(
        r
        , r.importItem(broadcastKey)
        , M::onOrderFacilityWithExternalEffects(
            new transport::MultiTransportBroadcastListener<TheEnvironment, basic::CBOR<TI::TransactionWithAccountInfo>>()
        )
        , std::nullopt //the trigger response is not needed
        , "transactionInput"
    );

    transactionLogicCombinationRes.transactionHandler(
        r
        , r.execute(discardTopic, std::move(transactionInput))
    );

    transport::redis::RedisOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::wrapLocalOnOrderFacility
        <GS::Input,GS::Output,GS::SubscriptionUpdate>(
        r
        , transactionLogicCombinationRes.subscriptionFacility
        , transport::ConnectionLocator::parse("127.0.0.1:6379:::"+queueName)
        , "subscription_wrapper_"
        , std::nullopt //no hook
    );

    transport::attachHeartbeatAndAlertComponent(r, &env, "heartbeats.transaction_test_reverse_broadcast_server", std::chrono::seconds(1));

    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "transaction_redundancy_test_reverse_broadcast_server");
    r.finalize();

    env.sendAlert("transaction_redundancy_test.server.info", infra::LogLevel::Info, "Transaction redundancy test server started");
    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "Transaction redundancy test reverse broadcast server started");

    infra::terminationController(infra::RunForever {&env});

    return 0;
}