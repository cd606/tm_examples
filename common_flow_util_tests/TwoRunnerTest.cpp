#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/SinglePassIterationApp.hpp>
#include <tm_kit/infra/TopDownSinglePassIterationApp.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/AppClockHelper.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/MultiAppRunnerUtils.hpp>
#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/BoostUUIDComponent.hpp>

#include <iostream>
#include <fstream>

using namespace dev::cd606::tm;

template <class R>
void graph1(std::chrono::system_clock::time_point now, R &r, std::function<void(infra::WithTime<std::string,std::chrono::system_clock::time_point> &&)> wormholeOut) {
    using M = typename R::AppType;
    auto importer = basic::AppClockHelper<M>::Importer::template createRecurringClockImporter<int>(
        now+std::chrono::seconds(1)
        , now+std::chrono::seconds(6)
        , std::chrono::seconds(1)
        , [](std::chrono::system_clock::time_point const &) {
            static int val = 0;
            return (++val);
        }
    );
    auto transform = M::template liftPure<int>([](int &&x) {return x+1;});
    auto exporter = M::template pureExporter<int>(
        [&r](int &&x) {
            r.environment()->log(infra::LogLevel::Info, std::string("graph1: ")+std::to_string(x));
        }
    );
    auto transform2 = M::template liftPure<int>([](int &&x) {return std::string("from 1: ")+std::to_string(x+100);});
    auto exporter2 = M::template simpleExporter<std::string>(
        [wormholeOut](typename M::template InnerData<std::string> &&data) {
            wormholeOut(std::move(data.timedData));
        }
    );
    r.exportItem("exporter", exporter, r.execute("transform", transform, r.importItem("importer", importer)));
    r.exportItem("exporter2", exporter2, r.execute("transform2", transform2, r.actionAsSource(transform)));
}
template <class R>
void graph2(std::chrono::system_clock::time_point now, R &r, typename R::template Source<std::string> &&wormholeIn) {
    using M = typename R::AppType;
    auto importer = basic::AppClockHelper<M>::Importer::template createRecurringClockImporter<std::string>(
        now+std::chrono::seconds(2)
        , now+std::chrono::seconds(7)
        , std::chrono::seconds(1)
        , [](std::chrono::system_clock::time_point const &) {
            static int val = 0;
            return std::string("source ")+std::to_string((++val)+200);
        }
    );
    auto transform = M::template liftPure<std::string>([](std::string &&x) {return x+":transformed";});
    r.registerAction("transform", transform);
    r.execute(transform, r.importItem("importer", importer));
    r.execute(transform, wormholeIn.clone());
    auto exporter = M::template pureExporter<std::string>(
        [&r](std::string &&x) {
            r.environment()->log(infra::LogLevel::Info, std::string("graph2: ")+x);
        }
    );
    r.exportItem("exporter", exporter, r.actionAsSource(transform));
}

void runRealTime() {
    using Env1 = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::CrossGuidComponent,
        infra::TraceNodesComponent
    >;
    using Env2 = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::real_time_clock::ClockComponent>,
        transport::BoostUUIDComponent
    >;
    Env1 env1;
    Env2 env2;
    std::ofstream ofs("trace.log");
    env1.setTraceStream(&ofs);
    using M1 = infra::RealTimeApp<Env1>;
    using M2 = infra::RealTimeApp<Env2>;
    infra::AppRunner<M1> r1(&env1);
    infra::AppRunner<M2> r2(&env2);

    auto wormhole = M2::triggerImporterWithTime<std::string>();
    r2.registerImporter("wormholeIn", std::get<0>(wormhole));

    auto now = env1.now();

    graph1(now, r1, std::get<1>(wormhole));
    graph2(now, r2, r2.importItem(std::get<0>(wormhole)));

    basic::MultiAppRunnerUtilComponents::run(r1, r2);

    infra::terminationController(infra::TerminateAfterDuration {
        std::chrono::seconds(8)
    });
}

void runSinglePass() {
    using Env1 = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point, true>>,
        transport::CrossGuidComponent,
        infra::TraceNodesComponent
    >;
    using Env2 = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point, true>>,
        transport::BoostUUIDComponent
    >;
    Env1 env1;
    Env2 env2;
    std::ofstream ofs("trace.log");
    env1.setTraceStream(&ofs);
    env1.setLocalTime(std::chrono::system_clock::now());
    env2.setLocalTime(std::chrono::system_clock::now());
    using M1 = infra::SinglePassIterationApp<Env1>;
    using M2 = infra::SinglePassIterationApp<Env2>;
    infra::AppRunner<M1> r1(&env1);
    infra::AppRunner<M2> r2(&env2);

    auto wormhole = M2::triggerImporterWithTime<std::string>();
    r2.registerImporter("wormholeIn", std::get<0>(wormhole));

    auto now = env1.now();

    graph1(now, r1, std::get<1>(wormhole));
    graph2(now, r2, r2.importItem(std::get<0>(wormhole)));

    basic::MultiAppRunnerUtilComponents::run(r1, r2);
}

void runTopDownSinglePass() {
    using Env1 = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::top_down_single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point, true>>,
        transport::CrossGuidComponent,
        infra::TraceNodesComponent
    >;
    using Env2 = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::top_down_single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point, true>>,
        transport::BoostUUIDComponent
    >;
    Env1 env1;
    Env2 env2;
    std::ofstream ofs("trace.log");
    env1.setTraceStream(&ofs);
    env1.setLocalTime(std::chrono::system_clock::now());
    env2.setLocalTime(std::chrono::system_clock::now());
    using M1 = infra::TopDownSinglePassIterationApp<Env1>;
    using M2 = infra::TopDownSinglePassIterationApp<Env2>;
    infra::AppRunner<M1> r1(&env1);
    infra::AppRunner<M2> r2(&env2);

    auto wormhole = M2::triggerImporterWithTime<std::string>();
    r2.registerImporter("wormholeIn", std::get<0>(wormhole));

    auto now = env1.now();

    graph1(now, r1, std::get<1>(wormhole));
    graph2(now, r2, r2.importItem(std::get<0>(wormhole)));

    basic::MultiAppRunnerUtilComponents::run(r1, r2);
}

int main() {
    //runRealTime();
    //runSinglePass();
    runTopDownSinglePass();
    return 0;
}