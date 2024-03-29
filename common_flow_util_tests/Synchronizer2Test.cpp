#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/SinglePassIterationApp.hpp>
#include <tm_kit/infra/TopDownSinglePassIterationApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>
#include <tm_kit/basic/IntIDComponent.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>
#include <tm_kit/basic/AppRunnerUtils.hpp>
#include <tm_kit/basic/CounterComponent.hpp>
#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/BoostUUIDComponent.hpp>

using namespace dev::cd606::tm::infra;
using namespace dev::cd606::tm::basic;
using namespace dev::cd606::tm::transport;

struct TrivialLoggingComponent {
    static inline void log(LogLevel l, std::string const &s) {
        std::cout << l << ": " << s << std::endl;
    }
};

template <class TimeComponent>
using BasicEnvironment = Environment<
    CheckTimeComponent<true>,
    FlagExitControlComponent,
    TrivialLoggingComponent,
    TimeComponent,
    BoostUUIDComponent,
    CounterComponent<>
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

    using CFU = CommonFlowUtilComponents<M>;
    using ARU = AppRunnerUtilComponents<R>;
    
    using IntP = std::shared_ptr<const int>;

    DeclarativeGraph<R>("", {
        {"source", [](M::StateType *env) -> std::tuple<bool, M::Data<int>> {
            static int ii = -1;
            ++ii;
            return {(ii<10), {M::InnerData<int> {
                env 
                , {
                    (M::TimePoint) ii
                    , ii
                    , ii >= 10
                }
            }}};
        } }
        , {"share", CFU::shareBetweenDownstream<int>()}
        , {"facility1", LiftAsFacility {}, 
            [](IntP &&a) -> SingleLayerWrapperWithID<1,int> {
                return {*a+1};
            }
        }
        , {"facility2", LiftAsFacility {}, 
            [](IntP &&a) -> SingleLayerWrapperWithID<2,int> {
                return {*a+2};
            }
        }
        , {"keyify", CFU::keyifyThroughCounter<IntP>()}
        , {"print", [&env](M::KeyedData<IntP,std::tuple<int,int>> &&d) {
            std::ostringstream oss;
            oss << d.key.id() << ' ' << *(d.key.key()) << ": " << std::get<0>(d.data) << ", " << std::get<1>(d.data);
            env.log(LogLevel::Info, oss.str());
        }}
        , DeclarativeGraphChain {{"source", "share", "keyify"}}
    })(r);

    auto combinedFacility = ARU::facilitoidSynchronizer2<
        IntP
        , SingleLayerWrapperWithID<1,int>
        , SingleLayerWrapperWithID<2,int>
    >(
        r.facilitioidByName<IntP, SingleLayerWrapperWithID<1,int>>("facility1")        
        , r.facilitioidByName<IntP, SingleLayerWrapperWithID<2,int>>("facility2")
        , [](auto &&key, auto &&x1, auto &&x2) -> std::tuple<int,int> {
            return {std::move(x1.value), std::move(x2.value)};
        }
        , "combined"
    );

    combinedFacility(
        r
        , r.sourceByName<M::Key<IntP>>("keyify")
        , r.sinkByName<M::KeyedData<IntP,std::tuple<int,int>>>("print")
    );

    r.finalize();
}
