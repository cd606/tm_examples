#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/GenericLift.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/TLSConfigurationComponent.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>

#include "FacilityData.hpp"

using namespace dev::cd606::tm;

using Env = infra::Environment<
    infra::CheckTimeComponent<false>
    , infra::TrivialExitControlComponent
    , basic::TimeComponentEnhancedWithSpdLogging<
        basic::real_time_clock::ClockComponent
        , false
    >
    , transport::CrossGuidComponent
    , transport::TLSServerConfigurationComponent
    , transport::AllNetworkTransportComponents
>;
using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;
using GL = infra::GenericLift<M>;

int main(int argc, char **argv) {
    Env env;

    env.transport::TLSServerConfigurationComponent::setConfigurationItem(
        transport::TLSServerInfoKey {
            12345
        }
        , transport::TLSServerInfo {
            "../grpc_interop_test/DotNetServer/server.crt"
            , "../grpc_interop_test/DotNetServer/server.key"
        }
    );

    R r(&env);

    auto facility = GL::lift(infra::LiftAsFacility {}, [](Query &&q) -> Response {
        std::time_t t = std::chrono::system_clock::to_time_t(q.now);
        std::tm *m = std::localtime(&t);
        return Response {
            *m 
            , q.textData+":"+std::to_string(q.intData)
            , q.doubleData
        };
    });
    r.registerOnOrderFacility("facility", facility);
    transport::MultiTransportFacilityWrapper<R>::wrapWithProtocol<
        std::void_t, Query, Response
    >(
        r, facility, "websocket://localhost:12345:::/test", "wrapper"
    );
    
    r.finalize();
    infra::terminationController(infra::RunForever {});
}