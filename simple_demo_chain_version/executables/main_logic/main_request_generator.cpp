#include "simple_demo_chain_version/main_program_logic/MainProgramLogicProvider.hpp"
#include "simple_demo_chain_version/enable_server_data/EnableServerTransactionData.hpp"

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/WrapFacilitioidConnectorForSerialization.hpp>
#include <tm_kit/basic/transaction/v2/DataStreamClientCombination.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListenerManagingUtils.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/ExitDataSource.hpp>
#include <tm_kit/transport/RemoteTransactionSubscriberManagingUtils.hpp>

using namespace simple_demo_chain_version;
using namespace simple_demo_chain_version::enable_server;

int main(int argc, char **argv) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<
            basic::real_time_clock::ClockComponent
        >,
        transport::CrossGuidComponent,
        transport::AllNetworkTransportComponents,
        transport::HeartbeatAndAlertComponent,
        transport::ClientSideSimpleIdentityAttacherComponent<std::string, GS::Input>
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    env.transport::ClientSideSimpleIdentityAttacherComponent<std::string,GS::Input>::operator=(
        transport::ClientSideSimpleIdentityAttacherComponent<std::string,GS::Input>(
            "main_request_generator"
        )
    ); 
    R r(&env);

    env.setLogFilePrefix("simple_demo_chain_version_main_logic_request_generator_");

    //setting up the heartbeat

    transport::initializeHeartbeatAndAlertComponent
        (&env, "simple_demo_chain_version MainLogic Request Generator", "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]");
    env.setStatus("program", transport::HeartbeatMessage::Status::Good);
    transport::attachHeartbeatAndAlertComponent(r, &env, "simple_demo_chain_version.main_logic.heartbeat", std::chrono::seconds(1));

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

    auto heartbeatSharer = 
        infra::KleisliUtils<M>::action(
            basic::CommonFlowUtilComponents<M>::shareBetweenDownstream<transport::HeartbeatMessage>()
        );
    r.registerAction("heartbeatSharer", heartbeatSharer);
    auto sharedHeartbeatSource = r.execute(heartbeatSharer, heartbeatSource.clone());

    auto inputDataSource = transport::MultiTransportBroadcastListenerManagingUtils<R>
        ::setupBroadcastListenerThroughHeartbeat<InputData>
    (
        r 
        , sharedHeartbeatSource.clone()
        , std::regex("simple_demo_chain_version DataSource")
        , "input data publisher"
        , "input.data"
        , "inputDataSourceComponents"
    );

    auto requestPlacer = transport::MultiTransportRemoteFacilityManagingUtils<R>
        ::setupOneNonDistinguishedRemoteFacility<basic::CBOR<double>, basic::CBOR<std::optional<ChainData>>>
        (
            r 
            , sharedHeartbeatSource.clone()
            , std::regex("simple_demo_chain_version MainLogic Request Placer")
            , "main_program/facility_combo/facility"
        ).facility;

    auto exitDataSource = transport::ExitDataSourceCreator::addExitDataSource(
        r, "onExit"
    );
    auto enableServerSubscriber = transport::RemoteTransactionSubscriberManagingUtils<R>
        ::createSubscriber<GS>
        (
            r 
            , sharedHeartbeatSource.clone()
            , std::regex("simple_demo_chain_version Enable Server")
            , "transaction_server_components/subscription_handler"
            , GS::Subscription {{basic::VoidStruct {}}}
            , {std::move(exitDataSource)}
        );
    //for some reason gcc requires the explicit parameter specification for GS::Input
    //but clang does not require that.
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

    //main logic
    main_program_logic::mainProgramLogicMain(
        r
        , basic::WrapFacilitioidConnectorForSerialization<R>::wrapClientSide<
            double, std::optional<ChainData>
        >(requestPlacer, "remote_facility_wrapper")
        , inputDataSource.clone()
        , r.actionAsSource(convertToBool)
        , "main_program"
    );

    //write execution graph and start

    std::ostringstream graphOss;
    r.writeGraphVizDescription(graphOss, "simple_demo_chain_version_main_request_generator");
    env.log(infra::LogLevel::Info, "The execution graph is:");
    env.log(infra::LogLevel::Info, graphOss.str());

    r.finalize();

    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });

    return 0;
}