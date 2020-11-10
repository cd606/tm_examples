#include "simple_demo_chain_version/main_program_logic/MainProgramLogicProvider.hpp"

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/zeromq/ZeroMQComponent.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListenerManagingUtils.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/SharedChainCreator.hpp>

using namespace simple_demo_chain_version;

int main(int argc, char **argv) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<
            basic::real_time_clock::ClockComponent
        >,
        transport::CrossGuidComponent,
        transport::ServerSideSimpleIdentityCheckerComponent<std::string,ConfigureCommand>,
        transport::rabbitmq::RabbitMQComponent,
        transport::zeromq::ZeroMQComponent,
        transport::HeartbeatAndAlertComponent,
        transport::lock_free_in_memory_shared_chain::SharedMemoryChainComponent
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    R r(&env);

    env.setLogFilePrefix("simple_demo_chain_version_main_logic_");

    //setting up the heartbeat

    transport::initializeHeartbeatAndAlertComponent
        (&env, "simple_demo_chain_version MainLogic", "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]");
    env.setStatus("program", transport::HeartbeatMessage::Status::Good);
    transport::attachHeartbeatAndAlertComponent(r, &env, "simple_demo_chain_version.main_logic.heartbeat", std::chrono::seconds(1));

    //setting up chain
    std::string today = infra::withtime_utils::localTimeString(std::chrono::system_clock::now()).substr(0,10);
    std::ostringstream chainLocatorOss;
    chainLocatorOss << "in_shared_memory://::::" << today << "-simple-demo-chain[size=" << (100*1024*1024) << "]";
    std::string chainLocatorStr = chainLocatorOss.str();

    //Please note that this object should not be allowed to go out of scope
    transport::SharedChainCreator<M> sharedChainCreator;

    auto chainFacilityFactory = sharedChainCreator.writerFactory<
        ChainData
        , main_program_logic::MainProgramStateFolder
        , main_program_logic::MainProgramFacilityInputHandler<TheEnvironment>
    >(
        &env
        , chainLocatorStr
    );
    auto chainDataImporterFactory = sharedChainCreator.readerFactory<
        ChainData
        , main_program_logic::TrivialChainDataFolder
    >(
        &env
        , chainLocatorStr
    );

    //get the input data
    auto heartbeatSource = 
        transport::MultiTransportBroadcastListenerManagingUtils<R>
        ::oneBroadcastListener<
            transport::HeartbeatMessage
        >(
            r 
            , "heartbeatListener"
            , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
            , "simple_demo_chain_version.#.heartbeat"
        );

    auto inputDataSource = transport::MultiTransportBroadcastListenerManagingUtils<R>
        ::setupBroadcastListenerThroughHeartbeat<InputData>
    (
        r 
        , heartbeatSource.clone()
        , std::regex("simple_demo_chain_version DataSource")
        , "input data publisher"
        , "input.data"
        , "inputDataSourceComponents"
    );

    //main logic 
    main_program_logic::mainProgramLogicMain(
        r
        , chainFacilityFactory
        , chainDataImporterFactory
        , inputDataSource.clone()
        , transport::MultiTransportFacilityWrapper<R>::facilityWrapper
            <ConfigureCommand, ConfigureResult>(
            "rabbitmq://127.0.0.1::guest:guest:test_config_queue"
            , "cfg_wrapper"
        )
        , "main_program"
    );

    //write execution graph and start

    std::ostringstream graphOss;
    r.writeGraphVizDescription(graphOss, "simple_demo_chain_version_calculator");
    env.log(infra::LogLevel::Info, "The execution graph is:");
    env.log(infra::LogLevel::Info, graphOss.str());

    r.finalize();

    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });

    return 0;
}