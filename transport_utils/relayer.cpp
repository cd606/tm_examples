#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

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
#include <tm_kit/transport/redis/RedisComponent.hpp>
#include <tm_kit/transport/redis/RedisImporterExporter.hpp>

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
        ("incomingTransport", value<std::string>(), "mcast, zmq, redis or rabbitmq")
        ("incomingAddress", value<std::string>(), "the address to listen on")
        ("outgoingTransport", value<std::string>(), "mcast, zmq, redis or rabbitmq")
        ("outgoingAddress", value<std::string>(), "the address to publish on")
        ("summaryPeriod", value<int>(), "print summary every this number of seconds")
    ;
    variables_map vm;
    store(parse_command_line(argc, argv, desc), vm);
    notify(vm);

    if (vm.count("help")) {
        std::cout << desc << '\n';
        return 0;
    }
    if (!vm.count("incomingTransport")) {
        std::cerr << "No incoming transport given!\n";
        return 1;
    }
    std::string incomingTransport = vm["incomingTransport"].as<std::string>();
    if (incomingTransport != "mcast" && incomingTransport != "rabbitmq" && incomingTransport != "zmq" && incomingTransport != "redis") {
        std::cerr << "Incoming transport must be mcast, zmq, redis or rabbitmq!\n";
        return 1;
    }
    if (!vm.count("incomingAddress")) {
        std::cerr << "No incoming address given!\n";
        return 1;
    }
    auto incomingAddress = transport::ConnectionLocator::parse(vm["incomingAddress"].as<std::string>());
    if (!vm.count("outgoingTransport")) {
        std::cerr << "No outgoing transport given!\n";
        return 1;
    }
    std::string outgoingTransport = vm["outgoingTransport"].as<std::string>();
    if (outgoingTransport != "mcast" && outgoingTransport != "rabbitmq" && outgoingTransport != "zmq" && outgoingTransport != "redis") {
        std::cerr << "Outgoing transport must be mcast, zmq, redis or rabbitmq!\n";
        return 1;
    }
    if (!vm.count("outgoingAddress")) {
        std::cerr << "No outgoing address given!\n";
        return 1;
    }
    auto outgoingAddress = transport::ConnectionLocator::parse(vm["outgoingAddress"].as<std::string>());

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
        transport::redis::RedisComponent
    >;

    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    R r(&env);

    {
        auto importer =
                (incomingTransport == "rabbitmq")
            ?
                transport::rabbitmq::RabbitMQImporterExporter<TheEnvironment>
                ::createImporter(incomingAddress, "#")
            :
                (
                    (incomingTransport == "mcast")
                ?
                    transport::multicast::MulticastImporterExporter<TheEnvironment>
                    ::createImporter(incomingAddress, transport::multicast::MulticastComponent::NoTopicSelection {})
                :
                    (
                        (incomingTransport == "zmq")
                    ?
                        transport::zeromq::ZeroMQImporterExporter<TheEnvironment>
                        ::createImporter(incomingAddress, transport::zeromq::ZeroMQComponent::NoTopicSelection {})               
                    :
                        transport::redis::RedisImporterExporter<TheEnvironment>
                        ::createImporter(incomingAddress, "*")
                    )
                )
            ;

        auto exporter =
                (outgoingTransport == "rabbitmq")
            ?
                transport::rabbitmq::RabbitMQImporterExporter<TheEnvironment>
                ::createExporter(outgoingAddress)
            :
                (
                    (outgoingTransport == "mcast")
                ?
                    transport::multicast::MulticastImporterExporter<TheEnvironment>
                    ::createExporter(outgoingAddress)
                :
                    (
                        outgoingTransport == "zmq"
                    ?
                        transport::zeromq::ZeroMQImporterExporter<TheEnvironment>
                        ::createExporter(outgoingAddress)
                    :
                        transport::redis::RedisImporterExporter<TheEnvironment>
                        ::createExporter(outgoingAddress)
                    )
                )
            ;

        r.exportItem("exporter", exporter, r.importItem("importer", importer));

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
}