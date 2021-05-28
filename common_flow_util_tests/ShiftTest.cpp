#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/SinglePassIterationApp.hpp>
#include <tm_kit/infra/TopDownSinglePassIterationApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/basic/IntIDComponent.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

using namespace dev::cd606::tm::infra;

struct TrivialLoggingComponent {
    static inline void log(LogLevel l, std::string const &s) {
        std::cout << l << ": " << s << std::endl;
    }
};

template <class TimeComponent>
using BasicEnvironment = Environment<
    dev::cd606::tm::basic::IntIDComponent<uint8_t>,
    CheckTimeComponent<true>,
    FlagExitControlComponent,
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
    using M = TopDownSinglePassIterationApp<BasicEnvironment<FakeClockComponent>>;
    using R = AppRunner<M>;
    M::EnvironmentType env;
    R r(&env);

    using CFU = dev::cd606::tm::basic::CommonFlowUtilComponents<M>;

    auto source = M::simpleImporter<int>(
        [](M::StateType *env) -> M::Data<int> {
            static int ii = -1;
            ++ii;
            std::variant<int,int,std::string> x;
            return M::InnerData<int> {
                env 
                , {
                    (M::TimePoint) ii
                    , ii
                    , ii >= 10
                }
            };
        }
    );
    r.registerImporter("source", source);

    //auto action = CFU::leftShift<int>(2);
    auto action = CFU::rightShift<int>(2);
    r.registerAction("action", action);

    auto bunch = CFU::bunch<int>();
    r.registerAction("bunch", bunch);

    auto print = M::simpleExporter<std::vector<int>>(
        [](M::InnerData<std::vector<int>> &&d) {
            std::ostringstream oss;
            oss << "Time " << d.timedData.timePoint
                << ", data [";
            for (auto x : d.timedData.value) {
                oss << x << ' ';
            }
            oss << "]" << (d.timedData.finalFlag?", final":"");
            d.environment->log(LogLevel::Info, oss.str());
        }
    );
    r.registerExporter("print", print);

    r.execute(bunch, r.execute(action, r.importItem(source)));
    r.execute(bunch, r.importItem(source));
    r.exportItem(print, r.actionAsSource(bunch));

    r.finalize();
}