#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/AppRunnerUtils.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>

using namespace dev::cd606::tm;

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: timeout_test timeout_sec sleep_sec\n";
        return 1;
    }
    int timeoutSec = std::atoi(argv[1]);
    int sleepSec = std::atoi(argv[2]);

    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<
            basic::real_time_clock::ClockComponent
        >,
        transport::CrossGuidComponent
    >;

    TheEnvironment env;
    
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    R r(&env);
    auto primary = M::liftPure<int>([&env,sleepSec](int &&x) -> double {
        env.log(infra::LogLevel::Info, "Primary fires");
        std::this_thread::sleep_for(std::chrono::seconds(sleepSec));
        return x*3.0;
    }, infra::LiftParameters<M::TimePoint>().SuggestThreaded(true));
    auto secondary = M::liftPure<int>([&env](int &&x) -> double {
        env.log(infra::LogLevel::Info, "Secondary fires");
        return x*2.0;
    });

    auto importer = M::constFirstPushImporter<int>(1);
    auto exporter = M::pureExporter<double>([&env](double &&d) {
        env.log(infra::LogLevel::Info, ""+std::to_string(d));
    });

    auto pathway = basic::AppRunnerUtilComponents<R>::pathwayWithTimeout<int, double>(
        std::chrono::seconds(timeoutSec)
        , [primary](R &r, R::Source<int> &&source, R::Sink<double> const &sink) {
            r.connect(r.execute(primary, std::move(source)), sink);
        }
        , [secondary](R &r, R::Source<int> &&source, R::Sink<double> const &sink) {
            r.connect(r.execute(secondary, std::move(source)), sink);
        }
        , "timeout"
    );
    r.registerAction("secondary", secondary);
    r.registerAction("primary", primary);
    r.registerImporter("importer", importer);
    r.registerExporter("exporter", exporter);
    pathway(r, r.importItem(importer), r.exporterAsSink(exporter));

    std::ostringstream oss;
    r.writeGraphVizDescription(oss, "timeout_test");
    env.log(infra::LogLevel::Info, oss.str());

    r.finalize();

    infra::terminationController(infra::TerminateAfterDuration {
        std::chrono::seconds(std::max(timeoutSec,sleepSec)+1)
    });
}