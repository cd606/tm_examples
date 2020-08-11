#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/ByteDataWithTopicRecordFileImporterExporter.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/multicast/MulticastComponent.hpp>
#include <tm_kit/transport/multicast/MulticastImporterExporter.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQImporterExporter.hpp>
#include <tm_kit/transport/zeromq/ZeroMQComponent.hpp>
#include <tm_kit/transport/zeromq/ZeroMQImporterExporter.hpp>
#include <tm_kit/transport/redis/RedisComponent.hpp>
#include <tm_kit/transport/redis/RedisImporterExporter.hpp>
#include <tm_kit/transport/nng/NNGComponent.hpp>
#include <tm_kit/transport/nng/NNGImporterExporter.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListener.hpp>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>
#include <fstream>

using namespace dev::cd606::tm;
using namespace boost::program_options;

int main(int argc, char **argv) {
    options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("transport", value<std::string>(), "mcast, zmq, nng, redis or rabbitmq")
        ("topic", value<std::string>(), "the topic to listen for, for rabbitmq, it can use rabbitmq wild card syntax, for mcast and zmq, it can be omitted(all topics), a simple string, or \"r/.../\" containing a regex, for redis, it can be a wildcard")
        ("address", value<std::string>(), "the address to listen on")
        ("summaryPeriod", value<int>(), "print summary every this number of seconds")
        ("output", value<std::string>(), "output to this file")
    ;
    variables_map vm;
    store(parse_command_line(argc, argv, desc), vm);
    notify(vm);

    if (vm.count("help")) {
        std::cout << desc << '\n';
        return 0;
    }
    if (!vm.count("transport")) {
        std::cerr << "No transport given!\n";
        return 1;
    }
    std::string transport = vm["transport"].as<std::string>();
    if (transport != "mcast" && transport != "rabbitmq" && transport != "zmq" && transport != "redis" && transport != "nng") {
        std::cerr << "Transport must be mcast, zmq, nng,redis or rabbitmq!\n";
        return 1;
    }
    
    if (!vm.count("address")) {
        std::cerr << "No address given!\n";
        return 1;
    }
    auto address = vm["address"].as<std::string>();
    
    std::string topic;
    if (vm.count("topic")) {
        topic = vm["topic"].as<std::string>();
    }

    if (!vm.count("output")) {
        std::cerr << "No output file given!\n";
        return 1;
    }
    std::optional<int> summaryPeriod = std::nullopt;
    if (vm.count("summaryPeriod")) {
        summaryPeriod = vm["summaryPeriod"].as<int>();
    }

    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<true>,
        infra::TrivialExitControlComponent,
        basic::TrivialBoostLoggingComponent,
        basic::real_time_clock::ClockComponent,
        transport::BoostUUIDComponent,
        transport::rabbitmq::RabbitMQComponent,
        transport::multicast::MulticastComponent,
        transport::zeromq::ZeroMQComponent,
        transport::redis::RedisComponent,
        transport::nng::NNGComponent
    >;

    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    R r(&env);

    std::ofstream ofs(vm["output"].as<std::string>());

    {
        auto initialImporter = M::simpleImporter<basic::VoidStruct>(
            [](M::PublisherCall<basic::VoidStruct> &p) {
                p(basic::VoidStruct {});
            }
        );
        auto createKey = M::liftMaybe<basic::VoidStruct>(
            [address,topic,transport](basic::VoidStruct &&) -> std::optional<transport::MultiTransportBroadcastListenerInput> {
                transport::MultiTransportBroadcastListenerConnectionType conn;
                if (transport == "rabbitmq") {
                    conn = transport::MultiTransportBroadcastListenerConnectionType::RabbitMQ;
                } else if (transport == "mcast") {
                    conn = transport::MultiTransportBroadcastListenerConnectionType::Multicast;
                } else if (transport == "zmq") {
                    conn = transport::MultiTransportBroadcastListenerConnectionType::ZeroMQ;
                } else if (transport == "redis") {
                    conn = transport::MultiTransportBroadcastListenerConnectionType::Redis;
                } else if (transport == "nng") {
                    conn = transport::MultiTransportBroadcastListenerConnectionType::NNG;
                } else {
                    return std::nullopt;
                }
                transport::ConnectionLocator locator;
                try {
                    locator = transport::ConnectionLocator::parse(address);
                } catch (transport::ConnectionLocatorParseError const &) {
                    return std::nullopt;
                }
                return transport::MultiTransportBroadcastListenerInput { {
                    transport::MultiTransportBroadcastListenerAddSubscription {
                        conn
                        , locator
                        , topic
                    }
                } };
            }
        );
        auto keyify = M::template kleisli<transport::MultiTransportBroadcastListenerInput>(
            basic::CommonFlowUtilComponents<M>::template keyify<transport::MultiTransportBroadcastListenerInput>()
        );
        auto multiSub = M::onOrderFacilityWithExternalEffects(
            new transport::MultiTransportBroadcastListener<TheEnvironment, basic::ByteData>()
        );

        auto subKey = r.execute(
            "keyify", keyify
            , r.execute(
                "createKey", createKey
                , r.importItem("initialImporter", initialImporter)
            )
        );

        r.placeOrderWithFacilityWithExternalEffectsAndForget(
            std::move(subKey), "multiSub", multiSub
        );

        auto removeOneLevel =
            M::liftPure<basic::TypedDataWithTopic<basic::ByteData>>(
                [](basic::TypedDataWithTopic<basic::ByteData> &&x) -> basic::ByteDataWithTopic {
                    return {std::move(x.topic), std::move(x.content.content)};
                }
            );

        auto fileWriter =
            basic::ByteDataWithTopicRecordFileImporterExporter<M>
            ::createExporter<basic::ByteDataWithTopicRecordFileFormat<std::chrono::microseconds>>(
                ofs
                , {(std::byte) 0x01,(std::byte) 0x23,(std::byte) 0x45,(std::byte) 0x67}
                , {(std::byte) 0x76,(std::byte) 0x54,(std::byte) 0x32,(std::byte) 0x10}
                , true //separate thread
            );

        r.exportItem("fileWriter", fileWriter, 
            r.execute("removeOneLevel", removeOneLevel, r.facilityWithExternalEffectsAsSource(multiSub)));

        if (summaryPeriod) {
            auto counter = M::liftPure<basic::TypedDataWithTopic<basic::ByteData>>(
                [](basic::TypedDataWithTopic<basic::ByteData> &&data) -> uint64_t {
                    static uint64_t counter = 0;
                    return (++counter);
                }
            );
            auto clockImporter = 
                basic::real_time_clock::ClockImporter<TheEnvironment>
                ::createRecurringClockConstImporter<basic::VoidStruct>(
                    env.now()
                    , env.now()+std::chrono::hours(24)
                    , std::chrono::seconds(*summaryPeriod)
                    , basic::VoidStruct {}
                );
            auto perClockUpdate = M::kleisli2<basic::VoidStruct,uint64_t>(
                [](int which, M::InnerData<basic::VoidStruct> &&clockData, M::InnerData<uint64_t> &&counter) -> M::Data<basic::VoidStruct> {
                    if (which == 0) {
                        std::ostringstream oss;
                        oss << "Got " << counter.timedData.value << " messages";
                        clockData.environment->log(infra::LogLevel::Info, oss.str());
                    }
                    return std::nullopt;
                }
            );
            auto emptyExporter = M::trivialExporter<basic::VoidStruct>();

            auto notUsed = r.execute("perClockUpdate", perClockUpdate, r.importItem("clockImporter", clockImporter));
            r.execute(perClockUpdate, 
                r.execute("counter", counter, 
                    r.facilityWithExternalEffectsAsSource(multiSub)));
            r.exportItem("emptyExporter", emptyExporter, std::move(notUsed));
        }
    }

    r.finalize();

    infra::terminationController(infra::TerminateAfterDuration {std::chrono::hours(24)});

    ofs.close();
}