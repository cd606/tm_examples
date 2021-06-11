#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>

#include <tm_kit/transport/zeromq/ZeroMQComponent.hpp>
#include <tm_kit/transport/zeromq/ZeroMQImporterExporter.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQImporterExporter.hpp>
#include <tm_kit/transport/multicast/MulticastComponent.hpp>
#include <tm_kit/transport/multicast/MulticastImporterExporter.hpp>
#include <tm_kit/transport/shared_memory_broadcast/SharedMemoryBroadcastComponent.hpp>
#include <tm_kit/transport/shared_memory_broadcast/SharedMemoryBroadcastImporterExporter.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>
#include <tm_kit/transport/MultiTransportTouchups.hpp>

#include "defs.pb.h"
#include "simple_demo_chain_version/external_logic/ExternalDataSource.hpp"

#include <iostream>
#include <sstream>

using namespace dev::cd606::tm;
using namespace simple_demo_chain_version;

using TheEnvironment = infra::Environment<
    infra::CheckTimeComponent<false>,
    infra::TrivialExitControlComponent,
    basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
    transport::zeromq::ZeroMQComponent,
    transport::rabbitmq::RabbitMQComponent,
    transport::multicast::MulticastComponent,
    transport::shared_memory_broadcast::SharedMemoryBroadcastComponent,
    transport::HeartbeatAndAlertComponent
>;
using M = infra::RealTimeApp<TheEnvironment>;

class ExternalDataSourceBridge final : public ExternalDataSourceListener {
private:
    std::function<void(InputData&&)> pub_;
public:
    ExternalDataSourceBridge(std::function<void(InputData&&)> pub) : pub_(pub) {}
    ~ExternalDataSourceBridge() {}
    virtual void onData(DataFromSource const &data) override final {
        InputData dataCopy;
        dataCopy.set_value(data.value);
        pub_(std::move(dataCopy));
    }
};

int main(int argc, char **argv) {
    bool same_host = false;
    if (argc > 1 && std::strcmp(argv[1], "help") == 0) {
        std::cout << "Usage: data_source [same_host]\n";
        return 0;
    }
    if (argc > 1 && std::strcmp(argv[1], "same_host") == 0) {
        same_host = true;
    }

    TheEnvironment env;

    env.setLogFilePrefix("simple_demo_chain_version_data_source_");
    
    using R = infra::AppRunner<M>;
    R r(&env);

    transport::HeartbeatAndAlertComponentTouchup<R>(r, {
#ifdef _MSC_VER
        "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
        , "simple_demo_chain_version.data_source.heartbeat"
        , "simple_demo_chain_version DataSource"
        , std::chrono::seconds(10)
        , "program"
#else
        .channelDescriptor = "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
        , .topic = "simple_demo_chain_version.data_source.heartbeat"
        , .identity = "simple_demo_chain_version DataSource"
        , .period = std::chrono::seconds(10)
        , .overallStatusEntry = "program"
#endif
    });
    transport::multi_transport_touchups::PublisherTouchup<R,InputData>(r, {
#ifdef _MSC_VER
        (same_host?"shared_memory_broadcast://::::simple_demo_chain_version_input_data":"multicast://224.0.1.101:12345")
#else
        .channelSpec = (same_host?"shared_memory_broadcast://::::simple_demo_chain_version_input_data":"multicast://224.0.1.101:12345")
#endif
    });

    auto addTopic = basic::SerializationActions<M>::template addConstTopic<InputData>("input.data");
    auto importerPair = M::triggerImporter<InputData>();

    r.execute("addTopic", addTopic
        , r.importItem("source", std::get<0>(importerPair)));

    std::ostringstream graphOss;
    r.writeGraphVizDescription(graphOss, "simple_demo_chain_version_data_source");
    env.log(infra::LogLevel::Info, "The execution graph is:");
    env.log(infra::LogLevel::Info, graphOss.str());

    r.finalize();

    ExternalDataSource externalSource;
    ExternalDataSourceBridge bridge(std::get<1>(importerPair));
    externalSource.start(&bridge);

    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });

    return 0;
}
