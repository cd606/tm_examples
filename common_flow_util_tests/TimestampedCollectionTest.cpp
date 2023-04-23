#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/SinglePassIterationApp.hpp>
#include <tm_kit/infra/TopDownSinglePassIterationApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/basic/IntIDComponent.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>
#include <tm_kit/basic/TimestampedCollectionImporter.hpp>

using namespace dev::cd606::tm;

struct TrivialLoggingComponent {
    static inline void log(infra::LogLevel l, std::string const &s) {
        std::cout << l << ": " << s << std::endl;
    }
};

template <class TimeComponent>
using BasicEnvironment = infra::Environment<
    basic::IntIDComponent<uint8_t>,
    infra::CheckTimeComponent<true>,
    infra::FlagExitControlComponent,
    TrivialLoggingComponent,
    TimeComponent
    >;

struct FakeClockComponent {
    using TimePointType = uint64_t;
    static constexpr bool PreserveInputRelativeOrder = true;
    static uint64_t resolveTime() {
        return 0;
    }
    static uint64_t resolveTime(uint64_t triggeringInputTime) {
        return triggeringInputTime;
    }
};

int main() {
    using M = infra::TopDownSinglePassIterationApp<BasicEnvironment<FakeClockComponent>>;
    using R = infra::AppRunner<M>;
    M::EnvironmentType env;
    R r(&env);

    infra::DeclarativeGraph<R>("", {
        { "source", basic::TimestampedCollectionImporterFactory<M>
            ::createImporter(
                std::vector<int> {1, 2, 3}
                , [](int const &x) -> uint64_t {
                    return x;
                }
            )
        }
        , {"print", [](M::InnerData<int> &&d) {
            std::ostringstream oss;
            oss << "Time " << d.timedData.timePoint
                << ", data " << d.timedData.value
                << (d.timedData.finalFlag?", final":"");
            d.environment->log(infra::LogLevel::Info, oss.str());
        } }
        , {"source", "print"}
    })(r);

    r.finalize();
}