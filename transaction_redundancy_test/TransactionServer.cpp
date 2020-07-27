#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeMonad.hpp>

#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>

#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/redis/RedisComponent.hpp>
#include <tm_kit/transport/redis/RedisOnOrderFacility.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>
#include <tm_kit/transport/HeartbeatMessageToRemoteFacilityCommand.hpp>

#include <boost/program_options.hpp>

#include "TransactionServerComponents.hpp"

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("lock_choice", po::value<std::string>(), "lock choice (none, simple or compound)")
        ("queue_name_prefix", po::value<std::string>(), "queue name prefix for processing commands")
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
            lockChoice = THComponent::LockChoice::Simple;
        } else if (choiceStr == "compound") {
            lockChoice = THComponent::LockChoice::Compound;
        }
    }

    if (!vm.count("queue_name_prefix")) {
        std::cerr << "Please provide queue name prefix\n";
        return 1;
    }
    std::string queueNamePrefix = vm["queue_name_prefix"].as<std::string>();

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
        transport::redis::RedisComponent,
        transport::HeartbeatAndAlertComponent,
        DSComponent,
        THComponent
    >;
    using M = infra::RealTimeMonad<TheEnvironment>;
    using R = infra::MonadRunner<M>;

    TheEnvironment env;

    auto channel = grpc::CreateChannel("127.0.0.1:2379", grpc::InsecureChannelCredentials());
    env.DSComponent::operator=(DSComponent {
        channel, [&env](std::string const &s) {
            env.log(infra::LogLevel::Info, s);
        }
    });
    env.THComponent::operator=(THComponent {
        lockChoice, channel, [&env](std::string const &s) {
            env.log(infra::LogLevel::Info, s);
        }
    });
    
    transport::HeartbeatAndAlertComponentInitializer<TheEnvironment,transport::redis::RedisComponent>()
        (&env, "transaction redundancy test server", transport::ConnectionLocator::parse("127.0.0.1:6379"));

    R r(&env);

    using TF = basic::transaction::v2::TransactionFacility<
        M, TI, DI
        , CheckVersion
        , CheckVersionSlice
        , CheckSummary
        , ProcessCommandOnLocalData
    >;
    auto dataStore = std::make_shared<TF::DataStore>();
    using DM = basic::transaction::current::TransactionDeltaMerger<
        DI, false, M::PossiblyMultiThreaded
        , ApplyVersionSlice
        , ApplyDataSlice
    >;
    TF tf(dataStore);
    tf.setDeltaProcessor(ProcessCommandOnLocalData([&env](std::string const &s) {
        env.log(infra::LogLevel::Warning, s);
    }));
    auto transactionLogicCombinationRes = basic::transaction::v2::transactionLogicCombination<
        R, TI, DI, DM
    >(
        r
        , "transaction_server_components"
        , &tf
    );

    transport::redis::RedisOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::wrapOnOrderFacilityWithExternalEffects
        <TI::Transaction,TI::TransactionResponse,DI::Update>(
        r
        , transactionLogicCombinationRes.transactionFacility
        , transport::ConnectionLocator::parse("127.0.0.1:6379:::"+queueNamePrefix+"_transaction_queue")
        , "transaction_wrapper_"
        , std::nullopt //no hook
    );
    transport::redis::RedisOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::wrapLocalOnOrderFacility
        <GS::Input,GS::Output,GS::SubscriptionUpdate>(
        r
        , transactionLogicCombinationRes.subscriptionFacility
        , transport::ConnectionLocator::parse("127.0.0.1:6379:::"+queueNamePrefix+"_subscription_queue")
        , "subscription_wrapper_"
        , std::nullopt //no hook
    );

    transport::attachHeartbeatAndAlertComponent(r, &env, "heartbeats.transaction_test_server", std::chrono::seconds(1));

    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "transaction_redundancy_test_server");
    r.finalize();

    env.sendAlert("transaction_redundancy_test.server.info", infra::LogLevel::Info, "Transaction redundancy test server started");
    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "Transaction redundancy test server started");

    infra::terminationController(infra::RunForever {&env});

    return 0;
}