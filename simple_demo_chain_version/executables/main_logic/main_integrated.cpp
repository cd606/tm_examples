#include "simple_demo_chain_version/main_program_logic/MainProgramLogicProvider.hpp"
#include "simple_demo_chain_version/security_keys/VerifyingKeys.hpp"
#include "simple_demo_chain_version/enable_server_data/EnableServerTransactionData.hpp"

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/TraceNodesComponent.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/transaction/v2/DataStreamClientCombination.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/zeromq/ZeroMQComponent.hpp>
#include <tm_kit/transport/multicast/MulticastComponent.hpp>
#include <tm_kit/transport/shared_memory_broadcast/SharedMemoryBroadcastComponent.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListenerManagingUtils.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/SharedChainCreator.hpp>
#include <tm_kit/transport/security/SignatureAndVerifyHookFactoryComponents.hpp>
#include <tm_kit/transport/ExitDataSource.hpp>
#include <tm_kit/transport/RemoteTransactionSubscriberManagingUtils.hpp>

using namespace simple_demo_chain_version;
using namespace simple_demo_chain_version::enable_server;

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
        //infra::TraceNodesComponent,
        basic::TimeComponentEnhancedWithSpdLogging<
            basic::real_time_clock::ClockComponent
        >,
        transport::CrossGuidComponent,
        transport::rabbitmq::RabbitMQComponent,
        transport::zeromq::ZeroMQComponent,
        transport::multicast::MulticastComponent,
        transport::shared_memory_broadcast::SharedMemoryBroadcastComponent,
        transport::HeartbeatAndAlertComponent,
        transport::lock_free_in_memory_shared_chain::SharedMemoryChainComponent,
        transport::security::SignatureWithNameHookFactoryComponent<ChainData>,
        transport::security::VerifyUsingNameTagHookFactoryComponent<ChainData>,
        transport::ClientSideSimpleIdentityAttacherComponent<std::string, GS::Input>
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
    env.transport::ClientSideSimpleIdentityAttacherComponent<std::string,GS::Input>::operator=(
        transport::ClientSideSimpleIdentityAttacherComponent<std::string,GS::Input>(
            "main_integrated"
        )
    );
    R r(&env);

    env.setLogFilePrefix("simple_demo_chain_version_main_logic_integrated_");

    //setting up the heartbeat

    transport::initializeHeartbeatAndAlertComponent
        (&env, "simple_demo_chain_version MainLogic Integrated", "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]");
    env.setStatus("program", transport::HeartbeatMessage::Status::Good);
    transport::attachHeartbeatAndAlertComponent(r, &env, "simple_demo_chain_version.main_logic.heartbeat", std::chrono::seconds(1));

    //setting up chain
    std::string today = infra::withtime_utils::localTimeString(std::chrono::system_clock::now()).substr(0,10);
    std::ostringstream chainLocatorOss;
    chainLocatorOss << "in_shared_memory://::::" << today << "-simple-demo-chain[size=" << (100*1024*1024) << "]";
    std::string chainLocatorStr = chainLocatorOss.str();

    //Please note that this object should not be allowed to go out of scope
    transport::SharedChainCreator<M> sharedChainCreator;

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
    auto exitDataSource = std::get<0>(transport::ExitDataSourceCreator::addExitDataSource(
        r, "onExit"
    ));
    auto enableServerSubscriber = transport::RemoteTransactionSubscriberManagingUtils<R>
        ::createSubscriber<basic::CBOR,GS>
        (
            r 
            , heartbeatSource.clone()
            , std::regex("simple_demo_chain_version Enable Server")
            , "transaction_server_components/subscription_handler"
            , GS::Subscription {{basic::VoidStruct {}}}
            , {std::move(exitDataSource)}
        );
    //for some reason gcc requires the explicit parameter specification for GS::Input
    //but clang does not require that
    auto enableServerDataSource = basic::transaction::v2::basicDataStreamClientCombination<R,DI,GS::Input>(
        r 
        , "translateEnableServerDataSource"
        , enableServerSubscriber
    );
    auto convertToBool = M::liftPure<M::KeyedData<GS::Input,DI::FullUpdate>>(
        [&env](M::KeyedData<GS::Input,DI::FullUpdate> &&update) -> bool {
            bool res = false;
            for (auto const &oneUpdate : update.data.data) {
                res = (oneUpdate.data && *(oneUpdate.data));
            }
            env.log(infra::LogLevel::Info, std::string("Received enabled update: ")+(res?"enabled":"disabled"));
            return res;
        }
    );
    r.registerAction("converToBool", convertToBool);
    r.execute(convertToBool, std::move(enableServerDataSource));

    std::string alertTopic = "";
    auto statusUpdaterUsingHeartbeatAndAlert = [&env,alertTopic](bool enabled) {
        if constexpr (std::is_convertible_v<TheEnvironment *, transport::HeartbeatAndAlertComponent *>) {
            if (enabled) {
                env.setStatus("calculation_status", transport::HeartbeatMessage::Status::Good, "enabled");
                if (alertTopic != "") {
                    env.sendAlert(alertTopic, infra::LogLevel::Info, "main logic calculation enabled");
                }
            } else {
                env.setStatus("calculation_status", transport::HeartbeatMessage::Status::Warning, "disabled");
                if (alertTopic != "") {
                    env.sendAlert(alertTopic, infra::LogLevel::Warning, "main logic calculation disabled");
                }
            }
        }
    };

    //main logic 
    main_program_logic::mainProgramLogicMain(
        r
        , std::get<0>(main_program_logic::chainBasedRequestHandler(
            r
            , sharedChainCreator 
            , chainLocatorStr
            , "main_program"
        ))
        , inputDataSource.clone()
        , r.actionAsSource(convertToBool)
        , "main_program"
        , statusUpdaterUsingHeartbeatAndAlert
    );

    //write execution graph and start

    std::ostringstream graphOss;
    r.writeGraphVizDescription(graphOss, "simple_demo_chain_version_main_integrated");
    env.log(infra::LogLevel::Info, "The execution graph is:");
    env.log(infra::LogLevel::Info, graphOss.str());

    r.finalize();

    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });

    return 0;
}
