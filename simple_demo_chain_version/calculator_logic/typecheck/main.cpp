#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/BasicWithTimeApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/IntIDComponent.hpp>
#include <tm_kit/basic/empty_clock/ClockComponent.hpp>
#include <tm_kit/basic/empty_clock/ClockOnOrderFacility.hpp>
#include <tm_kit/basic/simple_shared_chain/BasicWithTimeAppChainFactories.hpp>

#include "simple_demo_chain_version/calculator_logic/CalculatorLogicProvider.hpp"

using namespace dev::cd606::tm;
using namespace simple_demo_chain_version;

int main() {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<
            basic::empty_clock::ClockComponent<>
        >,
        basic::IntIDComponent<>
    >;
    using M = infra::BasicWithTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    R r(&env);

    auto facility = M::emptyOnOrderFacility<ExternalCalculatorInput,ExternalCalculatorOutput>();
    r.registerOnOrderFacility("facility", facility);

    auto calculatorLogicMainRes = calculator_logic::calculatorLogicMain(
        r
        , basic::simple_shared_chain::BasicWithTimeAppChainFactories<TheEnvironment>
            ::chainWriterOnOrderFacilityWithExternalEffectsFactory<
                calculator_logic::CalculatorStateFolder
                , calculator_logic::CalculatorFacilityInputHandler
                , calculator_logic::CalculatorIdleWorker
            >()
        , R::facilityConnector(facility)
        , "calculator"
    );

    auto theExporter = M::trivialExporter<ChainData>();
    r.registerExporter("theExporter", theExporter);
    r.exportItem(theExporter, calculatorLogicMainRes.chainDataGeneratedFromCalculator.clone());
    
    //write execution graph and start

    std::ostringstream graphOss;
    r.writeGraphVizDescription(graphOss, "simple_demo_chain_version_calculator_typecheck");
    env.log(infra::LogLevel::Info, "The execution graph is:\n"+graphOss.str());

    r.finalize();

    infra::terminationController(infra::ImmediatelyTerminate {});

    return 0;
}