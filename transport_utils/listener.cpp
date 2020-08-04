#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
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
#include <tm_kit/transport/MultiTransportBroadcastListener.hpp>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

using namespace dev::cd606::tm;
using namespace boost::program_options;

int main(int argc, char **argv) {
    options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("transport", value<std::string>(), "mcast, zmq, redis or rabbitmq")
        ("topic", value<std::string>(), "the topic to listen for, for rabbitmq, it can use rabbitmq wild card syntax, for mcast and zmq, it can be omitted(all topics), a simple string, or \"r/.../\" containing a regex, for redis, it can be a wildcard")
        ("address", value<std::string>(), "the address to listen on")
        ("summaryPeriod", value<int>(), "print summary every this number of seconds")
        ("printPerMessage", "whether to print per message")
        ("messageIsText", "when printing message, whether we are sure that the message is text")
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
    if (transport != "mcast" && transport != "rabbitmq" && transport != "zmq" && transport != "redis") {
        std::cerr << "Transport must be mcast, zmq, redis or rabbitmq!\n";
        return 1;
    }

    if (!vm.count("address")) {
        std::cerr << "No address given!\n";
        return 1;
    }
    auto address =vm["address"].as<std::string>();

    std::string topic;
    if (vm.count("topic")) {
        topic = vm["topic"].as<std::string>();
    }

    std::optional<int> summaryPeriod = std::nullopt;
    if (vm.count("summaryPeriod")) {
        summaryPeriod = vm["summaryPeriod"].as<int>();
    }
    bool printPerMessage = vm.count("printPerMessage");

    if (!summaryPeriod && !printPerMessage) {
        std::cerr << "This program must do something, either print summary message per period or print info per message\n";
        return 1;
    }
    bool messageIsText = vm.count("messageIsText");

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
        /*auto printSubRes = M::pureExporter<M::KeyedData<transport::MultiTransportBroadcastListenerInput, transport::MultiTransportBroadcastListenerOutput>>(
            [&env](M::KeyedData<transport::MultiTransportBroadcastListenerInput, transport::MultiTransportBroadcastListenerOutput> &&subRes) {
                std::visit([&env](auto &&x) {
                    using T = std::decay_t<decltype(x)>;
                    if constexpr (std::is_same_v<T, transport::MultiTransportBroadcastListenerAddSubscriptionResponse>) {
                        std::ostringstream oss;
                        oss << "Got subscription with id " << x.subscriptionID;
                        env.log(infra::LogLevel::Info, oss.str());
                    }
                }, std::move(subRes.data.value));
            }
        );*/

        auto subKey = r.execute(
            "keyify", keyify
            , r.execute(
                "createKey", createKey
                , r.importItem("initialImporter", initialImporter)
            )
        );
        /*
        r.placeOrderWithFacilityWithExternalEffects(
            std::move(subKey), "multiSub", multiSub
            , r.exporterAsSink("printSubRes", printSubRes)
        );
        */
        r.placeOrderWithFacilityWithExternalEffectsAndForget(
            std::move(subKey), "multiSub", multiSub
        );
        struct SharedState {
            std::mutex mutex;
            uint64_t messageCount;
            SharedState() : mutex(), messageCount(0) {}
        };
        std::shared_ptr<SharedState> sharedState = std::make_shared<SharedState>();
        
        r.preservePointer(sharedState);

        auto perMessage = M::simpleExporter<basic::TypedDataWithTopic<basic::ByteData>>([sharedState,printPerMessage,messageIsText](M::InnerData<basic::TypedDataWithTopic<basic::ByteData>> &&data) {
            if (printPerMessage) {
                std::ostringstream oss;
                if (messageIsText) {
                    oss << "topic='" << data.timedData.value.topic
                        << "',content='" << data.timedData.value.content.content
                        << "'";
                } else {
                    oss << data.timedData.value;
                }
                data.environment->log(infra::LogLevel::Info, oss.str());
            }
            {
                std::lock_guard<std::mutex> _(sharedState->mutex);
                ++(sharedState->messageCount);
            }
        });

        r.exportItem("perMessage", perMessage, r.facilityWithExternalEffectsAsSource(multiSub));

        if (summaryPeriod) {
            auto clockImporter = 
                basic::real_time_clock::ClockImporter<TheEnvironment>
                ::createRecurringClockConstImporter<basic::VoidStruct>(
                    env.now()
                    , env.now()+std::chrono::hours(24)
                    , std::chrono::seconds(*summaryPeriod)
                    , basic::VoidStruct {}
                );
            auto perClockUpdate = M::simpleExporter<basic::VoidStruct>([sharedState](M::InnerData<basic::VoidStruct> &&data) {
                std::ostringstream oss;
                {
                    std::lock_guard<std::mutex> _(sharedState->mutex);
                    oss << "Got " << sharedState->messageCount << " messages";
                }
                data.environment->log(infra::LogLevel::Info, oss.str());
            });
            r.exportItem("perClockUpdate", perClockUpdate, r.importItem("clockImporter", clockImporter));
        }
    }

    r.finalize();

    infra::terminationController(infra::TerminateAfterDuration {std::chrono::hours(24)});
}