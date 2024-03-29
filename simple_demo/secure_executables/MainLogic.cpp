#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/BasicWithTimeApp.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/SinglePassIterationApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/IntIDComponent.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockOnOrderFacility.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockOnOrderFacility.hpp>
#include <tm_kit/basic/ByteDataWithTopicRecordFileImporterExporter.hpp>
#include <tm_kit/basic/AppRunnerUtils.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListenerManagingUtils.hpp>
#include <tm_kit/transport/MultiTransportBroadcastPublisherManagingUtils.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>
#include <tm_kit/transport/security/SignatureBasedIdentityCheckerComponent.hpp>
#include <tm_kit/transport/security/SignatureAndVerifyHookFactoryComponents.hpp>
#include <tm_kit/transport/TLSConfigurationComponent.hpp>

#include "defs.pb.h"
#include "simple_demo/program_logic/MainLogic.hpp"
#include "simple_demo/app_combination/MainLogicCombination.hpp"
#include "simple_demo/app_combination/MockCalculatorCombination.hpp"
#include "simple_demo/security_logic/SignatureAndEncBasedIdentityCheckerComponent.hpp"
#include "simple_demo/security_logic/DHHelper.hpp"
#include "simple_demo/security_logic/EncAndSignHookFactory.hpp"

#include <boost/program_options.hpp>

#include <iostream>
#include <fstream>
#include <sstream>

using namespace dev::cd606::tm;
using namespace simple_demo;

void run_real_or_virtual(LogicChoice logicChoice, bool isReal, std::string const &calibrateTime, int calibrateAfter, double speed, std::optional<std::string> generateGraphOnlyWithThisFile) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::real_time_clock::ClockComponent>,
        //basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::BoostUUIDComponent,
        ClientSideSignatureAndEncBasedIdentityAttacherComponent<CalculateCommand>,
        transport::security::ClientSideSignatureBasedIdentityAttacherComponent<DHHelperCommand>,
        transport::security::VerifyHookFactoryComponent<DHHelperReply>,
        transport::security::ServerSideSignatureBasedIdentityCheckerComponent<ConfigureCommandPOCO>,
        transport::security::ServerSideSignatureBasedIdentityCheckerComponent<ClearCommands>,
        transport::AllNetworkTransportComponents,
        transport::HeartbeatAndAlertComponent,
        transport::TLSServerConfigurationComponent,
        EncAndSignHookFactoryComponent<transport::HeartbeatMessage>,
        DecHookFactoryComponent<InputData>
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;   
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

    //These are the keys to verify messages coming from clients
    std::tuple<
        std::string
        , std::array<unsigned char, 32>
    > client_pub_keys[4] {
        {
            "client_1"
            , {
                0xDC,0x60,0xCB,0x39,0x15,0x4E,0xAC,0x0F,0x2C,0x6E,0xCE,0xE6,0x09,0x3C,0xD6,0x3D,
                0x60,0x13,0x28,0xF7,0xA8,0xA5,0x18,0xAE,0xDA,0xDA,0x9E,0x6B,0x28,0x96,0x13,0x0C
            }
        }
        ,
        {
            "client_2"
            , {
                0xDF,0x4A,0x04,0xC4,0xD9,0x7B,0x6F,0xCC,0x28,0xDB,0xDB,0x79,0x6F,0x4E,0x9C,0x7C,
                0xD9,0x07,0x11,0xAA,0xD1,0xCE,0xF3,0xA7,0x37,0x30,0x2B,0x58,0xA8,0xD4,0x5B,0xF1
            }
        }
        ,
        {
            "client_3"
            , {
                0x37,0xBB,0xD5,0x85,0x7E,0x07,0x2A,0xE2,0x05,0xE9,0x15,0x8B,0xEF,0xCB,0x24,0x9A,
                0x3A,0xDB,0x9F,0x87,0x53,0x36,0x85,0x22,0x34,0x3D,0xC1,0x85,0xD1,0x0B,0x3E,0x99
            }
        }
        ,
        {
            "gui_client"
            , {
                0x3E,0xCD,0x80,0x72,0x7F,0x86,0xB2,0x22,0xB8,0xDB,0x46,0x3F,0x5C,0x75,0x74,0x54,
                0x96,0x14,0x08,0x35,0xB6,0x18,0xFE,0xCD,0xB6,0xC2,0xC3,0xCA,0xB5,0x3E,0xEC,0x0C
            }
        }
    };
    for (auto const &clientItem : client_pub_keys) {
        env.transport::security::ServerSideSignatureBasedIdentityCheckerComponent<ConfigureCommandPOCO>
            ::add_identity_and_key(
            std::get<0>(clientItem), std::get<1>(clientItem)
        );
        env.transport::security::ServerSideSignatureBasedIdentityCheckerComponent<ClearCommands>
            ::add_identity_and_key(
            std::get<0>(clientItem), std::get<1>(clientItem)
        );
    }

    env.transport::TLSServerConfigurationComponent::setConfigurationItem(
        transport::TLSServerInfoKey {
            34567
        }
        , transport::TLSServerInfo {
            "../grpc_interop_test/DotNetServer/server.crt"
            , "../grpc_interop_test/DotNetServer/server.key"
        }
    );
    env.transport::TLSServerConfigurationComponent::setConfigurationItem(
        transport::TLSServerInfoKey {
            45678
        }
        , transport::TLSServerInfo {
            "../grpc_interop_test/DotNetServer/server.crt"
            , "../grpc_interop_test/DotNetServer/server.key"
        }
    );
    env.transport::json_rest::JsonRESTComponent::setDocRoot(
        34567 
        , "../simple_demo/secure_executables/web_enabler"
    );
    env.transport::json_rest::JsonRESTComponent::addBasicAuthentication(
        34567, "user1", std::nullopt
    );
    env.transport::json_rest::JsonRESTComponent::addBasicAuthentication(
        34567, "user2", "abcde"
    );

    //env.setLogFilePrefix("simple_demo_secure_main", true);
    R r(&env);

    std::array<unsigned char, 64> heartbeat_sign_prv_key { 
        0x03,0x52,0x91,0x4C,0x68,0x72,0xA6,0xB3,0x7D,0xC6,0xBA,0x71,0x65,0x5D,0xEB,0x5C,
        0xC9,0xE8,0x3D,0x84,0xE5,0x90,0x36,0x5F,0xBF,0x3B,0xAF,0xB7,0x44,0xC4,0xA1,0x50,
        0xDA,0xA0,0x15,0xD4,0x33,0xE8,0x92,0xC9,0xF2,0x96,0xA1,0xF8,0x1F,0x79,0xBC,0xF4,
        0x2D,0x7A,0xDE,0x48,0x03,0x47,0x16,0x0C,0x57,0xBD,0x1F,0x45,0x81,0xB5,0x18,0x2E 
    };
    env.EncAndSignHookFactoryComponent<transport::HeartbeatMessage>::operator=(
        EncAndSignHookFactoryComponent<transport::HeartbeatMessage> {
            "testkey",
            heartbeat_sign_prv_key
        }
    );
    transport::initializeHeartbeatAndAlertComponent(
        &env
        , "simple_demo secure MainLogic"
        , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
    );

    env.DecHookFactoryComponent<InputData>::operator=(
        DecHookFactoryComponent<InputData> {
            "input_data_key"
        }
    );

    auto dataSourceHeartbeatSource = 
        transport::MultiTransportBroadcastListenerManagingUtils<R>
        ::oneBroadcastListener<
            transport::HeartbeatMessage
        >(
            r 
            , "heartbeatListener"
            , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
            , "simple_demo.secure_executables.data_source.heartbeat"
        );

    auto inputDataSource = transport::MultiTransportBroadcastListenerManagingUtils<R>
        ::setupBroadcastListenerThroughHeartbeat<InputData>
    (
        r 
        , dataSourceHeartbeatSource.clone()
        , std::regex("simple_demo secure DataSource")
        , "input data publisher"
        , "input.data"
        , "inputDataSourceComponents"
    );

    R::FacilitioidConnector<CalculateCommand,CalculateResult> calc;
    if (isReal) {
        //This is the key to sign messages going out to calculate server
        std::array<unsigned char, 64> my_private_key { 
            0x5E,0xD3,0x8F,0xE8,0x0A,0x67,0xA0,0xA4,0x24,0x0C,0x2D,0x0C,0xFE,0xB2,0xF4,0x78,
            0x69,0x46,0x01,0x95,0xF8,0xE4,0xD1,0xBB,0xC1,0xBC,0x22,0xCC,0x2F,0xB2,0x60,0xB0,
            0x69,0x61,0xB9,0xCF,0xBA,0x37,0xD0,0xE2,0x70,0x32,0x84,0xF9,0x41,0x02,0x17,0x22,
            0xFA,0x89,0x0F,0xE4,0xBA,0xAC,0xC8,0x73,0xB9,0x00,0x99,0x24,0x38,0x42,0xC2,0x9A 
        };
        //This is the key to verify messages coming from calculate server
        std::array<unsigned char, 32> calculate_server_public_key { 
            0x69,0x61,0xB9,0xCF,0xBA,0x37,0xD0,0xE2,0x70,0x32,0x84,0xF9,0x41,0x02,0x17,0x22,
            0xFA,0x89,0x0F,0xE4,0xBA,0xAC,0xC8,0x73,0xB9,0x00,0x99,0x24,0x38,0x42,0xC2,0x9A 
        };

        env.ClientSideSignatureAndEncBasedIdentityAttacherComponent<CalculateCommand>::operator=(
            ClientSideSignatureAndEncBasedIdentityAttacherComponent<CalculateCommand>(
                my_private_key
            )
        );
        env.transport::security::ClientSideSignatureBasedIdentityAttacherComponent<DHHelperCommand>::operator=(
            transport::security::ClientSideSignatureBasedIdentityAttacherComponent<DHHelperCommand>(
                my_private_key
            )
        );
        env.transport::security::VerifyHookFactoryComponent<DHHelperReply>::operator=(
            transport::security::VerifyHookFactoryComponent<DHHelperReply>(
                {{"calculate_server", calculate_server_public_key}}
            )
        );

        auto dhClientHelper = std::make_shared<DHClientHelper>(&env);
        r.preservePointer(dhClientHelper);

        auto dhHeartbeatHook = VerifyAndDecHookFactoryComponent<transport::HeartbeatMessage>(
            "testkey", calculate_server_public_key
        ).defaultHook();
        auto dhHeartbeatSource = 
            transport::MultiTransportBroadcastListenerManagingUtils<R>
            ::oneBroadcastListener<
                transport::HeartbeatMessage
            >(
                r 
                , "dh_heartbeatListener"
                , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
                , "simple_demo.secure_executables.calculator.heartbeat"
                , dhHeartbeatHook
            );
        calc = std::get<1>(
            transport::MultiTransportRemoteFacilityManagingUtils<R>
            ::template setupTwoStepRemoteFacility
                <DHHelperCommand, DHHelperReply, CalculateCommand, CalculateResult>
            (
                r
                , dhHeartbeatSource.clone()
                , std::regex("simple_demo secure Calculator")
                , {
                    "dh_server_facility", "facility"
                }
                , [&env,dhClientHelper]() -> DHHelperCommand {
                    env.log(infra::LogLevel::Info, "Creating DH client command");
                    return dhClientHelper->resetAndBuildCommand(); //this forces a regeneration of key
                }
                , [dhClientHelper](DHHelperCommand const &, DHHelperReply const &data) -> bool {
                    dhClientHelper->process(data);
                    return true;
                }
            )
        ).facility;
    } else {
        calc = &(MockCalculatorCombination<
                    R
                    ,basic::real_time_clock::ClockOnOrderFacility<TheEnvironment>
                >::service);
    }
    
    MainLogicInput<R> combinationInput {
        calc
        , {
            transport::MultiTransportFacilityWrapper<R>::facilityWrapperWithProtocol
                <basic::proto_interop::Proto, ConfigureCommandPOCO, ConfigureResultPOCO>(
                "rabbitmq://127.0.0.1::guest:guest:test_config_queue"
                , "cfg_wrapper_"
            )
            , transport::MultiTransportFacilityWrapper<R>::facilityWrapperWithProtocol
                <basic::nlohmann_json_interop::Json, ConfigureCommandPOCO, ConfigureResultPOCO>(
                "json_rest://0.0.0.0:34567:::/configureMainLogic"
                , "cfg_wrapper_2_"
            )
        }
        , transport::MultiTransportFacilityWrapper<R>::facilityWrapper
            <OutstandingCommandsQuery,OutstandingCommandsResult>(
            "rabbitmq://127.0.0.1::guest:guest:test_query_queue"
            , "query_wrapper_"
        )
        , transport::MultiTransportFacilityWrapper<R>::facilityWrapper
            <ClearCommands,ClearCommandsResult>(
            "rabbitmq://127.0.0.1::guest:guest:test_clear_queue"
            , "clear_cmd_wrapper_"
        )
    };
    auto mainLogicOutput = MainLogicCombination(r, env, std::move(combinationInput), logicChoice, "simple_demo.secure_executables.main_logic.alert");
    r.connect(std::move(inputDataSource), mainLogicOutput.dataSink);
    
    if (generateGraphOnlyWithThisFile) {
        std::ofstream ofs(*generateGraphOnlyWithThisFile);
        r.writeGraphVizDescription(ofs, "simple_demo_main_logic");
        ofs.close();
        return;
    }

    transport::attachHeartbeatAndAlertComponent(r, &env, "simple_demo.secure_executables.main_logic.heartbeat", std::chrono::seconds(1));

    auto heartbeatForPubSource = transport::addHeartbeatPublishingSource(r);
    auto extractEnabled = M::liftPure<transport::HeartbeatMessage>(
        [](transport::HeartbeatMessage &&msg) -> basic::TypedDataWithTopic<simple_demo::ConfigureResultPOCO> {
            auto st = msg.status("calculation_status");
            simple_demo::ConfigureResultPOCO res {
                st && st->status == transport::HeartbeatMessage::Status::Good
            };
            return {"", std::move(res)};
        }
    );
    r.registerAction("extractEnabled", extractEnabled);
    r.connect(heartbeatForPubSource.clone(), r.actionAsSink(extractEnabled));
    auto enabledPubSink = transport::MultiTransportBroadcastPublisherManagingUtils<R>
        ::oneBroadcastPublisherWithProtocol<basic::nlohmann_json_interop::Json, simple_demo::ConfigureResultPOCO>
        (
            r, "enabledPub"
            , "websocket://0.0.0.0:45678[ignoreTopic=true]"
        );
    r.connect(r.actionAsSource(extractEnabled), enabledPubSink);

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
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>,false>,
        //basic::TimeComponentEnhancedWithSpdLogging<basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>,false>,
        basic::IntIDComponent<>
    >;
    using M = infra::SinglePassIterationApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    std::ifstream ifs(inputFile, std::ios::binary);

    TheEnvironment env;
    //env.setLogFilePrefix("simple_demo_secure_main", true);
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

    basic::AppRunnerUtilComponents<R>
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
        , {}
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
        infra::TrivialExitControlComponent,
        basic::TrivialBoostLoggingComponent,
        basic::real_time_clock::ClockComponent,
        basic::IntIDComponent<>
    >;
    using M = infra::BasicWithTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

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
        , {}
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
