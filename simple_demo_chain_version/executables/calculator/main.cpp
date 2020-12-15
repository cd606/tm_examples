#include "simple_demo_chain_version/calculator_logic/CalculatorLogicProvider.hpp"
#include "simple_demo_chain_version/calculator_logic/ExternalCalculatorWrappedAsFacility.hpp"
#include "simple_demo_chain_version/calculator_logic/MockExternalCalculator.hpp"
#include "simple_demo_chain_version/security_keys/VerifyingKeys.hpp"

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
#include <tm_kit/transport/security/SignatureAndVerifyHookFactoryComponents.hpp>

using namespace simple_demo_chain_version;

int main(int argc, char **argv) {
    const transport::security::SignatureHelper::PrivateKey calculatorKey = {
        0x89,0xE9,0x4A,0xDF,0x38,0xA4,0x35,0xD2,0xD2,0x89,0x00,0xC1,0x7B,0xBA,0x7F,0x68,
        0x30,0xFD,0x9B,0x69,0xDB,0x6F,0xCE,0x49,0x16,0x3E,0xE6,0x30,0xCF,0xC0,0xA8,0xD7,
        0x9B,0x39,0xEE,0x8A,0xAB,0x24,0xD3,0x6B,0xFB,0xF2,0x79,0xC0,0x60,0x32,0x6D,0xF6,
        0x1E,0xA2,0x68,0x1D,0xDD,0x70,0x45,0x78,0x53,0xB7,0x12,0x38,0x10,0x13,0xED,0xAD
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
            "calculator"
            , calculatorKey
        }
    );
    env.transport::security::VerifyUsingNameTagHookFactoryComponent<ChainData>::operator=(
        transport::security::VerifyUsingNameTagHookFactoryComponent<ChainData> {
            verifyingKeys
        }
    );
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
