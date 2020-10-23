#include "simple_demo_chain_version/main_program_logic/MainProgramLogicProvider.hpp"
#include "simple_demo_chain_version/calculator_logic/CalculatorLogicProvider.hpp"
#include "simple_demo_chain_version/calculator_logic/MockExternalCalculator.hpp"

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/SinglePassIterationApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockOnOrderFacility.hpp>
#include <tm_kit/basic/ByteDataWithTopicRecordFileImporterExporter.hpp>
#include <tm_kit/basic/AppRunnerUtils.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/lock_free_in_memory_shared_chain/LockFreeInMemoryChain.hpp>

#include <iostream>
#include <fstream>

using namespace dev::cd606::tm;
using namespace simple_demo_chain_version;

using TheEnvironment = infra::Environment<
    infra::CheckTimeComponent<true>,
    infra::TrivialExitControlComponent,
    basic::TimeComponentEnhancedWithSpdLogging<
        basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>
        , false //don't log thread ID
    >,
    transport::CrossGuidComponent
>;
using Chain = transport::lock_free_in_memory_shared_chain::LockFreeInMemoryChain<
    ChainData
>;
using M = infra::SinglePassIterationApp<TheEnvironment>;
using R = infra::AppRunner<M>;

void run(std::string const &inputFile) {
    TheEnvironment env;
    R r(&env);
    std::ifstream ifs(inputFile.c_str());
    Chain theChain;

    auto byteDataImporter = basic::ByteDataWithTopicRecordFileImporterExporter<M>::createImporter<basic::ByteDataWithTopicRecordFileFormat<std::chrono::microseconds>>(
        ifs
        , {(std::byte) 0x01,(std::byte) 0x23,(std::byte) 0x45,(std::byte) 0x67}
        , {(std::byte) 0x76,(std::byte) 0x54,(std::byte) 0x32,(std::byte) 0x10}
    );
    auto parser = basic::SerializationActions<M>::deserialize<InputData>();
    auto removeTopic = basic::SerializationActions<M>::removeTopic<InputData>();
    r.registerImporter("byteDataImporter", byteDataImporter);
    r.registerAction("parser", parser);
    r.registerAction("removeTopic", removeTopic);

    auto inputDataSource = r.execute(removeTopic, r.execute(parser, r.importItem(byteDataImporter)));

    main_program_logic::mainProgramLogicMain(
        r
        , &theChain
        , inputDataSource.clone()
        , std::nullopt
        , "main_program"
    );
    
    basic::AppRunnerUtilComponents<R>
        ::setupExitTimer(
        r 
        , std::chrono::hours(24)
        , inputDataSource.clone()
        , [](TheEnvironment *env) {
            env->log(infra::LogLevel::Info, "Wrapping up!");
        }
        , "exitTimerPart"
    );

    auto mockExternalFacility = calculator_logic::MockExternalCalculator<
        R, basic::single_pass_iteration_clock::ClockOnOrderFacility<TheEnvironment>
    >::connector("mockExternalFacility");
    auto calculatorLogicMainRes = calculator_logic::calculatorLogicMain(
        r
        , &theChain
        , mockExternalFacility
        , "calculator"
    );

    //we don't need to print the calculator's results (since the main
    //logic already prints the updates, so we just discard them)
    auto discardCalculatorChainDataInfo = M::trivialExporter<ChainData>();
    r.registerExporter("discard", discardCalculatorChainDataInfo);
    r.exportItem(discardCalculatorChainDataInfo, calculatorLogicMainRes.chainDataGeneratedFromCalculator.clone());
    
    r.finalize();
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: single_pass FILE_NAME\n";
        return 1;
    }
    std::string dataSourceCaptureFileName = argv[1];
    run(dataSourceCaptureFileName);
    return 0;
}