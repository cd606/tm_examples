#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/TLSConfigurationComponent.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacility.hpp>

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
    , transport::TLSClientConfigurationComponent
    , transport::AllNetworkTransportComponents
>;

int main(int argc, char **argv) {
    Env env;

    env.transport::TLSClientConfigurationComponent::setConfigurationItem(
        transport::TLSClientInfoKey {
            "localhost", 12345
        }
        , transport::TLSClientInfo {
            "../grpc_interop_test/DotNetServer/server.crt"
            , ""
            , ""
        }
    );
    
    for (int ii=0; ii<10; ++ii) {
        Query q {
            env.now()
            , "test"
            , 123
            , {1.0, 2.0, 3.0}
        };
        auto resp = transport::OneShotMultiTransportRemoteFacilityCall<Env>
            ::callWithProtocol<std::void_t, Query, Response>
            (
                &env 
                , "websocket://localhost:12345:::/test"
                , std::move(q)
                , std::nullopt 
                , false
            ).get();
        std::cout << resp << '\n';
    }
}