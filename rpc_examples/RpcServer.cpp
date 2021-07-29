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

    auto simple = rpc_examples::simpleFacility<R>();
    infra::DeclarativeGraph<R>("", {
        {"simple", simple}
    })(r);
    transport::MultiTransportFacilityWrapper<R>::wrap<rpc_examples::Input,rpc_examples::Output>(
        r 
        , simple
        , "redis://127.0.0.1:6379:::rpc_example_simple"
        , "simple_wrapper"
    );

    auto clientStream = rpc_examples::clientStreamFacility<R>();
    infra::DeclarativeGraph<R>("", {
        {"clientStream", clientStream}
    })(r);
    transport::MultiTransportFacilityWrapper<R>::wrap<rpc_examples::Input,rpc_examples::Output>(
        r 
        , clientStream
        , "redis://127.0.0.1:6379:::rpc_example_client_stream"
        , "client_stream_wrapper"
    );

    auto serverStream = rpc_examples::serverStreamFacility<R>();
    infra::DeclarativeGraph<R>("", {
        {"serverStream", serverStream}
    })(r);
    transport::MultiTransportFacilityWrapper<R>::wrap<rpc_examples::Input,rpc_examples::Output>(
        r 
        , serverStream
        , "redis://127.0.0.1:6379:::rpc_example_server_stream"
        , "server_stream_wrapper"
    );

    auto bothStream = rpc_examples::bothStreamFacility<R>();
    infra::DeclarativeGraph<R>("", {
        {"bothStream", bothStream}
    })(r);
    transport::MultiTransportFacilityWrapper<R>::wrap<rpc_examples::Input,rpc_examples::Output>(
        r 
        , bothStream
        , "redis://127.0.0.1:6379:::rpc_example_both_stream"
        , "both_stream_wrapper"
    );

    r.finalize();
    infra::terminationController(infra::RunForever {});
}