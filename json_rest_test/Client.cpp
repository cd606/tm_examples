#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/TLSConfigurationComponent.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>

#include "Data.hpp"

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
    , transport::ClientSideSimpleIdentityAttacherComponent<
        std::string, Req
    >
>;

int main(int argc, char **argv) {
    Env env;

    bool useSsl = (argc>=2 && std::string_view(argv[1]) == "ssl");
    if (useSsl) {
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
    }

    for (int ii=0; ii<10; ++ii) {
        Req req {
            {"abc", "def"}
            , 2.0
            , decltype(req.tChoice) {std::in_place_index<0>, 1}
        };
        auto resp = transport::OneShotMultiTransportRemoteFacilityCall<Env>
            ::callWithProtocol<std::void_t, Req, Resp>
            (
                &env 
                , "json_rest://localhost:34567:user2:abcde:/test_facility"
                , std::move(req)
                , std::nullopt 
                , false
            ).get();
        std::cout << resp << '\n';
    }
}