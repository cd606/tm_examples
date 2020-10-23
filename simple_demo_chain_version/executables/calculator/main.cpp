#include "simple_demo_chain_version/calculator_logic/CalculatorLogicProvider.hpp"
#include "simple_demo_chain_version/calculator_logic/ExternalCalculatorWrappedAsFacility.hpp"
#include "simple_demo_chain_version/calculator_logic/MockExternalCalculator.hpp"

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockOnOrderFacility.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>
#include <tm_kit/transport/lock_free_in_memory_shared_chain/LockFreeInBoostSharedMemoryChain.hpp>

using namespace simple_demo_chain_version;

int main(int argc, char **argv) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<
            basic::real_time_clock::ClockComponent
        >,
        transport::CrossGuidComponent,
        transport::rabbitmq::RabbitMQComponent,
        transport::HeartbeatAndAlertComponent
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    R r(&env);

    env.setLogFilePrefix("simple_demo_chain_version_calculator_");

    //setting up the heartbeat

    transport::initializeHeartbeatAndAlertComponent
        (&env, "simple_demo_chain_version Calculator", "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]");
    env.setStatus("program", transport::HeartbeatMessage::Status::Good);
    transport::attachHeartbeatAndAlertComponent(r, &env, "simple_demo_chain_version.calculator.heartbeat", std::chrono::seconds(1));

    //setting up the chain

    using Chain = transport::lock_free_in_memory_shared_chain::LockFreeInBoostSharedMemoryChain<
        ChainData
        , transport::lock_free_in_memory_shared_chain::BoostSharedMemoryChainFastRecoverSupport::ByOffset
        , transport::lock_free_in_memory_shared_chain::BoostSharedMemoryChainExtraDataProtectionStrategy::MutexProtected
    >;
    std::string today = infra::withtime_utils::localTimeString(std::chrono::system_clock::now()).substr(0,10);
    auto chainName = today+"-simple-demo-chain";
    Chain theChain(chainName, 100*1024*1024);

    auto wrappedExternalFacility = M::fromAbstractOnOrderFacility(new calculator_logic::ExternalCalculatorWrappedAsFacility<TheEnvironment>());
    r.registerOnOrderFacility("wrappedExternalFacility", wrappedExternalFacility);
    /*
    auto mockExternalFacility = calculator_logic::MockExternalCalculator<
        R, basic::real_time_clock::ClockOnOrderFacility<TheEnvironment>
    >::connector("mockExternalFacility");
    */
    auto calculatorLogicMainRes = calculator_logic::calculatorLogicMain(
        r
        , &theChain
        , R::facilityConnector(wrappedExternalFacility)
        //, mockExternalFacility
        , "calculator"
    );

    //add a printer for the results
    auto printExporter = M::pureExporter<ChainData>(
        [&env](ChainData &&chainData) {
            std::ostringstream oss;
            oss << "Created chain action " << chainData;
            env.log(infra::LogLevel::Info, oss.str());
        }
    );
    r.registerExporter("printExporter", printExporter);
    r.exportItem(printExporter, calculatorLogicMainRes.chainDataGeneratedFromCalculator.clone());
    
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