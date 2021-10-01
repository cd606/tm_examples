#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/zeromq/ZeroMQComponent.hpp>
#include <tm_kit/transport/zeromq/ZeroMQImporterExporter.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQImporterExporter.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>
#include <tm_kit/transport/MultiTransportBroadcastPublisherManagingUtils.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>
#include <tm_kit/transport/TLSConfigurationComponent.hpp>

#include "simple_demo/data_structures/InputDataStructure.hpp"
#include "simple_demo/external_logic/DataSource.hpp"
#include "simple_demo/security_logic/EncAndSignHookFactory.hpp"

#include <iostream>
#include <sstream>

using namespace dev::cd606::tm;
using namespace simple_demo;

using TheEnvironment = infra::Environment<
    infra::CheckTimeComponent<false>,
    infra::TrivialExitControlComponent,
    basic::TrivialBoostLoggingComponent,
    basic::real_time_clock::ClockComponent,
    transport::BoostUUIDComponent,
    transport::zeromq::ZeroMQComponent,
    transport::rabbitmq::RabbitMQComponent,
    transport::web_socket::WebSocketComponent,
    transport::json_rest::JsonRESTComponent,
    transport::TLSServerConfigurationComponent,
    transport::HeartbeatAndAlertComponent,
    EncHookFactoryComponent<InputDataPOCO>
>;
using M = infra::RealTimeApp<TheEnvironment>;

class DataSourceImporter final : public M::AbstractImporter<InputDataPOCO>, public DataSourceListener {
private:
    TheEnvironment *env_;
    DataSource source_;
public:
    DataSourceImporter() : env_(nullptr), source_() {}
    ~DataSourceImporter() {}
    virtual void start(TheEnvironment *env) override final {
        env_ = env;
        source_.start(this);
        env->sendAlert("simple_demo.secure_executables.data_source.info", infra::LogLevel::Info, "Data source started");
    }
    virtual void onData(DataFromSource const &data) override final {
        publish(M::pureInnerData<InputDataPOCO>(env_, InputDataPOCO {data.value}, false));
    }
};

const std::string encKey = "input_data_key";

int main(int argc, char **argv) {
    TheEnvironment env;
    env.transport::json_rest::JsonRESTComponent::setDocRoot(56788, "../simple_demo/secure_executables/datasource_web");
    env.transport::json_rest::JsonRESTComponent::addBasicAuthentication(
        56788, "user1", std::nullopt
    );
    env.transport::json_rest::JsonRESTComponent::addBasicAuthentication(
        56788, "user2", "abcde"
    );
    env.transport::TLSServerConfigurationComponent::setConfigurationItem(
        transport::TLSServerInfoKey {
            56788
        }
        , transport::TLSServerInfo {
            "../grpc_interop_test/DotNetServer/server.crt"
            , "../grpc_interop_test/DotNetServer/server.key"
        }
    );
    env.transport::TLSServerConfigurationComponent::setConfigurationItem(
        transport::TLSServerInfoKey {
            56789
        }
        , transport::TLSServerInfo {
            "../grpc_interop_test/DotNetServer/server.crt"
            , "../grpc_interop_test/DotNetServer/server.key"
        }
    );
    env.EncHookFactoryComponent<InputDataPOCO>::operator=(
        EncHookFactoryComponent<InputDataPOCO> {
            encKey
        }
    );
    
    transport::initializeHeartbeatAndAlertComponent
        (&env, "simple_demo secure DataSource", "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]");
    env.setStatus("program", transport::HeartbeatMessage::Status::Good);

    using R = infra::AppRunner<M>;
    R r(&env);

    auto addTopic = basic::SerializationActions<M>::template addConstTopic<InputDataPOCO>("input.data");

    auto publisherSink = transport::MultiTransportBroadcastPublisherManagingUtils<R>
        ::oneBroadcastPublisherWithProtocol<basic::proto_interop::Proto, InputDataPOCO>(
            r
            , "input data publisher"
            , "zeromq://localhost:12345"
        );
    auto publisherSink2 = transport::MultiTransportBroadcastPublisherManagingUtils<R>
        ::oneBroadcastPublisherWithProtocol<std::void_t, InputDataPOCO>(
            r
            , "input data publisher 2"
            , "websocket://localhost:56789[ignoreTopic=true]"
        );

    auto source = M::importer(new DataSourceImporter());
    auto toPub = r.execute("addTopic", addTopic
            , r.importItem("source", source));

    r.connect(
        toPub.clone()
        , publisherSink
    );
    r.connect(
        toPub.clone()
        , publisherSink2
    );

    transport::attachHeartbeatAndAlertComponent(r, &env, "simple_demo.secure_executables.data_source.heartbeat", std::chrono::seconds(10));

    auto keyQueryFacility = M::liftPureOnOrderFacility<basic::VoidStruct>(
        [](basic::VoidStruct &&) {
            return encKey;
        }
    );
    r.registerOnOrderFacility("keyQuery", keyQueryFacility);
    transport::MultiTransportFacilityWrapper<R>::wrapWithProtocol<basic::nlohmann_json_interop::Json,basic::VoidStruct,std::string>(
        r 
        , keyQueryFacility
        , "json_rest://:56788:::/key_query"
        , "wrapper"
        , std::nullopt
        , false
    );

    r.finalize();

    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });

    return 0;
}