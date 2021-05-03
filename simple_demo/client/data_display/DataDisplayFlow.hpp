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
        , "input data publisher"
        , "input.data"
        , "inputDataSourceComponents"
    );
    //auto name = *(r.findFirstControllableNodeAtOrAbove(inputDataSource.registeredNodeName()));

    auto snapshotTimer = basic::real_time_clock::ClockImporter<Env>
        ::template createRecurringClockConstImporter<basic::VoidStruct>(
            r.environment()->now()
            , r.environment()->now()+std::chrono::hours(24)
            , std::chrono::milliseconds(100)
            , basic::VoidStruct {}
        );
    auto snapshotter = basic::CommonFlowUtilComponents<infra::RealTimeApp<Env>>
        ::template snapshotOnRight<simple_demo::InputData, basic::VoidStruct>
        (
            [/*&r, name*/](simple_demo::InputData &&a, basic::VoidStruct &&b) -> simple_demo::InputData {
                /*
                static unsigned count = 0;
                ++count;
                if (count == 30) {
                    r.environment()->log(infra::LogLevel::Info, "Stopping");
                    r.controlAll(name, "stop", {});
                } else if (count == 100) {
                    r.environment()->log(infra::LogLevel::Info, "Restarting");
                    r.controlAll(name, "restart", {});
                }
                */
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