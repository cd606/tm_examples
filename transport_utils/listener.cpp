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
#include <tm_kit/transport/MultiTransportBroadcastListenerManagingUtils.hpp>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

using namespace dev::cd606::tm;
using namespace boost::program_options;

int main(int argc, char **argv) {
    options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("topic", value<std::string>(), "the topic to listen for, for rabbitmq, it can use rabbitmq wild card syntax, for mcast and zmq, it can be omitted(all topics), a simple string, or \"r/.../\" containing a regex, for redis, it can be a wildcard")
        ("address", value<std::string>(), "the address to listen on, with protocol info")
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

    if (!vm.count("address")) {
        std::cerr << "No address given!\n";
        return 1;
    }
    auto address =vm["address"].as<std::string>();

    std::optional<std::string> topic = std::nullopt;
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
        transport::AllNetworkTransportComponents
    >;

    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    R r(&env);

    {
        auto dataSource = transport::MultiTransportBroadcastListenerManagingUtils<R>
            ::oneByteDataBroadcastListener(
                r 
                , "data source"
                , address 
                , topic
            );
        struct SharedState {
            std::mutex mutex;
            uint64_t messageCount;
            SharedState() : mutex(), messageCount(0) {}
        };
        std::shared_ptr<SharedState> sharedState = std::make_shared<SharedState>();
        
        r.preservePointer(sharedState);

        auto perMessage = M::simpleExporter<basic::ByteDataWithTopic>([sharedState,printPerMessage,messageIsText](M::InnerData<basic::ByteDataWithTopic> &&data) {
            if (printPerMessage) {
                std::ostringstream oss;
                if (messageIsText) {
                    oss << "topic='" << data.timedData.value.topic
                        << "',content='" << data.timedData.value.content
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

        r.exportItem("perMessage", perMessage, std::move(dataSource));

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