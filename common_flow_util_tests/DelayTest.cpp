#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>

#include <tm_kit/basic/AppClockHelper.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>

using namespace dev::cd606::tm;

using Env = infra::Environment<
    infra::CheckTimeComponent<true>
    , infra::FlagExitControlComponent
    , basic::TimeComponentEnhancedWithSpdLogging<
        basic::real_time_clock::ClockComponent
        , false 
    >
>;
using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;

int main() {
    Env env;
    R r(&env);

    (infra::DeclarativeGraph<R>("", {
        {"importer", basic::AppClockHelper<M>::Importer::createRecurringClockImporter<std::chrono::system_clock::time_point>(
            std::chrono::system_clock::now()
            , std::chrono::system_clock::now()+std::chrono::seconds(3)
            , std::chrono::milliseconds(200)
            , [](std::chrono::system_clock::time_point const &tp) {
                return tp;
            }
        )}
        , {"delayer", basic::CommonFlowUtilComponents<M>::delayer<std::chrono::system_clock::time_point>(std::chrono::seconds(2))}
        , {"exporter", [](M::InnerData<std::chrono::system_clock::time_point> &&x) {
            x.environment->log(infra::LogLevel::Info, infra::withtime_utils::localTimeString(x.timedData.value));
        }}
        , {"importer", "delayer"}
        , {"delayer", "exporter"}
    }))(r);
    r.finalize();

    infra::terminationController(infra::TerminateAfterDuration {std::chrono::seconds(10)});
}