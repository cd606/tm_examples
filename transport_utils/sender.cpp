#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/GenericLift.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/MultiTransportBroadcastPublisherManagingUtils.hpp>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

using namespace dev::cd606::tm;
using namespace boost::program_options;

int main(int argc, char **argv) {
    options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("topic", value<std::string>(), "the topic to publish on (default: \"default\")")
        ("address", value<std::string>(), "the address to publish on, with protocol info")
        ("summaryPeriod", value<int>(), "print summary every this number of seconds")
        ("frequencyMs", value<int>(), "send frequency in milliseconds (default: 1000)")
        ("firstValue", value<int>(), "first value to send (default: 1)")
        ("step", value<int>(), "value step (default: 1)")
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

    std::string topic = "default";
    if (vm.count("topic")) {
        topic = vm["topic"].as<std::string>();
    }

    std::optional<int> summaryPeriod = std::nullopt;
    if (vm.count("summaryPeriod")) {
        summaryPeriod = vm["summaryPeriod"].as<int>();
    }

    int frequencyMs = 1000;
    if (vm.count("frequencyMs")) {
        frequencyMs = vm["frequencyMs"].as<int>();
    }
    int firstValue = 1;
    if (vm.count("firstValue")) {
        firstValue = vm["firstValue"].as<int>();
    }
    int step = 1;
    if (vm.count("step")) {
        step = vm["step"].as<int>();
    }

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
        auto valueSource = basic::real_time_clock::ClockImporter<TheEnvironment>
            ::createRecurringClockImporter<int>(
                env.now()
                , env.now()+std::chrono::hours(24)
                , std::chrono::milliseconds(frequencyMs)
                , [firstValue,step](std::chrono::system_clock::time_point const &) {
                    static int v = firstValue-step;
                    v += step;
                    return v;
                }
            );
        auto encode = infra::GenericLift<M>::lift([topic](int x) {
            return basic::ByteDataWithTopic {
                .topic = topic
                , .content = std::to_string(x)
            };
        });
        auto publisher = transport::MultiTransportBroadcastPublisherManagingUtils<R>
            ::oneByteDataBroadcastPublisher
            (
                r 
                , "pub"
                , address
            );
        auto source = r.execute("encode", encode, r.importItem("valueSource", valueSource));
        r.connect(source.clone(), publisher);

        struct SharedState {
            std::mutex mutex;
            uint64_t messageCount;
            SharedState() : mutex(), messageCount(0) {}
        };
        std::shared_ptr<SharedState> sharedState = std::make_shared<SharedState>();
        
        r.preservePointer(sharedState);

        auto perMessage = infra::GenericLift<M>::lift([sharedState,&env](basic::ByteDataWithTopic &&data) {
            //env.log(infra::LogLevel::Info, "Sending message "+boost::lexical_cast<std::string>(data));
            {
                std::lock_guard<std::mutex> _(sharedState->mutex);
                ++(sharedState->messageCount);
            }
        });

        r.exportItem("perMessage", perMessage, source.clone());

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
                    oss << "Sent " << sharedState->messageCount << " messages";
                }
                data.environment->log(infra::LogLevel::Info, oss.str());
            });
            r.exportItem("perClockUpdate", perClockUpdate, r.importItem("clockImporter", clockImporter));
        }
    }

    r.finalize();

    infra::terminationController(infra::TerminateAfterDuration {std::chrono::hours(24)});
}