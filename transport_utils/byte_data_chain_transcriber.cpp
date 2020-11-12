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
using namespace boost::program_options;

class TrivialByteDataChainFolder : public basic::simple_shared_chain::TrivialChainDataFetchingFolder<basic::ByteData> {
private:
    std::chrono::system_clock::time_point tp_;
public:
    std::chrono::system_clock::time_point extractTime(std::optional<basic::ByteData> const &st) {
        if (st) {
            tp_ += std::chrono::microseconds(1);
        }
        return tp_;
    }
};

template <class R>
auto transcribe(R &r, std::string const &inputChainLocatorStr, std::string const &outputChainLocatorStr, bool inputIsFile, bool outputIsFile) 
    -> typename R::template Source<basic::ByteData>
{
    using M = typename R::AppType;
    using Env = typename M::EnvironmentType;

    Env *env = r.environment();

    auto sharedChainCreator = std::make_shared<transport::SharedChainCreator<M>>();
    r.preservePointer(sharedChainCreator);

    std::optional<typename R::template Source<basic::ByteData>> chainDataSource = std::nullopt;

    if (inputIsFile) {
        auto ifs = std::make_shared<std::ifstream>(inputChainLocatorStr.c_str(), std::ios::binary);
        r.preservePointer(ifs);
        auto byteDataImporter = basic::ByteDataWithTopicRecordFileImporterExporter<M>::template createImporter<basic::ByteDataWithTopicRecordFileFormat<std::chrono::microseconds>>(
            *ifs
            , {(std::byte) 0x01,(std::byte) 0x23,(std::byte) 0x45,(std::byte) 0x67}
            , {(std::byte) 0x76,(std::byte) 0x54,(std::byte) 0x32,(std::byte) 0x10}
        );
        auto parser = basic::SerializationActions<M>::template deserialize<basic::ByteData>();
        auto removeTopic = M::template liftPure<basic::ByteDataWithTopic>(
            [](basic::ByteDataWithTopic &&d) {
                return basic::ByteData {std::move(d.content)};
            }
        );
        chainDataSource = 
            r.execute("removeTopic", removeTopic
                , r.importItem("byteDataImporter", byteDataImporter));
    } else {
        auto chainDataImporter = sharedChainCreator->template reader<
            basic::ByteData
            , TrivialByteDataChainFolder
            , void //no trigger type
            , true //force separate data storage if possible
        >(
            env
            , inputChainLocatorStr
        );
        auto simpleFilter = infra::KleisliUtils<M>::action(
            basic::CommonFlowUtilComponents<M>::template filterOnOptional<basic::ByteData>()
        );
        chainDataSource = 
            r.execute("simpleFilter", simpleFilter
                , r.importItem("chainDataImporter", chainDataImporter));
    }

    if (outputIsFile) {
        auto ofs = std::make_shared<std::ofstream>(outputChainLocatorStr.c_str(), std::ios::binary);
        r.preservePointer(ofs);
        auto fileWriter = basic::ByteDataWithTopicRecordFileImporterExporter<M>
            ::template createExporter<basic::ByteDataWithTopicRecordFileFormat<std::chrono::microseconds>>(
                *ofs
                , {(std::byte) 0x01,(std::byte) 0x23,(std::byte) 0x45,(std::byte) 0x67}
                , {(std::byte) 0x76,(std::byte) 0x54,(std::byte) 0x32,(std::byte) 0x10}
            );
        r.registerExporter("fileWriter", fileWriter);
        auto addTopic = M::template liftPure<basic::ByteData>(
            [](basic::ByteData &&d) -> basic::ByteDataWithTopic {
                return {
                    "chain.data"
                    , std::move(d.content)
                };
            }
        );
        r.registerAction("addTopic", addTopic);
        r.exportItem(fileWriter, r.execute(addTopic, chainDataSource->clone()));
    } else {
        auto keyify = infra::KleisliUtils<M>::action(
            basic::CommonFlowUtilComponents<M>::template keyify<basic::ByteData>()
        );
        r.registerAction("keyify", keyify);
        r.execute(keyify, chainDataSource->clone());

        auto chainDataWriter = sharedChainCreator->template writer<
            basic::ByteData
            , basic::simple_shared_chain::EmptyStateChainFolder
            , basic::simple_shared_chain::SimplyPlaceOnChainInputHandler<basic::ByteData>
            , void //no idle logic
            , true //force separate data storage if possible
        >(env, outputChainLocatorStr);

        r.registerOnOrderFacility("chainDataWriter", chainDataWriter);

        auto trivialExporter = M::template trivialExporter<typename M::template KeyedData<basic::ByteData,bool>>();
        r.registerExporter("trivialExporter", trivialExporter);

        r.placeOrderWithFacility(
            r.actionAsSource(keyify)
            , chainDataWriter
            , r.exporterAsSink(trivialExporter)
        );
    }

    return chainDataSource->clone();
}

void runRT(std::string const &inputChainLocatorStr, std::string const &outputChainLocatorStr) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<
            basic::real_time_clock::ClockComponent
        >,
        transport::CrossGuidComponent,
        transport::AllChainComponents
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    R r(&env);
    transcribe<R>(r, inputChainLocatorStr, outputChainLocatorStr, false, false);

    r.finalize();
    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });
}

void runSP(std::string const &inputChainLocatorStr, std::string const &outputChainLocatorStr, bool inputIsFile, bool outputIsFile) {
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

    TheEnvironment env;
    R r(&env);

    auto src = transcribe<R>(r, inputChainLocatorStr, outputChainLocatorStr, inputIsFile, outputIsFile);

    basic::AppRunnerUtilComponents<R>
        ::setupExitTimer(
        r 
        , std::chrono::hours(24)
        , std::move(src)
        , [](TheEnvironment *env) {
            env->log(infra::LogLevel::Info, "Wrapping up!");
        }
        , "exitTimerPart"
    );
    r.finalize();
}

void run(std::string const &inputChainLocatorStr, std::string const &outputChainLocatorStr, bool realTimeMode) {
    bool inputIsFile = (inputChainLocatorStr.find("://") == std::string::npos);
    bool outputIsFile = (outputChainLocatorStr.find("://") == std::string::npos);
    if (inputIsFile && outputIsFile) {
        std::cout << "at least one of the input and output needs to be a chain\n";
        return;
    }

    bool actuallyRunInRealTimeMode = realTimeMode && !inputIsFile && !outputIsFile;

    if (actuallyRunInRealTimeMode) {
        runRT(inputChainLocatorStr, outputChainLocatorStr);
    } else {
        runSP(inputChainLocatorStr, outputChainLocatorStr, inputIsFile, outputIsFile);
    }
}

int main(int argc, char **argv) {
    options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("input", value<std::string>(), "a chain locator or a capture file name, if it does not contain \"://\", it is a capture file name")
        ("output", value<std::string>(), "a chain locator or a capture file name, if it does not contain \"://\", it is a capture file name")
        ("realTimeMode", "runs in real time mode (only meaningful if both input and output are chains)")
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
    bool realTimeMode = vm.count("realTimeMode");
    
    run(inputChainLocatorStr, outputChainLocatorStr, realTimeMode);

    return 0;
}