#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/TopDownSinglePassIterationApp.hpp>
#include <tm_kit/infra/KleisliSequence.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/basic/AppClockHelper.hpp>
#include <tm_kit/basic/top_down_single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>

using namespace dev::cd606::tm;

using Env = infra::Environment<
    infra::CheckTimeComponent<false>
    , infra::FlagExitControlComponent
    , basic::TimeComponentEnhancedWithSpdLogging<basic::top_down_single_pass_iteration_clock::ClockComponent<int>>
>;

using M = infra::TopDownSinglePassIterationApp<Env>;
using R = infra::AppRunner<M>;

using KS = infra::KleisliSequence<M>;

auto importer(Env *e) -> std::tuple<bool, M::Data<int>> {
    static int ii = 0;
    ++ii;
    return {
        (ii < 10)
        , M::InnerData<int> {
            e
            , {
                ii 
                , ii
                , (ii >= 10)
            }
        }
    };
}

void notMerged(R &r) {
    auto *env = r.environment();
    infra::DeclarativeGraph<R>("", {
        {"imp", &importer}
        , {"step1", [](std::tuple<M::TimePoint,int> &&x) -> std::optional<int> {
            return std::get<1>(x)*2;
        }}
        , {"step2", [](std::tuple<M::TimePoint,int> &&x) -> std::optional<int> {
            return std::get<1>(x)+1;
        }}
        , {"exporter", [env](int &&y) -> void {
            env->log(infra::LogLevel::Info, std::to_string(y));
        }}
        , infra::DeclarativeGraphChain {{"imp", "step1", "step2", "exporter"}}
    })(r);
}

void merged(R &r) {
    auto *env = r.environment();
    infra::DeclarativeGraph<R>("", {
        {"imp", &importer}
        , {"merged", KS::seq(
            [](std::tuple<M::TimePoint,int> &&x) -> std::optional<int> {
                return std::get<1>(x)*2;
            }
            , [](std::tuple<M::TimePoint,int> &&x) -> std::optional<int> {
                return std::get<1>(x)+1;
            }
            , [env](int &&y) -> void {
                env->log(infra::LogLevel::Info, std::to_string(y));
            }
        )}
        , infra::DeclarativeGraphChain {{"imp", "merged"}}
    })(r);
}

int main(int argc, char **argv) {
    Env env;
    R r(&env);
    if (argc > 1 && std::string_view(argv[1]) == "merged") {
        env.log(infra::LogLevel::Info, "merged");
        merged(r);
    } else {
        env.log(infra::LogLevel::Info, "not merged");
        notMerged(r);
    }
    r.finalize();
    return 0;
}