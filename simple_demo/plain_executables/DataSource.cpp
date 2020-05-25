#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeMonad.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/zeromq/ZeroMQComponent.hpp>
#include <tm_kit/transport/zeromq/ZeroMQImporterExporter.hpp>
#include <tm_kit/transport/HeartbeatComponent.hpp>

#include "defs.pb.h"
#include "simple_demo/external_logic/DataSource.hpp"

#include <iostream>
#include <sstream>

using namespace dev::cd606::tm;
using namespace simple_demo;

using TheEnvironment = infra::Environment<
    infra::CheckTimeComponent<false>,
    basic::TrivialBoostLoggingComponent,
    basic::real_time_clock::ClockComponent,
    transport::BoostUUIDComponent,
    transport::zeromq::ZeroMQComponent,
    transport::HeartbeatComponent
>;
using M = infra::RealTimeMonad<TheEnvironment>;

class DataSourceImporter final : public M::AbstractImporter<InputData>, public DataSourceListener {
private:
    TheEnvironment *env_;
    DataSource source_;
public:
    DataSourceImporter() : env_(nullptr), source_() {}
    ~DataSourceImporter() {}
    virtual void start(TheEnvironment *env) override final {
        env_ = env;
        source_.start(this);
    }
    virtual void onData(DataFromSource const &data) override final {
        InputData dataCopy;
        dataCopy.set_value(data.value);
        publish(M::pureInnerData<InputData>(env_, std::move(dataCopy), false));
    }
};

int main(int argc, char **argv) {
    TheEnvironment env;
    transport::HeartbeatComponentInitializer<TheEnvironment,transport::zeromq::ZeroMQComponent>()
        (&env, "simple_demo DataSource", transport::ConnectionLocator::parse("localhost:23456"));
    env.setStatus("program", transport::HeartbeatMessage::Status::Good);
    infra::MonadRunner<M> r(&env);

    auto addTopic = basic::SerializationActions<M>::template addConstTopic<InputData>("input.data");

    auto publisher = transport::zeromq::ZeroMQImporterExporter<TheEnvironment>
                    ::createTypedExporter<InputData>(
        transport::ConnectionLocator::parse("localhost:12345")
    );

    auto source = M::importer(new DataSourceImporter());

    r.exportItem("publisher", publisher
        , r.execute("addTopic", addTopic
            , r.importItem("source", source)));

    transport::attachHeartbeatComponent(r, &env, "simple_demo.data_source.heartbeat", std::chrono::seconds(10));

    r.finalize();

    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });

    return 0;
}