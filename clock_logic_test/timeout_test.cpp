#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/SinglePassIterationApp.hpp>
#include <tm_kit/infra/BasicWithTimeApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/AppRunnerUtils.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/empty_clock/ClockComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>

using namespace dev::cd606::tm;

template <class M>
void run(int timeoutSec, int sleepSec) {
    typename M::EnvironmentType env;
    using R = infra::AppRunner<M>;

    R r(&env);
    auto primary = M::template liftPure<int>(
        [&env,sleepSec](int &&x) -> double {
            env.log(infra::LogLevel::Info, "Primary fires");
            if constexpr (std::is_same_v<M, infra::RealTimeApp<typename M::EnvironmentType>>) {
                std::this_thread::sleep_for(std::chrono::seconds(sleepSec));
            }
            return x*3.0;
        }
        , infra::LiftParameters<std::chrono::system_clock::time_point>()
            .SuggestThreaded(true)
            .DelaySimulator([sleepSec](int, std::chrono::system_clock::time_point const &) {
                return std::chrono::seconds(sleepSec);
            })
    );
    auto secondary = M::template liftPure<int>([&env](int &&x) -> double {
        env.log(infra::LogLevel::Info, "Secondary fires");
        return x*2.0;
    });

    auto importer = M::template constFirstPushImporter<int>(1);
    auto exporter = M::template pureExporter<double>([&env](double &&d) {
        env.log(infra::LogLevel::Info, ""+std::to_string(d));
    });

    auto pathway = basic::AppRunnerUtilComponents<R>::template pathwayWithTimeout<int, double>(
        std::chrono::seconds(timeoutSec)
        , basic::AppRunnerUtilComponents<R>::template singleActionPathway<int,double>(primary)
        , basic::AppRunnerUtilComponents<R>::template singleActionPathway<int,double>(secondary)
        , "timeout"
    );
    r.registerAction("secondary", secondary);
    r.registerAction("primary", primary);
    r.registerImporter("importer", importer);
    r.registerExporter("exporter", exporter);
    pathway(r, r.importItem(importer), r.exporterAsSink(exporter));

    std::ostringstream oss;
    oss << "\n";
    r.writeGraphVizDescription(oss, "timeout_test");
    env.log(infra::LogLevel::Info, oss.str());

    r.finalize();

    if constexpr (std::is_same_v<M, infra::RealTimeApp<typename M::EnvironmentType>>) {
        infra::terminationController(infra::TerminateAfterDuration {
            std::chrono::seconds(std::max(timeoutSec,sleepSec)+1)
        });
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "Usage: timeout_test timeout_sec sleep_sec [real_time|single_pass_iteration|typecheck]\n";
        return 1;
    }
    int timeoutSec = std::atoi(argv[1]);
    int sleepSec = std::atoi(argv[2]);

    std::string mode = "real_time";
    if (argc >= 4) {
        mode = argv[3];
    }
    if (mode != "real_time" && mode != "single_pass_iteration" && mode != "typecheck") {
        std::cerr << "Wrong mode '" << mode << "'";
        return 1;
    }

    if (mode == "real_time") {
        run<infra::RealTimeApp<infra::Environment<
            infra::CheckTimeComponent<true>,
            infra::TrivialExitControlComponent,
            basic::TimeComponentEnhancedWithSpdLogging<
                basic::real_time_clock::ClockComponent
            >,
            transport::CrossGuidComponent
        >>>(timeoutSec, sleepSec);
    } else if (mode == "single_pass_iteration") {
        run<infra::SinglePassIterationApp<infra::Environment<
            infra::CheckTimeComponent<true>,
            infra::TrivialExitControlComponent,
            basic::TimeComponentEnhancedWithSpdLogging<
                basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>
            >,
            transport::CrossGuidComponent
        >>>(timeoutSec, sleepSec);
    } else if (mode == "typecheck") {
        run<infra::BasicWithTimeApp<infra::Environment<
            infra::CheckTimeComponent<true>,
            infra::TrivialExitControlComponent,
            basic::TimeComponentEnhancedWithSpdLogging<
                basic::empty_clock::ClockComponent<std::chrono::system_clock::time_point>
            >,
            transport::CrossGuidComponent
        >>>(timeoutSec, sleepSec);
    }
}