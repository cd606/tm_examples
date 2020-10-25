#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/SinglePassIterationApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/ByteDataWithTopicRecordFileImporterExporter.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockImporter.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/MultiTransportBroadcastPublisherManagingUtils.hpp>

#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

using namespace dev::cd606::tm;
using namespace boost::program_options;

struct HMS {int hour; int min; int sec;};
struct HM {int hour; int min;};

struct ReplayParameter {
    std::string inputFile;
    std::string address;
    double speed;
    HMS calibratePointHistorical;
    std::variant<int, HM> calibratePointActual;
};

std::chrono::system_clock::time_point fetchFirstTimePoint(ReplayParameter &&param) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::FlagExitControlComponent,
        basic::TrivialBoostLoggingComponent,
        basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>,
        transport::BoostUUIDComponent
    >;
    using App = infra::SinglePassIterationApp<TheEnvironment>;
    using FileComponent = basic::ByteDataWithTopicRecordFileImporterExporter<App>;

    TheEnvironment env;
    infra::AppRunner<App> r(&env);

    std::ifstream ifs(param.inputFile, std::ios::binary);    
    auto importer = FileComponent::createImporter<basic::ByteDataWithTopicRecordFileFormat<std::chrono::microseconds>>(
        ifs, 
        {(std::byte) 0x01,(std::byte) 0x23,(std::byte) 0x45,(std::byte) 0x67},
        {(std::byte) 0x76,(std::byte) 0x54,(std::byte) 0x32,(std::byte) 0x10}
    );
    
    std::chrono::system_clock::time_point ret;

    auto finalizer = basic::CommonFlowUtilComponents<App>::simpleFinalizer<basic::ByteDataWithTopic>();
    auto getTime = App::simpleExporter<basic::ByteDataWithTopic>([&ret](App::InnerData<basic::ByteDataWithTopic> &&d) {
        ret = d.timedData.timePoint;
        d.environment->exit();
    });

    r.exportItem("getTime", getTime
        , r.execute("finalizer", finalizer
            , r.importItem("importer", importer)));

    r.finalize();
    
    ifs.close();

    return ret;
}

basic::VoidStruct runReplay(int which, ReplayParameter &&param, std::chrono::system_clock::time_point &&firstTimePoint) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::FlagExitControlComponent,
        basic::TrivialBoostLoggingComponent,
        basic::real_time_clock::ClockComponent,
        transport::BoostUUIDComponent,
        transport::AllNetworkTransportComponents
    >;
    using App = infra::RealTimeApp<TheEnvironment>;
    using FileComponent = basic::ByteDataWithTopicRecordFileImporterExporter<App>;

    TheEnvironment env;
        
    auto dateStr = infra::withtime_utils::localTimeString(firstTimePoint).substr(0, 10);
    std::ostringstream oss;
    oss << dateStr << 'T' 
        << std::setw(2) << std::setfill('0') << param.calibratePointHistorical.hour
        << ':'
        << std::setw(2) << std::setfill('0') << param.calibratePointHistorical.min
        << ':'
        << std::setw(2) << std::setfill('0') << param.calibratePointHistorical.sec
        << ".000";
    TheEnvironment::ClockSettings settings;
    if (param.calibratePointActual.index() == 0) {
        settings = TheEnvironment::clockSettingsWithStartPointCorrespondingToNextAlignment(
            std::get<0>(param.calibratePointActual)
            , oss.str()
            , param.speed
        );
    } else {
        settings = TheEnvironment::clockSettingsWithStartPoint(
            std::get<1>(param.calibratePointActual).hour*100+std::get<1>(param.calibratePointActual).min
            , oss.str()
            , param.speed
        );
    }
    
    env.basic::real_time_clock::ClockComponent::operator=(
        basic::real_time_clock::ClockComponent(settings)
    );
    using R = infra::AppRunner<App>;
    R r(&env);
    
    std::ifstream ifs(param.inputFile, std::ios::binary);
    auto importer = FileComponent::createImporter<basic::ByteDataWithTopicRecordFileFormat<std::chrono::microseconds>,true>(
        ifs, 
        {(std::byte) 0x01,(std::byte) 0x23,(std::byte) 0x45,(std::byte) 0x67},
        {(std::byte) 0x76,(std::byte) 0x54,(std::byte) 0x32,(std::byte) 0x10}
    );
    auto dataSink = transport::MultiTransportBroadcastPublisherManagingUtils<R>
        ::oneByteDataBroadcastPublisher
        (
            r
            , "data sink"
            , param.address
        );

    auto filter = App::kleisli<basic::ByteDataWithTopic>(
        basic::CommonFlowUtilComponents<App>
            ::pureFilter<basic::ByteDataWithTopic>(
                [](basic::ByteDataWithTopic const &d) {
                    return (!d.content.empty());
                }
            )
    );

    r.connect(
        r.execute("filter", filter, r.importItem("importer", importer))
        , dataSink
    );

    auto exiter = App::simpleExporter<basic::ByteDataWithTopic>(
        [&env](App::InnerData<basic::ByteDataWithTopic> &&d) {
            if (d.timedData.finalFlag) {
                d.environment->log(infra::LogLevel::Info, "Got the final update!");
                std::thread([&env]() {
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    env.exit();
                }).detach();
            }
        }
    );
    r.exportItem("exiter", exiter, r.importItem(importer));

    r.finalize();

    infra::terminationController(infra::RunForever { &env, std::chrono::seconds(1) });

    ifs.close();

    return basic::VoidStruct {};
}

int main(int argc, char **argv) {
    options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("address", value<std::string>(), "the address to republish on, with protocol info")
        ("input", value<std::string>(), "input from this file")
        ("speed", value<double>(), "republish speed factor, default 1.0")
        ("calibratePointHistorical", value<std::string>(), "calibrate time point (for historical data), format is HH:MM[:SS]")
        ("calibratePointActual", value<std::string>(), "calibrate time point (for actual clock), format is HH:MM (no second!), or +N (where N is count of minutes)")
    ;
    variables_map vm;
    store(parse_command_line(argc, argv, desc), vm);
    notify(vm);

    ReplayParameter param;

    if (vm.count("help")) {
        std::cout << desc << '\n';
        return 0;
    }
    if (!vm.count("address")) {
        std::cerr << "No address given!\n";
        return 1;
    }
    param.address = vm["address"].as<std::string>();
    if (!vm.count("input")) {
        std::cerr << "No output file given!\n";
        return 1;
    }
    auto input = vm["input"].as<std::string>();
    param.inputFile = input;
    double speed = 1.0;
    if (vm.count("speed")) {
        speed = vm["speed"].as<double>();
    }
    param.speed = speed;
    if (!vm.count("calibratePointHistorical")) {
        std::cerr << "No historical calibrate point given!\n";
        return 1;
    }
    auto calibratePointHistorical = boost::trim_copy(vm["calibratePointHistorical"].as<std::string>());
    if ((calibratePointHistorical.length() != 5 && calibratePointHistorical.length() != 8) || calibratePointHistorical[2] != ':') {
        std::cerr << "Historical calibrate point must be in HH:MM[:SS] format!\n";
        return 1;
    }
    int hour_hist, min_hist, sec_hist;
    try {
        hour_hist = boost::lexical_cast<int>(calibratePointHistorical.substr(0,2));
        min_hist = boost::lexical_cast<int>(calibratePointHistorical.substr(3,2));
        if (calibratePointHistorical.length() == 8) {
            sec_hist = boost::lexical_cast<int>(calibratePointHistorical.substr(6,2));
        } else {
            sec_hist = 0;
        }
    } catch (boost::bad_lexical_cast const &) {
        std::cerr << "Historical calibrate point must be in HH:MM[:SS] format!\n";
        return 1;
    }
    if (hour_hist < 0 || hour_hist >= 24 || min_hist < 0 || min_hist >= 60 || sec_hist < 0 || sec_hist >= 60) {
        std::cerr << "Historical calibrate point must be in HH:MM[:SS] format!\n";
        return 1;
    }
    param.calibratePointHistorical = HMS {hour_hist, min_hist, sec_hist};
    if (!vm.count("calibratePointActual")) {
        std::cerr << "No Actual calibrate point given!\n";
        return 1;
    }
    auto calibratePointActual = boost::trim_copy(vm["calibratePointActual"].as<std::string>());
    std::optional<int> calibrateActualMinutes = std::nullopt;
    int hour_act=-1, min_act=-1;
    if (calibratePointActual.length() > 1 && calibratePointActual[0] == '+') {
        try {
            calibrateActualMinutes = boost::lexical_cast<int>(calibratePointActual.substr(1));
        } catch (boost::bad_lexical_cast const &) {
            calibrateActualMinutes = std::nullopt;
        }
    }
    if (!calibrateActualMinutes) {
        if (calibratePointActual.length() != 5 || calibratePointActual[2] != ':') {
            std::cerr << "Actual calibrate point must be in HH:MM or +N format!\n";
            return 1;
        }      
        try {
            hour_act = boost::lexical_cast<int>(calibratePointActual.substr(0,2));
            min_act = boost::lexical_cast<int>(calibratePointActual.substr(3,2));
        } catch (boost::bad_lexical_cast const &) {
            std::cerr << "Actual calibrate point must be in HH:MM or +N format!\n";
            return 1;
        }
        if (hour_act < 0 || hour_act >= 24 || min_act < 0 || min_act >= 60) {
            std::cerr << "Actual calibrate point must be in HH:MM or +N format!\n";
            return 1;
        }
        param.calibratePointActual = HM {hour_act, min_act};
    } else {
        param.calibratePointActual = *calibrateActualMinutes;
    }

    //Following commented-out code is an experiment of using an outer-graph
    //to combine inner-graphs. It is completely equivalent to the actual two-liner
    //below in functionality.
    /*
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::FlagExitControlComponent,
        basic::TrivialBoostLoggingComponent,
        basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>,
        transport::BoostUUIDComponent
    >;
    using App = infra::SinglePassIterationApp<TheEnvironment>;
    TheEnvironment env;
    infra::AppRunner<App> r(&env);

    auto starter = App::constFirstPushImporter(std::move(param));
    auto fetchTP = App::liftPure<ReplayParameter>(&fetchFirstTimePoint, infra::LiftParameters<App::TimePoint>().FireOnceOnly(true));
    auto replayRunner = App::liftPure2<ReplayParameter, std::chrono::system_clock::time_point>(&runReplay, infra::LiftParameters<App::TimePoint>().RequireMask(infra::FanInParamMask("11")).FireOnceOnly(true));
    auto emptyExporter = App::pureExporter<basic::VoidStruct>([&env](basic::VoidStruct &&) {
        env.exit();
    });

    auto paramData = r.importItem("starter", starter);
    auto tpData = r.execute("fetchTP", fetchTP, paramData.clone());
    r.execute("replayRunner", replayRunner, std::move(tpData));
    auto finalData = r.execute(replayRunner, paramData.clone());
    r.exportItem("emptyExporter", emptyExporter, std::move(finalData));

    r.finalize();
    */
    auto tp = fetchFirstTimePoint(std::move(param));
    runReplay(1, std::move(param), std::move(tp));

    return 0;
}
