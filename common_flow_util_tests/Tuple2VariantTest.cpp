#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/basic/AppClockHelper.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>

using namespace dev::cd606::tm;

using Env = infra::Environment<
    infra::CheckTimeComponent<false>
    , infra::graph_check_components::CheckActionChains
    , infra::FlagExitControlComponent
    , basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent, false>
>;

using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;

int main() {
    Env env;
    R r(&env);

    infra::DeclarativeGraph<R>("", {
        {"importer", M::constFirstPushImporter<std::tuple<std::string, int>>({"abc", 10})}
        , {"t2v", M::dispatchTupleAction<std::string,int>()}
        , {"printer1", [&env](std::string &&s) {
            env.log(infra::LogLevel::Info, "string: "+s);
        }}
        , {"printer2", [&env](int &&x) {
            env.log(infra::LogLevel::Info, "string: "+std::to_string(x));
        }}
        , {"importer", "t2v"}
        , {"t2v", 0, "printer1"}
        , {"t2v", 1, "printer2"}
    })(r);
    r.finalize();

    infra::terminationController(infra::RunForever {});

    return 0;
}