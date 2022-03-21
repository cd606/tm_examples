#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>

#include <tm_kit/transport/TLSConfigurationComponent.hpp>
#include <tm_kit/transport/websocket/WebSocketLazyImporter.hpp>

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

    bool ssl = (argc > 1 && std::string_view(argv[1]) == "ssl");

    if (ssl) {
        transport::tls::markConnectionAsUsingTLS(
            &env
            , "ws.ifelse.io"
        );
    }

    auto action = transport::web_socket::WebSocketLazyImporter<Env>
        ::createLazyImporter(
            transport::ConnectionLocator::parse(
                ssl
                ?
                "ws.ifelse.io:443[ignoreTopic=true]"
                :
                "ws.ifelse.io:80[ignoreTopic=true]"
            )
            , transport::web_socket::WebSocketComponent::NoTopicSelection()
            , std::nullopt
            , [](basic::ByteDataView const &) -> std::optional<basic::ByteData> {
                static int ii=0;
                return basic::ByteData {std::to_string(++ii)};
            }
        );
       
    infra::DeclarativeGraph<R>("", {
        {"initial", M::constFirstPushImporter<basic::ByteData>(
            basic::ByteData {"testtest"}
        )}
        , {"action", action}
        , {"printer", [](basic::ByteDataWithTopic &&x) {
            std::cout << "Got '" << x.content << "'\n";
        }}
        , infra::DeclarativeGraphChain {{"initial", "action", "printer"}}
    })(r);

    r.finalize();

    infra::terminationController(infra::RunForever {});

    return 0;
}
