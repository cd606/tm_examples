#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>
#include <tm_kit/infra/TerminationController.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>

#include "RpcInterface.hpp"

using namespace dev::cd606::tm;

using Environment = infra::Environment<
    infra::CheckTimeComponent<true>,
    infra::TrivialExitControlComponent,
    basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
    transport::CrossGuidComponent,
    transport::AllNetworkTransportComponents
>;
using M = infra::RealTimeApp<Environment>;
using R = infra::AppRunner<M>;
using CFU = basic::CommonFlowUtilComponents<M>;

int main() {
    Environment env;
    R r(&env);

    infra::DeclarativeGraph<R>("", {
        {"source", [](Environment *env) -> std::tuple<bool, rpc_examples::Input> {
            static int ii = -1;
            static const std::vector<rpc_examples::Input> values {
                {5, "abc"}
                , {-1, "bcd"}
                , {-2, "cde"}
                , {-3, "def"}
                , {-4, "efg"}
            };
            ++ii;
            return {ii < values.size()-1, values[ii]};
        }}
        , {"id", M::constFirstPushImporter<Environment::IDType>(env.new_id())}
    })(r);

    std::vector<std::tuple<
        std::string, std::string, bool
    >> descriptors {
        {"redis://127.0.0.1:6379:::rpc_example_simple", "SIMPLE RPC", false}
        , {"redis://127.0.0.1:6379:::rpc_example_client_stream", "CLIENT STREAM RPC", true}
        , {"redis://127.0.0.1:6379:::rpc_example_server_stream", "SERVER STREAM RPC", false}
        , {"redis://127.0.0.1:6379:::rpc_example_both_stream", "BOTH STREAM RPC", true}
    };
    for (auto const &desc : descriptors) {
        infra::DeclarativeGraph<R>("", {
            infra::DeclarativeGraph<R>(std::get<1>(desc), {
                {"passthrough", CFU::idFunc<rpc_examples::Input>(), infra::LiftParameters<M::TimePoint>().FireOnceOnly(!std::get<2>(desc))}
                , {"keyify", CFU::keyifyWithProvidedID<rpc_examples::Input>()}
                , {"facility", transport::MultiTransportRemoteFacilityManagingUtils<R>
                    ::setupSimpleRemoteFacility<rpc_examples::Input, rpc_examples::Output>(
                    std::get<0>(desc)
                )}
                , {"print", [&env,nm=std::get<1>(desc)](M::KeyedData<rpc_examples::Input,rpc_examples::Output> &&x) {
                    std::ostringstream oss;
                    oss << nm << ": " << x.data;
                    env.log(infra::LogLevel::Info, oss.str());
                }}
                , {"passthrough", "keyify", 0}
                , {"keyify", "facility", "print"}
            })
            , {"source", (std::get<1>(desc)+"/passthrough")}
            , {"id", (std::get<1>(desc)+"/keyify"), 1}
        })(r);
    }

    r.finalize();
    infra::terminationController(infra::TerminateAfterDuration {std::chrono::seconds(2)});
}