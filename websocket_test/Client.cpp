#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>

#include <tm_kit/transport/TLSConfigurationComponent.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListenerManagingUtils.hpp>

#include "Data.hpp"

using namespace dev::cd606::tm;

using Env = infra::Environment<
    infra::CheckTimeComponent<false>
    , infra::TrivialExitControlComponent
    , basic::TimeComponentEnhancedWithSpdLogging<
        basic::real_time_clock::ClockComponent
        , false
    >
    , transport::TLSClientConfigurationComponent
    , transport::AllNetworkTransportComponents
>;
using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;

int main(int argc, char **argv) {
    Env env; 
    R r(&env);

    env.transport::TLSClientConfigurationComponent::setConfigurationItem(
        transport::TLSClientInfoKey {
            "localhost", 34567
        }
        , transport::TLSClientInfo {
            "../grpc_interop_test/DotNetServer/server.crt"
            , ""
            , ""
        }
    );

    auto listener = transport::MultiTransportBroadcastListenerManagingUtils<R>
        ::oneBroadcastListenerWithProtocol<
            basic::nlohmann_json_interop::Json, Data
        >(
            r
            , "listener"
            , "websocket://localhost:34567[ignoreTopic=true]"
        );
        
    infra::DeclarativeGraph<R>("", {
        {"printer", [](Data &&x) {
            std::cout << x << '\n';
        }}
        , {listener.clone(), "printer"}
    })(r);

    r.finalize();

    infra::terminationController(infra::RunForever {});

    return 0;
}
