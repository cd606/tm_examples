#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/TopDownSinglePassIterationApp.hpp>
#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/GraphCheckComponents.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/top_down_single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>

using namespace dev::cd606::tm;

using Env = infra::Environment<
    infra::CheckTimeComponent<true>
    , infra::graph_check_components::DontCheckCycles
    , infra::TrivialExitControlComponent
    , basic::TimeComponentEnhancedWithSpdLogging<
        basic::real_time_clock::ClockComponent
        /*basic::top_down_single_pass_iteration_clock::ClockComponent<
            std::chrono::system_clock::time_point
        >*/
    >
>;
using M = infra::RealTimeApp<Env>;
//using M = infra::TopDownSinglePassIterationApp<Env>;
using R = infra::AppRunner<M>;

int main() {
    Env env;
    R r(&env);

    infra::DeclarativeGraph<R>("", {
        {"importer", M::constFirstPushImporter<double>(0.0)}
        , {"action", [](double x) -> std::optional<double> {
            if (x < 100.0) {
                return x+1.0;
            } else {
                return std::nullopt;
            }
        }}
        , {"delayer", basic::CommonFlowUtilComponents<M>::delayer<double>(
            std::chrono::milliseconds(100)
        )}
        , {"exporter", [&env](double x) {
            env.log(infra::LogLevel::Info, std::to_string(x));
        }}
        , infra::DeclarativeGraphChain {{"importer","action","exporter"}}
        , {"action", "delayer"}
        , {"delayer", "action"}
    })(r);
    r.finalize();

    infra::terminationController(infra::RunForever {});

    return 0;
}