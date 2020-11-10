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
        transport::rabbitmq::RabbitMQComponent,
        transport::HeartbeatAndAlertComponent,
        transport::lock_free_in_memory_shared_chain::SharedMemoryChainComponent
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

    std::string today = infra::withtime_utils::localTimeString(std::chrono::system_clock::now()).substr(0,10);
    std::ostringstream chainLocatorOss;
    chainLocatorOss << "in_shared_memory://::::" << today << "-simple-demo-chain[size=" << (100*1024*1024) << "]";

    //Please note that this object should not be allowed to go out of scope
    transport::SharedChainCreator<M> sharedChainCreator;

    auto chainFacilityFactory = sharedChainCreator.writerFactory<
        ChainData
        , calculator_logic::CalculatorStateFolder
        , calculator_logic::CalculatorFacilityInputHandler
        , calculator_logic::CalculatorIdleWorker
    >(
        &env
        , chainLocatorOss.str()
        //If very high throughput is required, then we need to use the busy-loop no-yield polling 
        //policy which will occupy full CPU (in real-time mode). If default polling policy is used
        //, then there will be a sleep of at least 1 millisecond (and most likely longer) between 
        //the polling, so the throughput will be degraded. In single-pass mode, the polling policy
        //is ignored since it is single-threaded and always uses busy polling.
        //, basic::simple_shared_chain::ChainPollingPolicy().BusyLoop(true).NoYield(true)
    );

    auto wrappedExternalFacility = M::fromAbstractOnOrderFacility(new calculator_logic::ExternalCalculatorWrappedAsFacility<TheEnvironment>());
    r.registerOnOrderFacility("wrappedExternalFacility", wrappedExternalFacility);
    /*
    auto mockExternalFacility = calculator_logic::MockExternalCalculator<
        R, basic::real_time_clock::ClockOnOrderFacility<TheEnvironment>
    >::connector("mockExternalFacility");
    */
    auto calculatorLogicMainRes = calculator_logic::calculatorLogicMain(
        r
        , chainFacilityFactory
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