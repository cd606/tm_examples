#include "simple_demo_chain_version/main_program_logic/MainProgramLogicProvider.hpp"

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/SinglePassIterationApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockOnOrderFacility.hpp>
#include <tm_kit/basic/ByteDataWithTopicRecordFileImporterExporter.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>
#include <tm_kit/basic/AppRunnerUtils.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/SharedChainCreator.hpp>

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
    transport::CrossGuidComponent,
    transport::AllChainComponents
>;
using M = infra::SinglePassIterationApp<TheEnvironment>;
using R = infra::AppRunner<M>;

void run(std::string const &chainLocatorStr, std::string const &outputFile) {
    TheEnvironment env;
    R r(&env);

    transport::SharedChainCreator<M> sharedChainCreator;
    auto chainDataImporter = sharedChainCreator.reader<
        ChainData
        , main_program_logic::TrivialChainDataFolder
    >(
        &env
        , chainLocatorStr
    );
    r.registerImporter("chainDataImporter", chainDataImporter);

    basic::AppRunnerUtilComponents<R>
        ::setupExitTimer(
        r 
        , std::chrono::hours(24)
        , r.importItem(chainDataImporter)
        , [](TheEnvironment *env) {
            env->log(infra::LogLevel::Info, "Wrapping up!");
        }
        , "exitTimerPart"
    );

    auto simpleFilter = infra::KleisliUtils<M>::action(
        basic::CommonFlowUtilComponents<M>::template filterOnOptional<ChainData>()
    );
    r.registerAction("simpleFilter", simpleFilter);

    if (outputFile != "") {
        std::ofstream ofs(outputFile.c_str(), std::ios::binary);
        auto fileWriter = basic::ByteDataWithTopicRecordFileImporterExporter<M>
            ::createExporter<basic::ByteDataWithTopicRecordFileFormat<std::chrono::microseconds>>(
                ofs
                , {(std::byte) 0x01,(std::byte) 0x23,(std::byte) 0x45,(std::byte) 0x67}
                , {(std::byte) 0x76,(std::byte) 0x54,(std::byte) 0x32,(std::byte) 0x10}
            );
        r.registerExporter("fileWriter", fileWriter);
        auto encoder = M::liftPure<ChainData>(
            [](ChainData &&d) -> basic::ByteDataWithTopic {
                return {
                    "chain.data"
                    , basic::bytedata_utils::RunSerializer<ChainData>::apply(std::move(d))
                };
            }
        );
        r.registerAction("encoder", encoder);
        r.exportItem(fileWriter, r.execute(encoder, r.execute(simpleFilter, r.importItem(chainDataImporter))));
        r.finalize();
    } else {
        auto printer = M::pureExporter<ChainData>(
            [&env](ChainData &&d) {
                std::ostringstream oss;
                oss << d;
                env.log(infra::LogLevel::Info, oss.str());
            }
        );
        r.registerExporter("printer", printer);
        r.exportItem(printer, r.execute(simpleFilter, r.importItem(chainDataImporter)));
        r.finalize();
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: print_chain CHAIN_LOCATOR [OUTPUT_FILE_NAME]\n";
        return 1;
    }
    std::string chainLocatorStr = argv[1];
    std::string outputFile = "";
    if (argc >= 3) {
        outputFile = argv[2];
    }
    run(chainLocatorStr, outputFile);
    return 0;
}