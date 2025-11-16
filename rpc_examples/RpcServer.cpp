#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SerializationHelperMacros.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>

#include "RpcServer.hpp"

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

int main() {
    Environment env;
    R r(&env);

    using FacilityCreator = R::OnOrderFacilityPtr<rpc_examples::Input,rpc_examples::Output> (*)();
    std::vector<std::tuple<
        FacilityCreator,std::string
    >> specs {
        {&rpc_examples::simpleFacility<R>, "simple"}
        , {&rpc_examples::clientStreamFacility<R>, "client_stream"}
        , {&rpc_examples::serverStreamFacility<R>, "server_stream"}
        , {&rpc_examples::bothStreamFacility<R>, "both_stream"}
    };

    for (auto const &spec : specs) {
        auto f = std::get<0>(spec)();
        infra::DeclarativeGraph<R>("", {
            {std::get<1>(spec), f}
        })(r);
        transport::MultiTransportFacilityWrapper<R>::wrap<rpc_examples::Input,rpc_examples::Output>(
            r 
            , f
            //, "redis://127.0.0.1:6379:::rpc_example_"+std::get<1>(spec)
            , "nats://127.0.0.1:4222:::rpc_example_"+std::get<1>(spec)
            , std::get<1>(spec)+"_wrapper"
        );
    }

    r.finalize();
    infra::terminationController(infra::RunForever {});
}