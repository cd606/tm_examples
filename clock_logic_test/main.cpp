#include "ClockLogicMain.hpp"

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/SinglePassIterationApp.hpp>
#include <tm_kit/infra/TopDownSinglePassIterationApp.hpp>

#include <tm_kit/basic/IntIDComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/top_down_single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/empty_clock/ClockComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>

#include <iostream>
#include <fstream>

using namespace dev::cd606::tm;

void real_time_run(std::ostream &fileOutput) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::CrossGuidComponent
    >;
    using App = infra::RealTimeApp<TheEnvironment>;

    TheEnvironment env;

    /*
    const std::string settingStr = R"json(
        {
            "clock_settings": {
                "actual" : {"time" : "09:00"}
                , "virtual" : {"date" : "2020-01-01", "time" : "10:00"}  
                , "speed" : "2.0"
            }
        }
        )json";
    auto settings = TheEnvironment::loadClockSettingsFromJSONString(settingStr);
    env.useClockSettings(settings);*/
    auto clockSettings = TheEnvironment::clockSettingsWithStartPointCorrespondingToNextAlignment(
        1
        , "2020-01-01T10:00"
        , 2.0
    );
    env.basic::real_time_clock::ClockComponent::operator=(
        basic::real_time_clock::ClockComponent(clockSettings)
    );
    env.setLogFilePrefix("clock_logic_test", true);

    infra::AppRunner<App> r(&env);
    auto fut = clock_logic_test_app::clockLogicMain(r, fileOutput);
    r.writeGraphVizDescription(std::cout, "test");
    r.finalize();

    infra::terminationController(infra::TerminateAtTimePoint {
        env.actualTime(
            infra::withtime_utils::parseLocalTime("2020-01-01T10:01:05")
        )
    });
    env.log(infra::LogLevel::Info, fut.get());
}

void single_pass_iteration_run(std::ostream &fileOutput) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<
            basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>
            , false
        >,
        basic::IntIDComponent<uint32_t>
    >;
    using App = infra::SinglePassIterationApp<TheEnvironment>;

    TheEnvironment env;
    env.setLogFilePrefix("clock_logic_test", true);

    infra::AppRunner<App> r(&env);
    clock_logic_test_app::clockLogicMain(r, fileOutput);
    r.writeGraphVizDescription(std::cout, "test");
    r.finalize();

    infra::terminationController(infra::ImmediatelyTerminate {});
}

void top_down_single_pass_iteration_run(std::ostream &fileOutput) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<
            basic::top_down_single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>
            , false
        >,
        basic::IntIDComponent<uint32_t>
    >;
    using App = infra::TopDownSinglePassIterationApp<TheEnvironment>;

    TheEnvironment env;
    env.setLogFilePrefix("clock_logic_test", true);

    infra::AppRunner<App> r(&env);
    clock_logic_test_app::clockLogicMain(r, fileOutput);
    r.writeGraphVizDescription(std::cout, "test");
    r.finalize();

    infra::terminationController(infra::ImmediatelyTerminate {});
}

void typecheck_run(std::ostream &fileOutput) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<
            basic::empty_clock::ClockComponent<std::chrono::system_clock::time_point>
            , false
        >,
        basic::IntIDComponent<uint32_t>
    >;
    using App = infra::BasicWithTimeApp<TheEnvironment>;

    TheEnvironment env;
    env.setLogFilePrefix("clock_logic_test", true);

    infra::AppRunner<App> r(&env);
    clock_logic_test_app::clockLogicMain(r, fileOutput);
    r.writeGraphVizDescription(std::cout, "test");
    r.finalize();

    infra::terminationController(infra::ImmediatelyTerminate {});
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: clock_logic_test fileName real_time|single_pass_iteration|top_down_single_pass_iteration|typecheck\n";
        return 1;
    }
    std::string mode = argv[2];
    if (mode != "real_time" && mode != "single_pass_iteration" && mode != "top_down_single_pass_iteration" && mode != "typecheck") {
        std::cerr << "Usage: clock_logic_test fileName [real_time|single_pass_iteration|top_down_single_pass_iteration|typecheck]\n";
        return 1;
    }  
    std::ofstream ofs(argv[1], std::ios::binary);
    if (mode == "real_time") {
        real_time_run(ofs);
    } else if (mode == "single_pass_iteration") {
        single_pass_iteration_run(ofs);
    } else if (mode == "top_down_single_pass_iteration") {
        top_down_single_pass_iteration_run(ofs);
    } else if (mode == "typecheck") {
        typecheck_run(ofs);
    }
    ofs.close();
    return 0;
}
