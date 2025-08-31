#include <tm_kit/infra/TopDownSinglePassIterationApp.hpp>
#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>

#include <tm_kit/basic/top_down_single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>

using namespace dev::cd606::tm;

using Env = infra::Environment<
    infra::CheckTimeComponent<false>
    , infra::FlagExitControlComponent
    , basic::TimeComponentEnhancedWithSpdLogging<
        basic::top_down_single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>
    >
>;
using M = infra::TopDownSinglePassIterationApp<Env>;
using R = infra::AppRunner<M>;

int main(int argc, char **argv) {
    Env env;
    env.log(infra::LogLevel::Info, "test {}", 5);
    env.log(infra::LogLevel::Info, "test 6");
#if 0
    R r(&env);
    infra::DeclarativeGraph<R>("", {
        {"importer", [](Env *env) -> std::tuple<bool, M::Data<std::vector<double>>> {
            static int kk=0;
            std::vector<double> ret;
            for (int ii=0; ii<1024*1024; ++ii) {
                ret.push_back((double) ii+kk);
            }
            ++kk;
            return {
                true
                , M::InnerData<std::vector<double>> {
                    env
                    , {
                        env->now()
                        , std::move(ret)
                        , false
                    }
                }
            };
        }}
        , {"discarder", [](std::vector<double> &&) {}}
        , {"discarder2", [](std::vector<double> &&) {}}
        , {"importer", "discarder"}
        , {"importer", "discarder2"}
    })(r);
    r.finalize();
    return 0;
#endif
}
