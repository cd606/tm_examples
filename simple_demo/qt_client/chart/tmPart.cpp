#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/ChronoUtils.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/ProtoInterop.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>
#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/security/SignatureBasedIdentityCheckerComponent.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListenerManagingUtils.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>
#include <tm_kit/transport/TLSConfigurationComponent.hpp>

#include "LineSeries.hpp"
#include "EnablePanel.hpp"
#include "../../data_structures/InputDataStructure.hpp"
#include "../../data_structures/ConfigureStructures.hpp"
#include "../../security_logic/EncAndSignHookFactory.hpp"

#include <QMetaObject>

using namespace dev::cd606::tm;

using InputData = basic::proto_interop::Proto<simple_demo::InputDataPOCO>;
using ConfigureCommand = basic::proto_interop::Proto<simple_demo::ConfigureCommandPOCO>;
using ConfigureResult = basic::proto_interop::Proto<simple_demo::ConfigureResultPOCO>;

using EnvBase = infra::Environment<
    infra::CheckTimeComponent<true>
    , infra::FlagExitControlComponent
    , basic::TimeComponentEnhancedWithSpdLogging<
        basic::real_time_clock::ClockComponent
    >
    , transport::CrossGuidComponent
    , transport::AllNetworkTransportComponents
>;

using PlainEnv = infra::Environment<
    EnvBase
    , transport::ClientSideSimpleIdentityAttacherComponent<std::string,ConfigureCommand>
>;
/* The data source heartbeat is not published with any hook,
 * but the main logic (enable/disable) heartbeat is published
 * with hook, the result is that we cannot use a hook factory
 * component for heartbeat message in the environment, instead,
 * we create hook and pass to listeners when needed
 */
using SecureEnv = infra::Environment<
    EnvBase
    , transport::security::ClientSideSignatureBasedIdentityAttacherComponent<ConfigureCommand>
    , transport::TLSClientConfigurationComponent
    , DecHookFactoryComponent<InputData>
>;

namespace tm_part {
    template <class R>
    void setup_graphPart(
            R &r
            , LineSeries *ls
            , EnablePanel *ep
            , typename R::template Source<InputData> &&inputDataSource
            , typename R::template Source<std::shared_ptr<transport::HeartbeatMessage const>> &&heartbeatSource
            , typename R::template FacilitioidConnector<ConfigureCommand, ConfigureResult> const &enableFacility
    ) {
        using M = typename R::AppType;

        auto configureTrigger = M::template triggerImporter<bool>();
        ep->connectSetStatusFunc(std::get<1>(configureTrigger));

        infra::DeclarativeGraph<R>(""
                                   , {
                                       {"chartDataOutput", [ls](InputData &&d) {
                                            auto v = d->value;
                                            QMetaObject::invokeMethod(ls, [ls,v]() {
                                                ls->addValue(v);
                                            });
                                          }}
                                       , {inputDataSource.clone(), "chartDataOutput"}
                                       , {"heartbeatEnableStatusOutput", [ep](std::shared_ptr<transport::HeartbeatMessage const> &&heartbeat) {
                                              auto status = heartbeat->status("calculation_status");
                                              if (status) {
                                                  bool enabled = (status->info == "enabled");
                                                  QMetaObject::invokeMethod(ep, [ep,enabled]() {
                                                      ep->updateStatus(enabled);
                                                  });
                                              }
                                          }}
                                       , {heartbeatSource.clone(), "heartbeatEnableStatusOutput"}
                                       , {"configureTrigger", std::get<0>(configureTrigger)}
                                       , {"createConfigureKey", [](bool &&x) -> typename M::template Key<ConfigureCommand> {
                                              return typename M::template Key<ConfigureCommand>(ConfigureCommand {{x}});
                                          }}
                                       , {"displayConfigureResult", [ep](typename M::template KeyedData<ConfigureCommand,ConfigureResult> &&data) {
                                              bool enabled = data.data->enabled;
                                              QMetaObject::invokeMethod(ep, [ep,enabled]() {
                                                  ep->updateStatus(enabled);
                                              });
                                          }}
                                       , {"configureTrigger", "createConfigureKey"}
                                       , {"createConfigureKey", enableFacility, "displayConfigureResult"}
                                   })(r);
    }

    template <class Env>
    void setup_internal(LineSeries *ls, EnablePanel *ep) {
        using M = infra::RealTimeApp<Env>;
        using R = infra::AppRunner<M>;
        using GL = infra::GenericLift<M>;

        Env *env = new Env();
        R *r = new R(env);

        if constexpr (std::is_same_v<Env,PlainEnv>) {
            env->transport::ClientSideSimpleIdentityAttacherComponent<std::string,ConfigureCommand>::operator=(
                transport::ClientSideSimpleIdentityAttacherComponent<std::string,ConfigureCommand>(
                    "qt_chart_enable_client"
                )
            );
            auto heartbeatSource = transport::MultiTransportBroadcastListenerManagingUtils<R>
                    ::template oneBroadcastListener<
                        transport::HeartbeatMessage
                    >
                (
                    *r
                    , "heartbeatListener"
                    , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
                    , "simple_demo.plain_executables.#.heartbeat"
                );
            auto sharer = GL::lift(basic::CommonFlowUtilComponents<M>::template shareBetweenDownstream<transport::HeartbeatMessage>());
            r->registerAction("sharer", sharer);
            auto sharedHeartbeatSource = r->execute(sharer, std::move(heartbeatSource));

            auto inputDataSource = transport::MultiTransportBroadcastListenerManagingUtils<R>
                    ::template setupBroadcastListenerThroughHeartbeat<
                        InputData
                    >
                (
                    *r
                    , sharedHeartbeatSource.clone()
                    , std::regex("simple_demo DataSource")
                    , "input data publisher"
                    , "input.data"
                    , "inputDataSourceComponents"
                );
            auto configureFacilityConnector = transport::MultiTransportRemoteFacilityManagingUtils<R>
                    ::template setupOneNonDistinguishedRemoteFacility<
                        ConfigureCommand, ConfigureResult
                    >
                (
                    *r
                    , sharedHeartbeatSource.clone()
                    , std::regex("simple_demo plain MainLogic")
                    , "cfgFacility"
                ).facility;

            setup_graphPart<R>(
                        *r, ls, ep
                        , inputDataSource.clone()
                        , sharedHeartbeatSource.clone()
                        , configureFacilityConnector
                        );
            r->finalize();
        } else {
            std::array<unsigned char, 64> my_private_key {
                0xCB,0xEC,0xAD,0x2B,0xA0,0x0A,0x19,0x9F,0xE1,0x3B,0x81,0x33,0x51,0xC4,0xFC,0x05,
                0x0D,0xCF,0x48,0xF1,0x6E,0x77,0xCD,0x67,0xB6,0xA7,0xB7,0xD5,0x6D,0x66,0x58,0xF6,
                0x3E,0xCD,0x80,0x72,0x7F,0x86,0xB2,0x22,0xB8,0xDB,0x46,0x3F,0x5C,0x75,0x74,0x54,
                0x96,0x14,0x08,0x35,0xB6,0x18,0xFE,0xCD,0xB6,0xC2,0xC3,0xCA,0xB5,0x3E,0xEC,0x0C
            };
            env->transport::security::ClientSideSignatureBasedIdentityAttacherComponent<ConfigureCommand>::operator=(
                transport::security::ClientSideSignatureBasedIdentityAttacherComponent<ConfigureCommand>(
                    my_private_key
                )
            );
            env->transport::TLSClientConfigurationComponent::setConfigurationItem(
                transport::TLSClientInfoKey {
                    "localhost", 56788
                }
                , transport::TLSClientInfo {
                    "../../../grpc_interop_test/DotNetServer/server.crt"
                    , ""
                    , ""
                }
            );

            std::thread th([env,r,ls,ep]() {
                auto firstDataSourceHeartbeat = transport::MultiTransportBroadcastFirstUpdateQueryManagingUtils<Env>
                        ::template fetchTypedFirstUpdateAndDisconnect<transport::HeartbeatMessage>
                    (
                        env
                        , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
                        , "simple_demo.secure_executables.data_source.heartbeat"
                    ).get().content;
                auto inputDataChannelIter = firstDataSourceHeartbeat.broadcastChannels().find("input data publisher");
                std::string inputDataChannel;
                if (
                   inputDataChannelIter != firstDataSourceHeartbeat.broadcastChannels().end()
                   && !inputDataChannelIter->second.empty()
                ) {
                    inputDataChannel = inputDataChannelIter->second.front();
                }
                auto keyQueryChannelIter = firstDataSourceHeartbeat.facilityChannels().find("keyQuery");
                std::string keyQueryChannel;
                if (
                   keyQueryChannelIter != firstDataSourceHeartbeat.facilityChannels().end()
                ) {
                    keyQueryChannel = keyQueryChannelIter->second;
                }
                if (inputDataChannel == "" || keyQueryChannel == "") {
                    std::cerr << "No data source information\n";
                    return;
                }
                auto keyQueryChannelParseRes = transport::parseMultiTransportRemoteFacilityChannel(keyQueryChannel);
                if (!keyQueryChannelParseRes || std::get<0>(*keyQueryChannelParseRes) != transport::MultiTransportRemoteFacilityConnectionType::JsonREST) {
                    std::cerr << "Data source key query information not present\n";
                    return;
                }
                auto keyQueryChannelLocator =
                        std::get<1>(*keyQueryChannelParseRes)
                        .modifyHost("localhost")
                        .modifyUserName("user2")
                        .modifyPassword("abcde");
                auto inputDataKey = transport::OneShotMultiTransportRemoteFacilityCall<Env>
                        ::template callWithTimeout<basic::VoidStruct, std::string>
                    (
                        env
                        , transport::MultiTransportRemoteFacilityConnectionType::JsonREST
                        , keyQueryChannelLocator
                        , basic::VoidStruct {}
                        , std::chrono::seconds(10)
                    );
                if (!inputDataKey) {
                    std::cerr << "Data source input data key not obtained\n";
                    return;
                }
                env->DecHookFactoryComponent<InputData>::setDecKey(*inputDataKey);

                VerifyAndDecHookFactoryComponent<transport::HeartbeatMessage> verifyAndDecHookFactory(
                    "testkey"
                    , std::array<unsigned char, 32> {
                        0xDA,0xA0,0x15,0xD4,0x33,0xE8,0x92,0xC9,0xF2,0x96,0xA1,0xF8,0x1F,0x79,0xBC,0xF4,
                        0x2D,0x7A,0xDE,0x48,0x03,0x47,0x16,0x0C,0x57,0xBD,0x1F,0x45,0x81,0xB5,0x18,0x2E
                    }
                );

                auto heartbeatSource = transport::MultiTransportBroadcastListenerManagingUtils<R>
                        ::template oneBroadcastListener<
                            transport::HeartbeatMessage
                        >
                    (
                        *r
                        , "heartbeatListener"
                        , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
                        , "simple_demo.secure_executables.main_logic.heartbeat"
                        , verifyAndDecHookFactory.defaultHook()
                    );
                auto sharer = GL::lift(basic::CommonFlowUtilComponents<M>::template shareBetweenDownstream<transport::HeartbeatMessage>());
                r->registerAction("sharer", sharer);
                auto sharedHeartbeatSource = r->execute(sharer, std::move(heartbeatSource));

                auto inputDataSource = transport::MultiTransportBroadcastListenerManagingUtils<R>
                        ::template oneBroadcastListener<
                            InputData
                        >
                    (
                        *r
                        , "inputDataListener"
                        , inputDataChannel
                    );
                auto configureFacilityConnector = transport::MultiTransportRemoteFacilityManagingUtils<R>
                        ::template setupOneNonDistinguishedRemoteFacility<
                            ConfigureCommand, ConfigureResult
                        >
                    (
                        *r
                        , sharedHeartbeatSource.clone()
                        , std::regex("simple_demo secure MainLogic")
                        , "cfgFacility"
                    ).facility;

                setup_graphPart<R>(
                            *r, ls, ep
                            , inputDataSource.clone()
                            , sharedHeartbeatSource.clone()
                            , configureFacilityConnector
                            );
                r->finalize();
            });
            th.detach();
        }
    }

    void setup(bool secureServers, LineSeries *ls, EnablePanel *ep) {
        if (secureServers) {
            setup_internal<SecureEnv>(ls, ep);
        } else {
            setup_internal<PlainEnv>(ls, ep);
        }
    }
}
