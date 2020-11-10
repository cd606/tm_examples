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

void run(std::string const &inputChainLocatorStr, std::string const &outputChainLocatorStr) {
    TheEnvironment env;
    R r(&env);

    transport::SharedChainCreator<M> sharedChainCreator;

    bool inputIsCaptureFile = (inputChainLocatorStr.find("://") == std::string::npos);
    R::Sourceoid<ChainData> chainDataSource;

    if (inputIsCaptureFile) {
        auto ifs = std::make_shared<std::ifstream>(inputChainLocatorStr.c_str(), std::ios::binary);
        r.preservePointer(ifs);
        auto byteDataImporter = basic::ByteDataWithTopicRecordFileImporterExporter<M>::createImporter<basic::ByteDataWithTopicRecordFileFormat<std::chrono::microseconds>>(
            *ifs
            , {(std::byte) 0x01,(std::byte) 0x23,(std::byte) 0x45,(std::byte) 0x67}
            , {(std::byte) 0x76,(std::byte) 0x54,(std::byte) 0x32,(std::byte) 0x10}
        );
        auto parser = basic::SerializationActions<M>::deserialize<ChainData>();
        auto removeTopic = basic::SerializationActions<M>::removeTopic<ChainData>();
        chainDataSource = R::sourceAsSourceoid(
            r.execute("removeTopic", removeTopic
                , r.execute("parser", parser
                    , r.importItem("byteDataImporter", byteDataImporter)))
        );
    } else {
        auto chainDataImporter = sharedChainCreator.reader<
            ChainData
            , main_program_logic::TrivialChainDataFolder
        >(
            &env
            , inputChainLocatorStr
        );
        auto simpleFilter = infra::KleisliUtils<M>::action(
            basic::CommonFlowUtilComponents<M>::template filterOnOptional<ChainData>()
        );
        chainDataSource = R::sourceAsSourceoid(
            r.execute("simpleFilter", simpleFilter
                , r.importItem("chainDataImporter", chainDataImporter))
        );
    }

    auto keyify = infra::KleisliUtils<M>::action(
        basic::CommonFlowUtilComponents<M>::template keyify<ChainData>()
    );
    r.registerAction("keyify", keyify);
    r.connect(chainDataSource, r.actionAsSink(keyify));

    auto chainDataWriter = sharedChainCreator.writer<
        ChainData
        , basic::simple_shared_chain::EmptyStateChainFolder
        , basic::simple_shared_chain::SimplyPlaceOnChainInputHandler<ChainData>
    >(&env, outputChainLocatorStr);

    r.registerOnOrderFacility("chainDataWriter", chainDataWriter);

    auto trivialExporter = M::trivialExporter<M::KeyedData<ChainData,bool>>();
    r.registerExporter("trivialExporter", trivialExporter);

    r.placeOrderWithFacility(
        r.actionAsSource(keyify)
        , chainDataWriter
        , r.exporterAsSink(trivialExporter)
    );

    basic::AppRunnerUtilComponents<R>
        ::setupExitTimer(
        r 
        , std::chrono::hours(24)
        , r.actionAsSource(keyify)
        , [](TheEnvironment *env) {
            env->log(infra::LogLevel::Info, "Wrapping up!");
        }
        , "exitTimerPart"
    );
    r.finalize();
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: transcribe_chain INPUT_CHAIN_LOCATOR OUTPUT_CHAIN_LOCATOR\n";
        std::cerr << "If INPUT_CHAIN_LOCATOR does not contain \"://\", it is treated as a capture file name\n";
        return 1;
    }
    std::string inputChainLocatorStr = argv[1];
    std::string outputChainLocatorStr = argv[2];
    run(inputChainLocatorStr, outputChainLocatorStr);
    return 0;
}