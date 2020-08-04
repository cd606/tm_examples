#include "ClockLogicMain.hpp"

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/SinglePassIterationApp.hpp>
#include <tm_kit/infra/IntIDComponent.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/real_time_clock/ClockOnOrderFacility.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockImporter.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockOnOrderFacility.hpp>

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

    infra::AppRunner<App> r(&env);
    clock_logic_test_app::clockLogicMain<
        basic::real_time_clock::ClockImporter<TheEnvironment>
        , basic::real_time_clock::ClockOnOrderFacility<TheEnvironment>
        >(r, fileOutput);
    r.writeGraphVizDescription(std::cout, "test");
    r.finalize();

    infra::terminationController(infra::TerminateAtTimePoint {
        env.actualTime(
            infra::withtime_utils::parseLocalTime("2020-01-01T10:01:05")
        )
    });
}
void single_pass_iteration_run(std::ostream &fileOutput) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<
            basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>
            , false
        >,
        infra::IntIDComponent<uint32_t>
    >;
    using App = infra::SinglePassIterationApp<TheEnvironment>;

    TheEnvironment env;

    infra::AppRunner<App> r(&env);
    clock_logic_test_app::clockLogicMain<
        basic::single_pass_iteration_clock::ClockImporter<TheEnvironment>
        , basic::single_pass_iteration_clock::ClockOnOrderFacility<TheEnvironment>
        >(r, fileOutput);
    r.writeGraphVizDescription(std::cout, "test");
    r.finalize();

    infra::terminationController(infra::ImmediatelyTerminate {});
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: clock_logic_test fileName real_time|single_pass_iteration\n";
        return 1;
    }
    std::string mode = argv[2];
    if (mode != "real_time" && mode != "single_pass_iteration") {
        std::cerr << "Usage: clock_logic_test [real_time|single_pass_iteration]\n";
        return 1;
    }   
    std::ofstream ofs(argv[1]);
    if (mode == "real_time") {
        real_time_run(ofs);
    } else {
        single_pass_iteration_run(ofs);
    }
    ofs.close();
    return 0;
}
