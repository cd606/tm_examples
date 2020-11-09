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

class EmptyStateFolder {
public:
    using ResultType = basic::VoidStruct;
    template <class Chain>
    static ResultType initialize(TheEnvironment *, Chain *) {
        return ResultType {};
    }
    static void foldInPlace(ResultType &res, std::string_view const &storageIDView, ChainData const *item) {
    }
};

class TranscriptionInputHandler {
public:
    using InputType = ChainData;
    using ResponseType = bool;
    template <class Chain>
    static void initialize(TheEnvironment *, Chain *) {}
    template <class Chain>
    static std::tuple<ResponseType, std::optional<std::tuple<std::string, ChainData>>>
    handleInput(TheEnvironment *env, Chain *chain, M::TimedDataType<M::Key<ChainData>> &&input, basic::VoidStruct const &state) {
        return {true, {{"", input.value.key()}}};
    }
};

void run(std::string const &inputChainLocatorStr, std::string const &outputChainLocatorStr) {
    TheEnvironment env;
    R r(&env);

    transport::SharedChainCreator<M> sharedChainCreator;
    auto chainDataImporter = sharedChainCreator.reader<
        ChainData
        , main_program_logic::TrivialChainDataFolder
    >(
        &env
        , inputChainLocatorStr
    );
    r.registerImporter("chainDataImporter", chainDataImporter);

    auto simpleFilter = infra::KleisliUtils<M>::action(
        basic::CommonFlowUtilComponents<M>::template filterOnOptional<ChainData>()
    );
    r.registerAction("simpleFilter", simpleFilter);
    auto keyify = infra::KleisliUtils<M>::action(
        basic::CommonFlowUtilComponents<M>::template keyify<ChainData>()
    );
    r.registerAction("keyify", keyify);

    auto chainDataWriter = sharedChainCreator.writer<
        ChainData
        , EmptyStateFolder
        , TranscriptionInputHandler
    >(outputChainLocatorStr);

    r.registerOnOrderFacility("chainDataWriter", chainDataWriter);

    auto trivialExporter = M::trivialExporter<M::KeyedData<ChainData,bool>>();
    r.registerExporter("trivialExporter", trivialExporter);

    r.placeOrderWithFacility(
        r.execute(keyify, r.execute(simpleFilter, r.importItem(chainDataImporter)))
        , chainDataWriter
        , r.exporterAsSink(trivialExporter)
    );

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
    r.finalize();
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: transcribe_chain INPUT_CHAIN_LOCATOR OUTPUT_CHAIN_LOCATOR\n";
        return 1;
    }
    std::string inputChainLocatorStr = argv[1];
    std::string outputChainLocatorStr = argv[2];
    run(inputChainLocatorStr, outputChainLocatorStr);
    return 0;
}