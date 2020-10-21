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
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>
#include <tm_kit/transport/MultiTransportBroadcastPublisherManagingUtils.hpp>

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
    
    transport::initializeHeartbeatAndAlertComponent
        (&env, "simple_demo_chain_version DataSource", "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]");
    env.setStatus("program", transport::HeartbeatMessage::Status::Good);

    using R = infra::AppRunner<M>;
    R r(&env);

    auto addTopic = basic::SerializationActions<M>::template addConstTopic<InputData>("input.data");

    auto publisherSink = transport::MultiTransportBroadcastPublisherManagingUtils<R>
        ::oneBroadcastPublisher<InputData>(
            r
            , "input data publisher"
            , (same_host?"zeromq://ipc::::/tmp/simple_demo_chain_version_input_data":"zeromq://localhost:12345")
        );
    auto importerPair = M::triggerImporter<InputData>();

    r.connect(
        r.execute("addTopic", addTopic
            , r.importItem("source", std::get<0>(importerPair)))
        , publisherSink
    );

    transport::attachHeartbeatAndAlertComponent(r, &env, "simple_demo_chain_version.data_source.heartbeat", std::chrono::seconds(10));

    r.finalize();

    ExternalDataSource externalSource;
    ExternalDataSourceBridge bridge(std::get<1>(importerPair));
    externalSource.start(&bridge);

    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });

    return 0;
}