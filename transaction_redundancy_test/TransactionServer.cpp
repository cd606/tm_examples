#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>

#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/redis/RedisComponent.hpp>
#include <tm_kit/transport/redis/RedisOnOrderFacility.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>
#include <tm_kit/transport/HeartbeatMessageToRemoteFacilityCommand.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>

#include <boost/program_options.hpp>

#include "TransactionServerComponents.hpp"
#include "LocalTransactionServerComponents.hpp"

template <class Env>
int run(Env &env, std::string const &queueNamePrefix) {
    using M = infra::RealTimeApp<Env>;
    using R = infra::AppRunner<M>;

    transport::initializeHeartbeatAndAlertComponent
        (&env, "transaction redundancy test server", "redis://127.0.0.1:6379");

    R r(&env);

    using TF = basic::transaction::v2::TransactionFacility<
        M, TI, DI, basic::transaction::v2::TransactionLoggingLevel::None
        , CheckVersion
        , CheckVersionSlice
        , CheckSummary
        , ProcessCommandOnLocalData
    >;
    auto dataStore = std::make_shared<typename TF::DataStore>();
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
        R, TI, DI, DM, basic::transaction::v2::SubscriptionLoggingLevel::None
    >(
        r
        , "transaction_server_components"
        , &tf
    );

    transport::MultiTransportFacilityWrapper<R>
        ::template wrap<TI::Transaction,TI::TransactionResponse,DI::Update>(
            r
            , transactionLogicCombinationRes.transactionFacility
            , ("redis://127.0.0.1:6379:::"+queueNamePrefix+"_transaction_queue")
            , "transaction_wrapper/"
        );
    transport::MultiTransportFacilityWrapper<R>
        ::template wrap<GS::Input,GS::Output,GS::SubscriptionUpdate>(
            r
            , transactionLogicCombinationRes.subscriptionFacility
            , ("redis://127.0.0.1:6379:::"+queueNamePrefix+"_subscription_queue")
            , "subscription_wrapper/"
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

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("lock_choice", po::value<std::string>(), "lock choice (none, simple or compound)")
        ("queue_name_prefix", po::value<std::string>(), "queue name prefix for processing commands")
        ("local_test", "local test only")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }
    if (!vm.count("queue_name_prefix")) {
        std::cerr << "Please provide queue name prefix\n";
        return 1;
    }
    std::string queueNamePrefix = vm["queue_name_prefix"].as<std::string>();

    if (vm.count("local_test")) {
        using TheEnvironment = infra::Environment<
            infra::CheckTimeComponent<false>,
            infra::TrivialExitControlComponent,
            basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent, true, true>,
            transport::BoostUUIDComponent,
            transport::ServerSideSimpleIdentityCheckerComponent<
                std::string
                , TI::Transaction>,
            transport::ServerSideSimpleIdentityCheckerComponent<
                std::string
                , GS::Input>,
            transport::redis::RedisComponent,
            transport::HeartbeatAndAlertComponent,
            LocalDSComponent,
            LocalTHComponent
        >;
        
        TheEnvironment env;
        
        env.LocalDSComponent::operator=(LocalDSComponent {
            [&env](std::string const &s) {
                env.log(infra::LogLevel::Info, s);
            }
        });
        env.LocalTHComponent::operator=(LocalTHComponent {
            &env, [&env](std::string const &s) {
                env.log(infra::LogLevel::Info, s);
            }
        });

        return run<TheEnvironment>(env, queueNamePrefix);
    } else {
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

        using TheEnvironment = infra::Environment<
            infra::CheckTimeComponent<false>,
            infra::TrivialExitControlComponent,
            basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent, true, true>,
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

        return run<TheEnvironment>(env, queueNamePrefix);
    }

    
}