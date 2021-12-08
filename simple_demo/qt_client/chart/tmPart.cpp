#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/ChronoUtils.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/ProtoInterop.hpp>
#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListenerManagingUtils.hpp>

#include "LineSeries.hpp"
#include "SignalObject.hpp"
#include "../../data_structures/InputDataStructure.hpp"

using namespace dev::cd606::tm;

using Env = infra::Environment<
    infra::CheckTimeComponent<true>
    , infra::FlagExitControlComponent
    , basic::TimeComponentEnhancedWithSpdLogging<
        basic::real_time_clock::ClockComponent
    >
    , transport::CrossGuidComponent
    , transport::AllNetworkTransportComponents
>;

using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;

namespace tm_part {
    void setup(LineSeries *ls) {
        Env *env = new Env();
        R *r = new R(env);

        auto so = std::make_shared<SignalObject>();
        r->preservePointer(so);
        QObject::connect(so.get(), &SignalObject::valueUpdated, ls, &LineSeries::addValue, Qt::QueuedConnection);

        auto heartbeatListener = std::get<0>(
                transport::MultiTransportBroadcastListenerManagingUtils<R>
                    ::template setupBroadcastListeners<transport::HeartbeatMessage>
                (
                    *r
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
        auto inputDataSource = transport::MultiTransportBroadcastListenerManagingUtils<R>
                ::template setupBroadcastListenerThroughHeartbeat<
                    basic::proto_interop::Proto<simple_demo::InputDataPOCO>
                >
            (
                *r
                , heartbeatListener
                , std::regex("simple_demo DataSource")
                , "input data publisher"
                , "input.data"
                , "inputDataSourceComponents"
            );

        infra::DeclarativeGraph<R>(""
                                   , {
                                       {"output", [so](basic::proto_interop::Proto<simple_demo::InputDataPOCO> &&d) {
                                              so->setValue(d->value);
                                          }}
                                       , {inputDataSource.clone(), "output"}
                                   })(*r);
        r->finalize();
    }
}
