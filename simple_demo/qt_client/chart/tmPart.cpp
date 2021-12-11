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
#include <tm_kit/transport/MultiTransportBroadcastListenerManagingUtils.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>

#include "LineSeries.hpp"
#include "EnablePanel.hpp"
#include "../../data_structures/InputDataStructure.hpp"
#include "../../data_structures/ConfigureStructures.hpp"

#include <QMetaObject>

using namespace dev::cd606::tm;

using InputData = basic::proto_interop::Proto<simple_demo::InputDataPOCO>;
using ConfigureCommand = basic::proto_interop::Proto<simple_demo::ConfigureCommandPOCO>;
using ConfigureResult = basic::proto_interop::Proto<simple_demo::ConfigureResultPOCO>;

using Env = infra::Environment<
    infra::CheckTimeComponent<true>
    , infra::FlagExitControlComponent
    , basic::TimeComponentEnhancedWithSpdLogging<
        basic::real_time_clock::ClockComponent
    >
    , transport::CrossGuidComponent
    , transport::ClientSideSimpleIdentityAttacherComponent<std::string,ConfigureCommand>
    , transport::AllNetworkTransportComponents
>;

using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;
using GL = infra::GenericLift<M>;

namespace tm_part {
    void setup(LineSeries *ls, EnablePanel *ep) {
        Env *env = new Env();
        R *r = new R(env);

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
        auto sharer = GL::lift(basic::CommonFlowUtilComponents<M>::shareBetweenDownstream<transport::HeartbeatMessage>());
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
        auto configureTrigger = M::triggerImporter<bool>();
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
                                       , {sharedHeartbeatSource.clone(), "heartbeatEnableStatusOutput"}
                                       , {"configureTrigger", std::get<0>(configureTrigger)}
                                       , {"createConfigureKey", [](bool &&x) -> M::Key<ConfigureCommand> {
                                              return M::Key<ConfigureCommand>(ConfigureCommand {{x}});
                                          }}
                                       , {"displayConfigureResult", [ep](M::KeyedData<ConfigureCommand,ConfigureResult> &&data) {
                                              bool enabled = data.data->enabled;
                                              QMetaObject::invokeMethod(ep, [ep,enabled]() {
                                                  ep->updateStatus(enabled);
                                              });
                                          }}
                                       , {"configureTrigger", "createConfigureKey"}
                                       , {"createConfigureKey", configureFacilityConnector, "displayConfigureResult"}
                                   })(*r);
        r->finalize();
    }
}
