#include "simple_demo_chain_version/main_program_logic/MainProgramLogicProvider.hpp"
#include "simple_demo_chain_version/security_keys/VerifyingKeys.hpp"

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/SinglePassIterationApp.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockOnOrderFacility.hpp>
#include <tm_kit/basic/ByteDataWithTopicRecordFileImporterExporter.hpp>
#include <tm_kit/basic/simple_shared_chain/ChainDataImporterExporter.hpp>
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

template <class Env, template <class E> class App>
void runTranscription(Env *env, std::string const &inputChainLocatorStr, std::string const &outputChainLocatorStr) {
    using M = App<Env>;
    using R = infra::AppRunner<M>;

    R r(env);
    transport::SharedChainCreator<M> sharedChainCreator;

    bool inputIsCaptureFile = (inputChainLocatorStr.find("://") == std::string::npos);
    typename std::optional<typename R::template Source<ChainData>> chainDataSource = std::nullopt;

    if (inputIsCaptureFile) {
        auto ifs = std::make_shared<std::ifstream>(inputChainLocatorStr.c_str(), std::ios::binary);
        r.preservePointer(ifs);
        auto byteDataImporter = basic::ByteDataWithTopicRecordFileImporterExporter<M>::template createImporter<basic::ByteDataWithTopicRecordFileFormat<std::chrono::microseconds>>(
            *ifs
            , {(std::byte) 0x01,(std::byte) 0x23,(std::byte) 0x45,(std::byte) 0x67}
            , {(std::byte) 0x76,(std::byte) 0x54,(std::byte) 0x32,(std::byte) 0x10}
        );
        auto parser = basic::SerializationActions<M>::template deserialize<ChainData>();
        auto removeTopic = basic::SerializationActions<M>::template removeTopic<ChainData>();
        chainDataSource = r.execute("removeTopic", removeTopic
            , r.execute("parser", parser
                , r.importItem("byteDataImporter", byteDataImporter))
        );

        auto chainDataSink = basic::simple_shared_chain::createChainDataSink<
            R, ChainData
        >(
            r 
            , sharedChainCreator.template writerFactory<
                ChainData
                , basic::simple_shared_chain::EmptyStateChainFolder
                , basic::simple_shared_chain::SimplyPlaceOnChainInputHandler<ChainData>
            >(env, outputChainLocatorStr)
            , "output_chain"
        );

        r.connect(chainDataSource->clone(), chainDataSink);
    } else {
        chainDataSource = basic::simple_shared_chain::setupChainTranscriber<
            R, transport::SharedChainCreator, ChainData
        >(
            r 
            , sharedChainCreator
            , inputChainLocatorStr
            , outputChainLocatorStr
            , [](ChainData const &d) {
                return main_program_logic::TrivialChainDataFolder::extractTime({d});
            }
            , "transcriber"
        );
    }

    if constexpr (std::is_same_v<M, infra::SinglePassIterationApp<Env>>) {
        basic::AppRunnerUtilComponents<R>
            ::setupExitTimer(
            r 
            , std::chrono::hours(24)
            , chainDataSource->clone()
            , [](Env *env) {
                env->log(infra::LogLevel::Info, "Wrapping up!");
            }
            , "exitTimerPart"
        );
    }
    r.finalize();
    if constexpr (std::is_same_v<M, infra::RealTimeApp<Env>>) {
        infra::terminationController(infra::RunForever{});
    }
}

template <template <class E> class App>
void runSignedTranscription(std::string const &inputChainLocatorStr, std::string const &outputChainLocatorStr) {
    const transport::security::SignatureHelper::PrivateKey transcriptionKey = {
        0x72,0x6C,0xE8,0x00,0x79,0xB9,0x13,0xD9,0x9F,0xE7,0x95,0xC8,0xAD,0x50,0xBA,0xF9,
        0x94,0x0E,0x20,0xEE,0x1C,0xAD,0x64,0x48,0xDF,0xBB,0x64,0xFF,0x75,0x54,0x6B,0xD7,
        0x66,0x32,0x6E,0x9F,0x1E,0xC0,0x25,0x62,0x21,0x02,0x7C,0xF6,0xD7,0xAB,0xB7,0x55,
        0x84,0x23,0xFE,0xCF,0xAA,0x02,0x21,0x94,0x55,0x08,0x65,0x13,0x5B,0x3B,0x57,0xD6
    };

    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<
            basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>
            , false //don't log thread ID
        >,
        transport::CrossGuidComponent,
        transport::AllChainComponents,
        transport::security::SignatureWithNameHookFactoryComponent<ChainData>,
        transport::security::VerifyUsingNameTagHookFactoryComponent<ChainData>
    >;
    TheEnvironment env;
    env.transport::security::SignatureWithNameHookFactoryComponent<ChainData>::operator=(
        transport::security::SignatureWithNameHookFactoryComponent<ChainData> {
            "transcription"
            , transcriptionKey
        }
    );
    env.transport::security::VerifyUsingNameTagHookFactoryComponent<ChainData>::operator=(
        transport::security::VerifyUsingNameTagHookFactoryComponent<ChainData> {
            verifyingKeys
        }
    );

    runTranscription<TheEnvironment,App>(&env, inputChainLocatorStr, outputChainLocatorStr);
}

template <template <class E> class App>
void runPlainTranscription(std::string const &inputChainLocatorStr, std::string const &outputChainLocatorStr) {
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

    runTranscription<TheEnvironment,App>(&env, inputChainLocatorStr, outputChainLocatorStr);
}

int main(int argc, char **argv) {
    options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("input", value<std::string>(), "a chain locator or a capture file name, if it does not contain \"://\", it is a capture file name")
        ("output", value<std::string>(), "a chain locator")
        ("chainIsSigned", "whether chain is signed")
        ("realTimeMode", "whether to run in real-time mode")
    ;
    variables_map vm;
    store(parse_command_line(argc, argv, desc), vm);
    notify(vm);

    if (vm.count("help")) {
        std::cout << desc << '\n';
        return 0;
    }
    if (!vm.count("input")) {
        std::cerr << "Please provide input\n";
        return 1;
    }
    if (!vm.count("output")) {
        std::cerr << "Please provide output\n";
        return 1;
    }

    auto inputChainLocatorStr = vm["input"].as<std::string>();
    auto outputChainLocatorStr = vm["output"].as<std::string>();
    bool chainIsSigned = vm.count("chainIsSigned");
    bool realTimeMode = vm.count("realTimeMode");

    if (realTimeMode) {
        if (chainIsSigned) {
            runSignedTranscription<infra::RealTimeApp_T>(inputChainLocatorStr, outputChainLocatorStr);
        } else {
            runPlainTranscription<infra::RealTimeApp_T>(inputChainLocatorStr, outputChainLocatorStr);
        }
    } else {
        if (chainIsSigned) {
            runSignedTranscription<infra::SinglePassIterationApp_T>(inputChainLocatorStr, outputChainLocatorStr);
        } else {
            runPlainTranscription<infra::SinglePassIterationApp_T>(inputChainLocatorStr, outputChainLocatorStr);
        }
    }
    return 0;
}