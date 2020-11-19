#include "simple_demo_chain_version/main_program_logic/MainProgramLogicProvider.hpp"

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/AppRunnerUtils.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListenerManagingUtils.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#ifdef _MSC_VER
//Visual C++ does not like void appearing in std::tuple, which means
//some heavy template code for multi-transport client side facility creator
//will fail if there is no identity attacher for the facility.
//Before the template code can be made MSVC-compliant, the easiest way
//is just to put an empty identity attacher in
#include <tm_kit/transport/EmptyIdentityCheckerComponent.hpp>
#endif

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
        transport::AllNetworkTransportComponents,
        transport::HeartbeatAndAlertComponent
#ifdef _MSC_VER
        , transport::ClientSideEmptyIdentityAttacherComponent<basic::VoidStruct, basic::CBOR<double>>
#endif
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
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

    auto requestPlacer = transport::MultiTransportRemoteFacilityManagingUtils<R>
        ::setupOneNonDistinguishedRemoteFacility<basic::CBOR<double>, basic::CBOR<std::optional<ChainData>>>
        (
            r 
            , heartbeatSource.clone()
            , std::regex("simple_demo_chain_version MainLogic Request Placer")
            , "main_program/facility_combo/facility"
        );

    //main logic
    main_program_logic::mainProgramLogicMain(
        r
        , basic::AppRunnerUtilComponents<R>::wrapFacilitioidConnector<
            double, std::optional<ChainData>, basic::CBOR<double>, basic::CBOR<std::optional<ChainData>>
        >(
            infra::KleisliUtils<M>::liftPure<double>(
                [](double &&x) -> basic::CBOR<double> {
                    return {x};
                }
            )
            , infra::KleisliUtils<M>::liftPure<basic::CBOR<double>>(
                [](basic::CBOR<double> &&x) -> double {
                    return x.value;
                }
            )
            , infra::KleisliUtils<M>::liftPure<basic::CBOR<std::optional<ChainData>>>(
                [](basic::CBOR<std::optional<ChainData>> &&x) -> std::optional<ChainData> {
                    return std::move(x.value);
                }
            )
            , requestPlacer 
            , "remote_facility_wrapper"
        )
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
    r.writeGraphVizDescription(graphOss, "simple_demo_chain_version_main_request_generator");
    env.log(infra::LogLevel::Info, "The execution graph is:");
    env.log(infra::LogLevel::Info, graphOss.str());

    r.finalize();

    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });

    return 0;
}