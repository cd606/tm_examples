#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>

#include <tm_kit/transport/TLSConfigurationComponent.hpp>
#include <tm_kit/transport/websocket/WebSocketImporterExporter.hpp>
#include <tm_kit/transport/security/SignatureHelper.hpp>

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
    , transport::web_socket::WebSocketComponent
>;
using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;

int main(int argc, char **argv) {
    Env env; 
    R r(&env);

    const std::array<unsigned char, 64> sk {0x0d, 0xc7, 0x1c, 0xa9, 0x6d, 0xb4, 0x7b, 0xa6, 0xd6, 0x02, 0xb9, 0x14, 0x6e, 0x51, 0x66, 0xd9, 0x0b, 0x8d, 0x61, 0xf4, 0xe4, 0x05, 0x6a, 0x79, 0x0c, 0x67, 0x8b, 0x44, 0x34, 0x80, 0x41, 0x71, 0xa7, 0x89, 0x0f, 0x04, 0x12, 0xf0, 0xa3, 0x74, 0x3e, 0x10, 0x23, 0x81, 0x4a, 0x6c, 0x32, 0xa9, 0xc4, 0x5d, 0xb2, 0x0d, 0xfe, 0x32, 0xb9, 0x2e, 0xfa, 0x33, 0x16, 0x64, 0xdc, 0x8e, 0xdf, 0xe2};

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

    transport::security::SignatureHelper::Signer signer {sk};
    auto sig = signer.sign({std::string {argv[1]}});

    auto listener = transport::web_socket::WebSocketImporterExporter<Env>
        ::createTypedImporter<basic::nlohmann_json_interop::Json<Data>>
        (
            transport::ConnectionLocator::parse("localhost:34567[ignoreTopic=true]")
            , transport::web_socket::WebSocketComponent::NoTopicSelection {}
            , std::nullopt
            , {std::move(sig)}
        );
        
    infra::DeclarativeGraph<R>("", {
        {"printer", [](basic::TypedDataWithTopic<basic::nlohmann_json_interop::Json<Data>> &&x) {
            std::cout << *(x.content) << '\n';
        }}
        , {"listener", listener}
        , {"listener", "printer"}
    })(r);

    r.finalize();

    infra::terminationController(infra::RunForever {});

    return 0;
}
