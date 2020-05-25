#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/BasicWithTimeMonad.hpp>
#include <tm_kit/infra/RealTimeMonad.hpp>
#include <tm_kit/infra/SinglePassIterationMonad.hpp>
#include <tm_kit/infra/IntIDComponent.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockOnOrderFacility.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockOnOrderFacility.hpp>
#include <tm_kit/basic/ByteDataWithTopicRecordFileImporterExporter.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQImporterExporter.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQOnOrderFacility.hpp>
#include <tm_kit/transport/zeromq/ZeroMQComponent.hpp>
#include <tm_kit/transport/zeromq/ZeroMQImporterExporter.hpp>

#include "defs.pb.h"
#include "simple_demo/program_logic/MainLogic.hpp"
#include "simple_demo/monad_combination/MainLogicCombination.hpp"
#include "simple_demo/monad_combination/MockCalculatorCombination.hpp"

#include <boost/program_options.hpp>

#include <iostream>
#include <fstream>
#include <sstream>

using namespace dev::cd606::tm;
using namespace simple_demo;

void run_real_or_virtual(LogicChoice logicChoice, bool isReal, std::string const &calibrateTime, int calibrateAfter, double speed, std::optional<std::string> generateGraphOnlyWithThisFile) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::real_time_clock::ClockComponent>,
        transport::BoostUUIDComponent,
        transport::ClientSideSimpleIdentityAttacherComponent<std::string,CalculateCommand>,
        transport::ServerSideSimpleIdentityCheckerComponent<std::string,ConfigureCommand>,
        transport::ServerSideSimpleIdentityCheckerComponent<std::string,ClearCommands>,
        transport::rabbitmq::RabbitMQComponent,
        transport::zeromq::ZeroMQComponent
    >;
    using M = infra::RealTimeMonad<TheEnvironment>;
    using R = infra::MonadRunner<M>;

    TheEnvironment env;
    env.transport::ClientSideSimpleIdentityAttacherComponent<std::string,CalculateCommand>::operator=(
        transport::ClientSideSimpleIdentityAttacherComponent<std::string,CalculateCommand>(
            "my_identity"
        )
    );
    if (!isReal) {
        env.basic::real_time_clock::ClockComponent::operator=(
            basic::real_time_clock::ClockComponent(
                TheEnvironment::clockSettingsWithStartPointCorrespondingToNextAlignment(
                    calibrateAfter
                    , calibrateTime
                    , speed
                )
            )
        );
    }
    R r(&env);

    auto importer = transport::zeromq::ZeroMQImporterExporter<TheEnvironment>
                    ::createTypedImporter<InputData>(
        transport::ConnectionLocator::parse("localhost:12345")
        , "input.data"
    );
    auto removeTopic = M::liftPure<basic::TypedDataWithTopic<InputData>>(
        [](basic::TypedDataWithTopic<InputData> &&data) -> InputData {
            return data.content;
        }
    );
    
    r.registerImporter("importer", importer);
    r.registerAction("removeTopic", removeTopic);

    R::FacilitioidConnector<CalculateCommand,CalculateResult> calc;
    if (isReal) {
        auto facility = transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>
                    ::WithIdentity<std::string>::createTypedRPCOnOrderFacility<CalculateCommand, CalculateResult>(
                        transport::ConnectionLocator::parse("localhost::guest:guest:test_queue")
                    );
        r.registerOnOrderFacility("facility", facility);
        calc = R::facilityConnector(facility);
    } else {
        calc = &(MockCalculatorCombination<
                    R
                    ,basic::real_time_clock::ClockOnOrderFacility<TheEnvironment>
                >::service);
    }
    
    MainLogicInput<R> combinationInput {
        calc
        , transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment,true>::WithIdentity<std::string>::facilityWrapper
            <ConfigureCommand, ConfigureResult>(
            transport::ConnectionLocator::parse("localhost::guest:guest:test_config_queue")
            , "cfg_wrapper_"
            , std::nullopt //no hook
        )
        , transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment,true>::WithoutIdentity::facilityWrapper
            <OutstandingCommandsQuery,OutstandingCommandsResult>(
            transport::ConnectionLocator::parse("localhost::guest:guest:test_query_queue")
            , "query_wrapper_"
            , std::nullopt //no hook
        )
        , transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment,true>::WithIdentity<std::string>::facilityWrapper
            <ClearCommands,ClearCommandsResult>(
            transport::ConnectionLocator::parse("localhost::guest:guest:test_clear_queue")
            , "clear_cmd_wrapper_"
            , std::nullopt //no hook
        )
    };
    auto mainLogicOutput = MainLogicCombination(r, env, std::move(combinationInput), logicChoice);
    r.connect(r.execute(removeTopic, r.importItem(importer)), mainLogicOutput.dataSink);

    if (generateGraphOnlyWithThisFile) {
        std::ofstream ofs(*generateGraphOnlyWithThisFile);
        r.writeGraphVizDescription(ofs, "simple_demo_main_logic");
        ofs.close();
        return;
    }

    r.finalize();

    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });
}

void run_real(LogicChoice logicChoice, std::optional<std::string> generateGraphOnlyWithThisFile) {
    run_real_or_virtual(logicChoice, true, "", 0, 0.0, generateGraphOnlyWithThisFile);
}

void run_virtual(LogicChoice logicChoice, std::string const &calibrateTime, int calibrateAfter, double speed, std::optional<std::string> generateGraphOnlyWithThisFile) {
    run_real_or_virtual(logicChoice, false, calibrateTime, calibrateAfter, speed, generateGraphOnlyWithThisFile);
}

void run_backtest(LogicChoice logicChoice, std::string const &inputFile, std::optional<std::string> generateGraphOnlyWithThisFile) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>,false>,
        infra::IntIDComponent<>
    >;
    using M = infra::SinglePassIterationMonad<TheEnvironment>;
    using R = infra::MonadRunner<M>;

    std::ifstream ifs(inputFile);

    TheEnvironment env;
    R r(&env);

    using FileComponent = basic::ByteDataWithTopicRecordFileImporterExporter<M>;

    auto importer = FileComponent::createImporter<basic::ByteDataWithTopicRecordFileFormat<std::chrono::microseconds>>(
        ifs, 
        {(std::byte) 0x01,(std::byte) 0x23,(std::byte) 0x45,(std::byte) 0x67},
        {(std::byte) 0x76,(std::byte) 0x54,(std::byte) 0x32,(std::byte) 0x10}
    );
    auto parser = basic::SerializationActions<M>::deserialize<InputData>();
    
    auto removeTopic = M::liftPure<basic::TypedDataWithTopic<InputData>>(
        [](basic::TypedDataWithTopic<InputData> &&data) -> InputData {
            return data.content;
        }
    );
    
    r.registerImporter("importer", importer);
    r.registerAction("parser", parser);
    r.registerAction("removeTopic", removeTopic);

    auto dataInput = r.execute(removeTopic, r.execute(parser, r.importItem(importer)));

    basic::single_pass_iteration_clock::ClockOnOrderFacility<TheEnvironment>
        ::setupExitTimer(
        r 
        , std::chrono::hours(24)
        , dataInput.clone()
        , [](TheEnvironment *env) {
            env->log(infra::LogLevel::Info, "Wrapping up!");
        }
        , "exitTimerPart"
    );

    MainLogicInput<R> combinationInput {
        &(MockCalculatorCombination<
                R
                ,basic::single_pass_iteration_clock::ClockOnOrderFacility<TheEnvironment>
            >::service)
        , std::nullopt
        , std::nullopt
        , std::nullopt
    };
    auto mainLogicOutput = MainLogicCombination(r, env, std::move(combinationInput), logicChoice);
    r.connect(dataInput.clone(), mainLogicOutput.dataSink);

    if (generateGraphOnlyWithThisFile) {
        std::ofstream ofs(*generateGraphOnlyWithThisFile);
        r.writeGraphVizDescription(ofs, "simple_demo_main_logic");
        ofs.close();
        return;
    }

    r.finalize();

    infra::terminationController(infra::ImmediatelyTerminate {});
}

void run_typecheck(LogicChoice logicChoice, std::optional<std::string> generateGraphOnlyWithThisFile) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        basic::TrivialBoostLoggingComponent,
        basic::real_time_clock::ClockComponent,
        infra::IntIDComponent<>
    >;
    using M = infra::BasicWithTimeMonad<TheEnvironment>;
    using R = infra::MonadRunner<M>;

    TheEnvironment env;
    R r(&env);

    auto importer = M::simpleImporter<InputData>(
        [](TheEnvironment *) -> M::Data<InputData> {
            return std::nullopt;
        }
    );
    r.registerImporter("importer", importer);

    auto facility = M::liftPureOnOrderFacility<CalculateCommand>(
        [](CalculateCommand &&) -> CalculateResult {
            return CalculateResult {};
        }
    );
    r.registerOnOrderFacility("facility", facility);

    MainLogicInput<R> combinationInput {
        r.facilityConnector(facility)
        , std::nullopt
        , std::nullopt
        , std::nullopt
    };
    
    auto mainLogicOutput = MainLogicCombination(r, env, std::move(combinationInput), logicChoice);
    r.connect(r.importItem(importer), mainLogicOutput.dataSink);

    if (generateGraphOnlyWithThisFile) {
        std::ofstream ofs(*generateGraphOnlyWithThisFile);
        r.writeGraphVizDescription(ofs, "simple_demo_main_logic");
        ofs.close();
        return;
    }

    r.finalize();

    infra::terminationController(infra::ImmediatelyTerminate {});
}

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("mode", po::value<std::string>(), "real (default), virtual, backtest, or typecheck")
        ("input_file", po::value<std::string>(), "backtest input file")
        ("calibrate_time", po::value<std::string>(), "virtual time calibrate point")
        ("calibrate_after", po::value<int>(), "calibrate with virtual time after these minutes")
        ("speed", po::value<double>(), "virtual time speed (default 1.0)")
        ("generate_graph_only", po::value<std::string>(), "generate graph to this file and exit")
        ("logic_choice", po::value<int>(), "logic choice (1 or 2, default 1)")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    enum {
        Real
        , Virtual
        , Backtest
        , Typecheck
    } mode;
    if (vm.count("mode")) {
        auto modeStr = vm["mode"].as<std::string>();
        if (modeStr == "real") {
            mode = Real;
        } else if (modeStr == "virtual") {
            mode = Virtual;
        } else if (modeStr == "backtest") {
            mode = Backtest;
        } else if (modeStr == "typecheck") {
            mode = Typecheck;
        } else {
            std::cerr << "Mode must be real, virtual or backtest.\n";
            return 1;
        }
    } else {
        mode = Real;
    }

    std::optional<std::string> generateGraphOnlyWithThisFile = std::nullopt;
    if (vm.count("generate_graph_only")) {
        generateGraphOnlyWithThisFile = vm["generate_graph_only"].as<std::string>();
    }

    LogicChoice logicChoice = LogicChoice::One;
    if (vm.count("logic_choice")) {
        switch (vm["logic_choice"].as<int>()) {
        case 1:
            logicChoice = LogicChoice::One;
            break;
        case 2:
            logicChoice = LogicChoice::Two;
            break;
        default:
            std::cerr << "Logic choice must be 1 or 2.\n";
            return 1;
        }
    }

    switch (mode) {
    case Real:
        run_real(logicChoice, generateGraphOnlyWithThisFile);
        break;
    case Virtual:
        {
            if (!vm.count("calibrate_time")) {
                std::cerr << "Please provide calibrate time and calibrate after\n";
                return 1;
            }
            if (!vm.count("calibrate_after")) {
                std::cerr << "Please provide calibrate time and calibrate after\n";
                return 1;
            }
            double speed = 1.0;
            if (vm.count("speed")) {
                speed = vm["speed"].as<double>();
            }
            run_virtual(logicChoice, vm["calibrate_time"].as<std::string>(), vm["calibrate_after"].as<int>(), speed, generateGraphOnlyWithThisFile);
        }
        break;
    case Backtest:
        {
            if (!vm.count("input_file")) {
                std::cerr << "Please provide input file\n";
                return 1;
            }
            run_backtest(logicChoice, vm["input_file"].as<std::string>(), generateGraphOnlyWithThisFile);
        }
        break;
    case Typecheck:
        run_typecheck(logicChoice, generateGraphOnlyWithThisFile);
        break;
    default:
        return 0;
    }

    return 0;
}