#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>
#include <tm_kit/transport/TLSConfigurationComponent.hpp>

using namespace dev::cd606::tm;

int main() {
    using Env = infra::Environment<
        infra::CheckTimeComponent<false>
        , infra::TrivialExitControlComponent
        , basic::TimeComponentEnhancedWithSpdLogging<
            basic::real_time_clock::ClockComponent
        >
        , transport::CrossGuidComponent 
        , transport::AllNetworkTransportComponents
        , transport::TLSClientConfigurationComponent
    >;
    Env env;
    env.transport::TLSClientConfigurationComponent::setConfigurationItem(
        transport::TLSClientInfoKey {"localhost", 56790}
        , transport::TLSClientInfo {
            "../grpc_interop_test/DotNetServer/server.crt"
        }
    );
    auto token = transport::OneShotMultiTransportRemoteFacilityCall<Env>
        ::call<transport::json_rest::TokenAuthenticationRequest, transport::json_rest::RawString>
        (
            &env 
            , "json_rest://localhost:56790:::"+transport::json_rest::JsonRESTComponent::TOKEN_AUTHENTICATION_REQUEST
            , transport::json_rest::TokenAuthenticationRequest {
                "user2"
                , "abcde"
            }
        );
    auto tokenStr = *(token.get());
    std::cout << tokenStr << '\n';   
    auto res = transport::OneShotMultiTransportRemoteFacilityCall<Env>
        ::call<basic::VoidStruct,std::string>
        (
            &env
            , "json_rest://localhost:56790:::/key_query[auth_token="+tokenStr+"]"
            , basic::VoidStruct {}
        );
    std::cout << res.get() << '\n';
}