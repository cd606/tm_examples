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
        {"triggerTimer", basic::real_time_clock::ClockImporter<Env>::createRecurringClockConstImporter<basic::VoidStruct>(
            std::chrono::system_clock::now()
            , std::chrono::system_clock::now()+std::chrono::hours(1)
            , std::chrono::seconds(1)
            , basic::VoidStruct {}
        )}
        , {"dataSource", basic::real_time_clock::ClockImporter<Env>::createVariableDurationRecurringClockImporter<std::tuple<std::string,double>>(
            std::chrono::system_clock::now()
            , std::chrono::system_clock::now()+std::chrono::hours(1)
            , [](std::chrono::system_clock::time_point const &t) -> std::chrono::system_clock::duration {
                return std::chrono::milliseconds(std::rand()%500);
            }
            , [](std::chrono::system_clock::time_point const &t) -> std::tuple<std::string, double> {
                static std::string names[3] = {"A", "B", "C"};
                int which = std::rand()%3;
                return {names[which], (std::rand()%1000)*0.001};
            }
        )}
        , {"updates", basic::CommonFlowUtilComponents<M>::KeyedUpdateGenerator<
            std::tuple<std::string, double>
        >::keyedUpdateGenerator(
            [](std::tuple<std::string, double> const &x) {
                return std::get<0>(x);
            }
            , [](std::tuple<std::string, double> &&x) {
                return std::get<1>(x);
            }
            , std::chrono::milliseconds(700)
        )}
        , {"printer", [&env](std::vector<std::tuple<std::string,double>> &&v) {
            if (!v.empty()) {
                std::ostringstream oss;
                oss << "Got [";
                for (int ii=0; ii<v.size(); ++ii) {
                    if (ii != 0) {
                        oss << ',';
                    }
                    oss << std::get<0>(v[ii]) << '=' << std::get<1>(v[ii]);
                }
                oss << "]";
                env.log(infra::LogLevel::Info, oss.str());
            }
        }}
        , {"dataSource", "updates", 0}
        , {"triggerTimer", "updates", 1}
        , {"updates", "printer"}
    })(r);
    
    r.finalize();
    infra::terminationController(infra::TerminateAfterDuration {std::chrono::hours(1)});
}