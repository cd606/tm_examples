#include "EnablerGUIDataFlow.hpp"
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListenerManagingUtils.hpp>
#include "simple_demo/security_logic/DHClientSecurityCombination.hpp"

void enablerGUIDataFlow(
    R &r
    , std::string const &clientName
    , R::Sourceoid<ConfigureCommand> configureSource
    , R::Sinkoid<bool> statusSink
) {
    std::array<unsigned char, 64> my_private_key {
        0xCB,0xEC,0xAD,0x2B,0xA0,0x0A,0x19,0x9F,0xE1,0x3B,0x81,0x33,0x51,0xC4,0xFC,0x05,
        0x0D,0xCF,0x48,0xF1,0x6E,0x77,0xCD,0x67,0xB6,0xA7,0xB7,0xD5,0x6D,0x66,0x58,0xF6,
        0x3E,0xCD,0x80,0x72,0x7F,0x86,0xB2,0x22,0xB8,0xDB,0x46,0x3F,0x5C,0x75,0x74,0x54,
        0x96,0x14,0x08,0x35,0xB6,0x18,0xFE,0xCD,0xB6,0xC2,0xC3,0xCA,0xB5,0x3E,0xEC,0x0C
    };
    r.environment()->transport::security::ClientSideSignatureBasedIdentityAttacherComponent<ConfigureCommand>::operator=(
        transport::security::ClientSideSignatureBasedIdentityAttacherComponent<ConfigureCommand>(
            clientName
            , my_private_key
        )
    );

    r.environment()->setLogFilePrefix(clientName, true);

    std::array<unsigned char, 32> heartbeat_sign_pub_key { 
        0xDA,0xA0,0x15,0xD4,0x33,0xE8,0x92,0xC9,0xF2,0x96,0xA1,0xF8,0x1F,0x79,0xBC,0xF4,
        0x2D,0x7A,0xDE,0x48,0x03,0x47,0x16,0x0C,0x57,0xBD,0x1F,0x45,0x81,0xB5,0x18,0x2E 
    };

    auto hook = clientSideHeartbeatHook<R>(r, heartbeat_sign_pub_key, "testkey");

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
                    , "simple_demo.secure_executables.#.heartbeat"
                }
            }
            , "heartbeatListeners"
            , [hook](std::string const &s) -> std::optional<transport::WireToUserHook> {
                if (s == "heartbeatListener") {
                    return hook;
                } else {
                    return std::nullopt;
                }
            }
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
            , std::regex("simple_demo secure MainLogic")
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