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
    , transport::TLSServerConfigurationComponent
    , transport::web_socket::WebSocketComponent
>;
using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;

int main(int argc, char **argv) {
    Env env; 
    R r(&env);

    const std::array<unsigned char, 32> pk {0xa7, 0x89, 0x0f, 0x04, 0x12, 0xf0, 0xa3, 0x74, 0x3e, 0x10, 0x23, 0x81, 0x4a, 0x6c, 0x32, 0xa9, 0xc4, 0x5d, 0xb2, 0x0d, 0xfe, 0x32, 0xb9, 0x2e, 0xfa, 0x33, 0x16, 0x64, 0xdc, 0x8e, 0xdf, 0xe2};

    env.transport::TLSServerConfigurationComponent::setConfigurationItem(
        transport::TLSServerInfoKey {
            34567
        }
        , transport::TLSServerInfo {
            "../grpc_interop_test/DotNetServer/server.crt"
            , "../grpc_interop_test/DotNetServer/server.key"
        }
    );

    transport::security::SignatureHelper::Verifier verifier;
    verifier.addKey("auth_key", pk);

    auto publisher = transport::web_socket::WebSocketImporterExporter<Env>
        ::createTypedExporter<basic::nlohmann_json_interop::Json<Data>>
        (
            transport::ConnectionLocator::parse("localhost:34567[ignoreTopic=true]")
            , std::nullopt
            , ""
            , [&env,&verifier]() {
                return [&env,&verifier](basic::ByteDataView const &input, std::atomic<bool> &allowPublish)
                    -> std::optional<basic::ByteData>
                {
                    env.log(infra::LogLevel::Info, "Got input");
                    auto ver = verifier.verify(input);
                    if (ver) {
                        if (std::get<1>(*ver).content == "auth") {
                            env.log(infra::LogLevel::Info, "Verified 'auth' from '"+std::get<0>(*ver)+"'");
                            allowPublish = true;
                        } else {
                            env.log(infra::LogLevel::Error, "verified but the message is not correct");
                            allowPublish = false;
                        }
                    } else {
                        env.log(infra::LogLevel::Error, "Not verified");
                        allowPublish = false;
                    }
                    return std::nullopt;
                };
            }
        );
    r.registerExporter("publisher", publisher);
    r.connectTypedSinkToAllNodes(r.exporterAsSink(publisher));

    auto timerInput = basic::real_time_clock::ClockImporter<Env>
        ::createRecurringClockImporter<basic::TypedDataWithTopic<basic::nlohmann_json_interop::Json<Data>>>
    (
        std::chrono::system_clock::now()
        , std::chrono::system_clock::now()+std::chrono::minutes(1)
        , std::chrono::seconds(2)
        , [](std::chrono::system_clock::time_point const &t) -> basic::TypedDataWithTopic<basic::nlohmann_json_interop::Json<Data>> {
            static int ii=0;
            ++ii;
            std::variant<int,float> v;
            if (ii%2==0) {
                v.emplace<0>(ii);
            } else {
                v.emplace<1>(ii+0.5f);
            }
            return {
                "test"
                , {{
                    t 
                    , "abc"
                    , (uint64_t) (ii*2)
                    , {ii*3.0, ii*4.0, ii*5.0}
                    , v
                    , {(ii%2==0),ii}
                }}
            };
        }
    );

    r.registerImporter("timerInput", timerInput);

    r.finalize();

    infra::terminationController(infra::TerminateAfterDuration {std::chrono::seconds(62)});

    return 0;
}
