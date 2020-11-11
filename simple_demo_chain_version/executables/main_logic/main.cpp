#include "simple_demo_chain_version/main_program_logic/MainProgramLogicProvider.hpp"
#include "simple_demo_chain_version/security_keys/VerifyingKeys.hpp"

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
#include <tm_kit/transport/security/SignatureAndVerifyHookFactoryComponents.hpp>

using namespace simple_demo_chain_version;

int main(int argc, char **argv) {
    const transport::security::SignatureHelper::PrivateKey mainLogicKey = {
        0xBD,0x07,0x1E,0x7C,0x85,0x73,0x30,0xE4,0x19,0xA5,0x6F,0x5B,0x1D,0x71,0x73,0x12,
        0x81,0x5D,0x4D,0x8C,0x54,0xB9,0x03,0x85,0x05,0xB7,0x30,0x63,0x73,0xBF,0xBC,0xF5,
        0x97,0x17,0xC4,0x33,0xC6,0x24,0xA6,0xEF,0xD5,0x0C,0xB9,0xB5,0x02,0x1E,0xFF,0x38,
        0x54,0x57,0xCE,0x9A,0x45,0x3F,0x74,0x74,0x26,0x4B,0x3C,0x78,0x54,0xE3,0x07,0x50
    };
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
        transport::lock_free_in_memory_shared_chain::SharedMemoryChainComponent,
        transport::security::SignatureWithNameHookFactoryComponent<ChainData>,
        transport::security::VerifyUsingNameTagHookFactoryComponent<ChainData>
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    env.transport::security::SignatureWithNameHookFactoryComponent<ChainData>::operator=(
        transport::security::SignatureWithNameHookFactoryComponent<ChainData> {
            "main_logic"
            , mainLogicKey
        }
    );
    env.transport::security::VerifyUsingNameTagHookFactoryComponent<ChainData>::operator=(
        transport::security::VerifyUsingNameTagHookFactoryComponent<ChainData> {
            verifyingKeys
        }
    );
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