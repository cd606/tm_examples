#include "EnablerGUIDataFlow.hpp"
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListenerManagingUtils.hpp>

void enablerGUIDataFlow(
    R &r
    , std::string const &clientName
    , R::Sourceoid<ConfigureCommand> configureSource
    , R::Sinkoid<bool> statusSink
) {
    r.environment()->transport::ClientSideSimpleIdentityAttacherComponent<std::string,ConfigureCommand>::operator=(
        transport::ClientSideSimpleIdentityAttacherComponent<std::string,ConfigureCommand>(
            clientName
        )
    ); 
    r.environment()->setLogFilePrefix(clientName);

    auto heartbeatListener = std::get<0>(
        transport::MultiTransportBroadcastListenerManagingUtils<R>
        ::setupBroadcastListeners<
            transport::HeartbeatMessage
        >(
            r 
            , {
                {
                    "heartbeatListener"
                    , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
                    , "simple_demo.plain_executables.#.heartbeat"
                }
            }
            , "heartbeatListeners"
        )
    );
    auto configureFacilityConnector = std::get<0>(std::get<1>(
        transport::MultiTransportRemoteFacilityManagingUtils<R>
        ::SetupRemoteFacilities<
            std::tuple<>
            , std::tuple<
                std::tuple<std::string, ConfigureCommand, ConfigureResult>
            >
        >::run(
            r 
            , heartbeatListener
            , std::regex("simple_demo plain MainLogic")
            , {"cfgFacility"}
            , std::chrono::seconds(3)
            , std::chrono::seconds(5)
            , {}
            , {}
            , {"main logic configure facility"}
            , "facilities"
        )
    ));

    auto keyify = M::template kleisli<ConfigureCommand>(
        basic::CommonFlowUtilComponents<M>::template keyify<ConfigureCommand>()
    );
    r.registerAction("keyify", keyify);
    configureSource(r, r.actionAsSink(keyify));

    auto configureResultHandler = M::liftPure<M::KeyedData<ConfigureCommand,ConfigureResult>>(
        [](M::KeyedData<ConfigureCommand,ConfigureResult> &&d) {
            return d.data.enabled();
        }
    );
    r.registerAction("configureResultHandler", configureResultHandler);

    configureFacilityConnector(r, r.actionAsSource(keyify), r.actionAsSink(configureResultHandler));
 
    statusSink(r, r.actionAsSource(configureResultHandler));

    auto heartbeatHandler = M::liftMaybe<transport::HeartbeatMessage>(
        [](transport::HeartbeatMessage &&m) -> std::optional<bool> {
            auto status = m.status("calculation_status");
            if (status) {
                return (status->info == "enabled");
            } else {
                return std::nullopt;
            }
        }
    );
    r.registerAction("heartbeatHandler", heartbeatHandler);
    heartbeatListener(r, r.actionAsSink(heartbeatHandler));
    statusSink(r, r.actionAsSource(heartbeatHandler));
}