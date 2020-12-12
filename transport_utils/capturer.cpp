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
#include <tm_kit/transport/MultiTransportBroadcastListenerManagingUtils.hpp>

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
        ("topic", value<std::string>(), "the topic to listen for, for rabbitmq, it can use rabbitmq wild card syntax, for mcast and zmq, it can be omitted(all topics), a simple string, or \"r/.../\" containing a regex, for redis, it can be a wildcard")
        ("address", value<std::string>(), "the address to listen on, with protocol info")
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
    
    if (!vm.count("address")) {
        std::cerr << "No address given!\n";
        return 1;
    }
    auto address = vm["address"].as<std::string>();
    
    std::optional<std::string> topic = std::nullopt;
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
        transport::AllNetworkTransportComponents
    >;

    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    R r(&env);

    std::ofstream ofs(vm["output"].as<std::string>(), std::ios::binary);

    {
        auto dataSource = transport::MultiTransportBroadcastListenerManagingUtils<R>
            ::oneByteDataBroadcastListener(
                r 
                , "data source"
                , address 
                , topic
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
           dataSource.clone());

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
                [](M::InnerData<std::variant<basic::VoidStruct,uint64_t>> &&data) -> M::Data<basic::VoidStruct> {
                    static uint64_t counter = 0;
                    if (data.timedData.value.index() == 0) {
                        std::ostringstream oss;
                        oss << "Got " << counter << " messages";
                        data.environment->log(infra::LogLevel::Info, oss.str());
                    } else {
                        counter = std::get<1>(data.timedData.value);
                    }
                    return std::nullopt;
                }
            );
            auto emptyExporter = M::trivialExporter<basic::VoidStruct>();

            auto notUsed = r.execute("perClockUpdate", perClockUpdate, r.importItem("clockImporter", clockImporter));
            r.execute(perClockUpdate, 
                r.execute("counter", counter, 
                    dataSource.clone()));
            r.exportItem("emptyExporter", emptyExporter, std::move(notUsed));
        }
    }

    r.finalize();

    infra::terminationController(infra::TerminateAfterDuration {std::chrono::hours(24)});

    ofs.close();
}