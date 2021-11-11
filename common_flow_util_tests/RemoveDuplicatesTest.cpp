#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

using namespace dev::cd606::tm;

using Env = infra::Environment<
    infra::CheckTimeComponent<false>
    , infra::TrivialExitControlComponent
    , basic::TimeComponentEnhancedWithSpdLogging<
        basic::real_time_clock::ClockComponent
    >
>;
using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;

int main() {
    std::srand(std::time(nullptr));

    Env env;
    R r(&env);

    infra::DeclarativeGraph<R>("", {
        {"dataSource", basic::real_time_clock::ClockImporter<Env>::createVariableDurationRecurringClockImporter<std::tuple<std::string,double>>(
            std::chrono::system_clock::now()
            , std::chrono::system_clock::now()+std::chrono::hours(1)
            , [](std::chrono::system_clock::time_point const &t) -> std::chrono::system_clock::duration {
                return std::chrono::milliseconds(std::rand()%500);
            }
            , [](std::chrono::system_clock::time_point const &t) -> std::tuple<std::string, double> {
                static int ii=0;
                ++ii;
                return {(ii%2==0)?"A":"B", (ii/5)};
            }
        )}
        , {"updates", basic::CommonFlowUtilComponents<M>::RemoveDuplicates<
            std::tuple<std::string, double>
        >::removeDuplicates(
            [](std::tuple<std::string, double> const &x) {
                return std::get<0>(x);
            }
            , [](std::tuple<std::string, double> &&x) {
                return std::get<1>(x);
            }
        )}
        , {"printer", [&env](std::tuple<std::string,double> &&v) {
            std::ostringstream oss;
            oss << "Got " << std::get<0>(v) << '=' << std::get<1>(v);
            env.log(infra::LogLevel::Info, oss.str());
        }}
        , {"dataSource", "updates"}
        , {"updates", "printer"}
    })(r);
    
    r.finalize();
    infra::terminationController(infra::TerminateAfterDuration {std::chrono::hours(1)});
}