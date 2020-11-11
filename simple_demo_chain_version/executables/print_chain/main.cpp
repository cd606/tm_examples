#include "simple_demo_chain_version/main_program_logic/MainProgramLogicProvider.hpp"
#include "simple_demo_chain_version/security_keys/VerifyingKeys.hpp"

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
#include <tm_kit/transport/security/SignatureAndVerifyHookFactoryComponents.hpp>

#include <iostream>
#include <fstream>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

using namespace dev::cd606::tm;
using namespace simple_demo_chain_version;
using namespace boost::program_options;

template <class Env>
void run(Env *env, std::string const &chainLocatorStr, std::string const &outputFile) {
    using M = infra::SinglePassIterationApp<Env>;
    using R = infra::AppRunner<M>;
    R r(env);

    transport::SharedChainCreator<M> sharedChainCreator;
    auto chainDataImporter = sharedChainCreator.template reader<
        ChainData
        , main_program_logic::TrivialChainDataFolder
    >(
        env
        , chainLocatorStr
    );
    r.registerImporter("chainDataImporter", chainDataImporter);

    basic::AppRunnerUtilComponents<R>
        ::setupExitTimer(
        r 
        , std::chrono::hours(24)
        , r.importItem(chainDataImporter)
        , [](Env *env) {
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
            ::template createExporter<basic::ByteDataWithTopicRecordFileFormat<std::chrono::microseconds>>(
                ofs
                , {(std::byte) 0x01,(std::byte) 0x23,(std::byte) 0x45,(std::byte) 0x67}
                , {(std::byte) 0x76,(std::byte) 0x54,(std::byte) 0x32,(std::byte) 0x10}
            );
        r.registerExporter("fileWriter", fileWriter);
        auto encoder = M::template liftPure<ChainData>(
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
        auto printer = M::template pureExporter<ChainData>(
            [env](ChainData &&d) {
                std::ostringstream oss;
                oss << d;
                env->log(infra::LogLevel::Info, oss.str());
            }
        );
        r.registerExporter("printer", printer);
        r.exportItem(printer, r.execute(simpleFilter, r.importItem(chainDataImporter)));
        r.finalize();
    }
}

void runSigned(std::string const &chainLocatorStr, std::string const &outputFile) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<
            basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>
            , false //don't log thread ID
        >,
        transport::CrossGuidComponent,
        transport::AllChainComponents,
        transport::TrivialOutgoingHookFactoryComponent<ChainData>,
        transport::security::VerifyUsingNameTagHookFactoryComponent<ChainData>
    >;

    TheEnvironment env;
    env.transport::security::VerifyUsingNameTagHookFactoryComponent<ChainData>::operator=(
        transport::security::VerifyUsingNameTagHookFactoryComponent<ChainData> {
            verifyingKeys
        }
    );

    run<TheEnvironment>(&env, chainLocatorStr, outputFile); 
}

void runPlain(std::string const &chainLocatorStr, std::string const &outputFile) {
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

    TheEnvironment env;

    run<TheEnvironment>(&env, chainLocatorStr, outputFile); 
}

int main(int argc, char **argv) {
    options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("chain", value<std::string>(), "the locator string of the chain to print")
        ("outputFile", value<std::string>(), "the file to write to (if not given, then print on terminal")
        ("chainIsSigned", "whether chain is signed")
    ;
    variables_map vm;
    store(parse_command_line(argc, argv, desc), vm);
    notify(vm);

    if (vm.count("help")) {
        std::cout << desc << '\n';
        return 0;
    }
    if (!vm.count("chain")) {
        std::cerr << "Please provide chain to be printed\n";
        return 1;
    }
    auto chainLocatorStr = vm["chain"].as<std::string>();
    std::string outputFile = "";
    if (vm.count("outputFile")) {
        outputFile = vm["outputFile"].as<std::string>();
    }
    bool chainIsSigned = vm.count("chainIsSigned");

    if (chainIsSigned) {
        runSigned(chainLocatorStr, outputFile);
    } else {
        runPlain(chainLocatorStr, outputFile);
    }
    return 0;
}