#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeMonad.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/ByteDataWithTopicRecordFileImporterExporter.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/multicast/MulticastComponent.hpp>
#include <tm_kit/transport/multicast/MulticastImporterExporter.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQImporterExporter.hpp>
#include <tm_kit/transport/zeromq/ZeroMQComponent.hpp>
#include <tm_kit/transport/zeromq/ZeroMQImporterExporter.hpp>
#if ENABLE_REDIS_SUPPORT
#include <tm_kit/transport/redis/RedisComponent.hpp>
#include <tm_kit/transport/redis/RedisImporterExporter.hpp>
#endif

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
#if ENABLE_REDIS_SUPPORT
        ("transport", value<std::string>(), "mcast, zmq, redis or rabbitmq")
        ("topic", value<std::string>(), "the topic to listen for, for rabbitmq, it can use rabbitmq wild card syntax, for mcast and zmq, it can be omitted(all topics), a simple string, or \"r/.../\" containing a regex, for redis, it can be a wildcard")
#else
        ("transport", value<std::string>(), "mcast, zmq or rabbitmq")
        ("topic", value<std::string>(), "the topic to listen for, for rabbitmq, it can use rabbitmq wild card syntax, for mcast and zmq, it can be omitted(all topics), a simple string, or \"r/.../\" containing a regex")
#endif
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
#if ENABLE_REDIS_SUPPORT
    if (transport != "mcast" && transport != "rabbitmq" && transport != "zmq" && transport != "redis") {
        std::cerr << "Transport must be mcast, zmq, redis or rabbitmq!\n";
        return 1;
    }
#else 
    if (transport != "mcast" && transport != "rabbitmq" && transport != "zmq") {
        std::cerr << "Transport must be mcast, zmq or rabbitmq!\n";
        return 1;
    }
#endif
    std::string rabbitMQTopic;
    std::variant<
        transport::multicast::MulticastComponent::NoTopicSelection
        , std::string
        , std::regex
    > multicastTopic;
    std::variant<
        transport::zeromq::ZeroMQComponent::NoTopicSelection
        , std::string
        , std::regex
    > zeroMQTopic;
#if ENABLE_REDIS_SUPPORT
    std::string redisTopic;
#endif
    if (transport == "rabbitmq") {
        if (!vm.count("topic")) {
            rabbitMQTopic = "#";
        } else {
            rabbitMQTopic = vm["topic"].as<std::string>();
        }
    } else if (transport == "mcast") {
        if (!vm.count("topic")) {
            multicastTopic = transport::multicast::MulticastComponent::NoTopicSelection {};
        } else {
            std::string topic = vm["topic"].as<std::string>();
            if (boost::starts_with(topic, "r/") && boost::ends_with(topic, "/") && topic.length() > 3) {
                multicastTopic = std::regex {topic.substr(2, topic.length()-3)};
            } else {
                multicastTopic = topic;
            }
        }
    } else if (transport == "zmq") {
        if (!vm.count("topic")) {
            zeroMQTopic = transport::zeromq::ZeroMQComponent::NoTopicSelection {};
        } else {
            std::string topic = vm["topic"].as<std::string>();
            if (boost::starts_with(topic, "r/") && boost::ends_with(topic, "/") && topic.length() > 3) {
                zeroMQTopic = std::regex {topic.substr(2, topic.length()-3)};
            } else {
                zeroMQTopic = topic;
            }
        }
#if ENABLE_REDIS_SUPPORT
    } else if (transport == "redis") {
        if (!vm.count("topic")) {
            redisTopic = "*";
        } else {
            redisTopic = vm["topic"].as<std::string>();
        }
#endif
    }
    if (!vm.count("address")) {
        std::cerr << "No address given!\n";
        return 1;
    }
    auto address = transport::ConnectionLocator::parse(vm["address"].as<std::string>());
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
        basic::TrivialBoostLoggingComponent,
        basic::real_time_clock::ClockComponent,
        transport::BoostUUIDComponent,
        transport::rabbitmq::RabbitMQComponent,
        transport::multicast::MulticastComponent,
        transport::zeromq::ZeroMQComponent
#if ENABLE_REDIS_SUPPORT
        , transport::redis::RedisComponent
#endif
    >;

    using M = infra::RealTimeMonad<TheEnvironment>;
    using R = infra::MonadRunner<M>;

    TheEnvironment env;
    R r(&env);

    std::ofstream ofs(vm["output"].as<std::string>());

    {
        auto importer =
                (transport == "rabbitmq")
            ?
                transport::rabbitmq::RabbitMQImporterExporter<TheEnvironment>
                ::createImporter(address, rabbitMQTopic)
            :
                (
                    (transport == "mcast")
                ?
                    transport::multicast::MulticastImporterExporter<TheEnvironment>
                    ::createImporter(address, multicastTopic)
                :
#if ENABLE_REDIS_SUPPORT
                    (
                        (transport == "zmq")
                    ?
#endif
                        transport::zeromq::ZeroMQImporterExporter<TheEnvironment>
                        ::createImporter(address, zeroMQTopic)
#if ENABLE_REDIS_SUPPORT
                    :
                        transport::redis::RedisImporterExporter<TheEnvironment>
                        ::createImporter(address, redisTopic)                   
                    )
#endif
                )
            ;

        auto fileWriter =
            basic::ByteDataWithTopicRecordFileImporterExporter<M>
            ::createExporter<std::chrono::microseconds>(
                ofs
                , {(std::byte) 0x01,(std::byte) 0x23,(std::byte) 0x45,(std::byte) 0x67}
                , {(std::byte) 0x76,(std::byte) 0x54,(std::byte) 0x32,(std::byte) 0x10}
                , true //separate thread
            );

        r.exportItem("fileWriter", fileWriter, r.importItem("importer", importer));

        if (summaryPeriod) {
            auto counter = M::liftPure<basic::ByteDataWithTopic>(
                [](basic::ByteDataWithTopic &&data) -> uint64_t {
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
            auto emptyExporter = M::simpleExporter<basic::VoidStruct>(
                [](M::InnerData<basic::VoidStruct> &&) {}
            );
            auto notUsed = r.execute("perClockUpdate", perClockUpdate, r.importItem("clockImporter", clockImporter));
            r.execute(perClockUpdate, 
                r.execute("counter", counter, 
                    r.importItem(importer)));
            r.exportItem("emptyExporter", emptyExporter, std::move(notUsed));
        }
    }

    r.finalize();

    infra::terminationController(infra::TerminateAfterDuration {std::chrono::hours(24)});

    ofs.close();
}