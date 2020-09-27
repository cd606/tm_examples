#ifndef DATA_DISPLAY_FLOW_HPP_
#define DATA_DISPLAY_FLOW_HPP_

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/CommonFlowUtils.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>

#include <tm_kit/transport/MultiTransportBroadcastListenerManagingUtils.hpp>

#include "defs.pb.h"

using namespace dev::cd606::tm;

template <class Env>
void dataDisplayFlow(
    infra::AppRunner<infra::RealTimeApp<Env>> &r
    , typename infra::AppRunner<infra::RealTimeApp<Env>>::template Sinkoid<simple_demo::InputData> sink
) 
{
    using R = infra::AppRunner<infra::RealTimeApp<Env>>;

    auto heartbeatListener = std::get<0>(
        transport::MultiTransportBroadcastListenerManagingUtils<R>
            ::template setupBroadcastListeners<transport::HeartbeatMessage>
        (
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

    auto inputDataSource = transport::MultiTransportBroadcastListenerManagingUtils<R>
        ::template setupBroadcastListenerThroughHeartbeat<simple_demo::InputData>
    (
        r 
        , heartbeatListener
        , std::regex("simple_demo DataSource")
        , "input data source"
        , "input.data"
        , "inputDataSourceComponents"
    );

    auto snapshotTimer = basic::real_time_clock::ClockImporter<Env>
        ::template createRecurringClockConstImporter<basic::VoidStruct>(
            r.environment()->now()
            , r.environment()->now()+std::chrono::hours(1)
            , std::chrono::milliseconds(100)
            , basic::VoidStruct {}
        );
    auto snapshotter = basic::CommonFlowUtilComponents<infra::RealTimeApp<Env>>
        ::template snapshotOnRight<simple_demo::InputData, basic::VoidStruct>
        (
            [](simple_demo::InputData &&a, basic::VoidStruct &&b) -> simple_demo::InputData {
                return std::move(a);
            }
        );
    r.registerImporter("snapshot_timer", snapshotTimer);
    r.registerAction("snapshotter", snapshotter);
    r.execute(snapshotter, r.importItem(snapshotTimer));
    r.execute(snapshotter, std::move(inputDataSource));
    sink(r, r.actionAsSource(snapshotter));
}

#endif