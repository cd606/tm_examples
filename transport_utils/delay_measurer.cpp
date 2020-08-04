#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/ByteDataWithTopicRecordFileImporterExporter.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListener.hpp>
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

using namespace dev::cd606::tm;
using namespace boost::program_options;

enum class Mode {
    Sender,
    Receiver
};

using TheEnvironment = infra::Environment<
    infra::CheckTimeComponent<true>,
    infra::TrivialExitControlComponent,
    basic::SpdLoggingComponent,
    basic::real_time_clock::ClockComponent,
    transport::CrossGuidComponent,
    transport::AllNetworkTransportComponents
>;

using M = infra::RealTimeApp<TheEnvironment>;
using R = infra::AppRunner<M>;
using SendDataType = basic::CBOR<std::tuple<uint64_t, int64_t, basic::ByteData>>;

void runSender(unsigned interval, unsigned bytes, std::string const &transport, transport::ConnectionLocator const &address, std::optional<unsigned> summaryPeriod) {
    TheEnvironment env;
    R r (&env);

    const basic::ByteData testData { std::string(bytes, ' ') };
    auto mainClockImporter = 
        basic::real_time_clock::ClockImporter<TheEnvironment>
            ::createRecurringClockConstImporter<basic::VoidStruct>(
                env.now()
                , env.now()+std::chrono::hours(24)
                , std::chrono::milliseconds(interval)
                , basic::VoidStruct {}
            );

    std::atomic<uint64_t> counter = 0;
    
    auto createData =
        M::liftPure<basic::VoidStruct>(
            [&env,&testData,&counter](basic::VoidStruct &&) -> basic::TypedDataWithTopic<SendDataType> {
                return {
                    "test.data"
                    , { {++counter, infra::withtime_utils::sinceEpoch<std::chrono::milliseconds>(env.now()), testData} }
                };
            }
        );
    auto exporter =
            (transport == "rabbitmq")
        ?
            transport::rabbitmq::RabbitMQImporterExporter<TheEnvironment>
            ::createTypedExporter<SendDataType>(address)
        :
            (
                (transport == "mcast")
            ?
                transport::multicast::MulticastImporterExporter<TheEnvironment>
                ::createTypedExporter<SendDataType>(address)
            :
                (
                    transport == "zmq"
                ?
                    transport::zeromq::ZeroMQImporterExporter<TheEnvironment>
                    ::createTypedExporter<SendDataType>(address)
                :
                    transport::redis::RedisImporterExporter<TheEnvironment>
                    ::createTypedExporter<SendDataType>(address)
                )
            )
        ;

    r.exportItem("exporter", exporter, 
        r.execute("createData", createData,
            r.importItem("mainClockImporter", mainClockImporter)));

    if (summaryPeriod) {
        auto summaryClockImporter = 
            basic::real_time_clock::ClockImporter<TheEnvironment>
            ::createRecurringClockConstImporter<basic::VoidStruct>(
                env.now()
                , env.now()+std::chrono::hours(24)
                , std::chrono::seconds(*summaryPeriod)
                , basic::VoidStruct {}
            );
        auto perSummaryClockUpdate = M::pureExporter<basic::VoidStruct>(
            [&env,&counter](basic::VoidStruct &&) {
                std::ostringstream oss;
                oss << "Sent " << counter << " messages";
                env.log(infra::LogLevel::Info, oss.str());
            }
        );
        r.exportItem("perSummaryClockUpdate", perSummaryClockUpdate,
            r.importItem("summaryClockImporter", summaryClockImporter));
    }
    r.finalize();

    infra::terminationController(infra::RunForever {&env});
}

void runReceiver(std::string const &transport, transport::ConnectionLocator const &address, std::optional<unsigned> summaryPeriod) {
    TheEnvironment env;
    R r (&env);

    auto importer =
            (transport == "rabbitmq")
        ?
            transport::rabbitmq::RabbitMQImporterExporter<TheEnvironment>
            ::createTypedImporter<SendDataType>(address, "test.data")
        :
            (
                (transport == "mcast")
            ?
                transport::multicast::MulticastImporterExporter<TheEnvironment>
                ::createTypedImporter<SendDataType>(address, "test.data")
            :
                (
                    (transport == "zmq")
                ?
                    transport::zeromq::ZeroMQImporterExporter<TheEnvironment>
                    ::createTypedImporter<SendDataType>(address, "test.data")               
                :
                    transport::redis::RedisImporterExporter<TheEnvironment>
                    ::createTypedImporter<SendDataType>(address, "test.data")
                )
            )
        ;

    struct Stats {
        uint64_t count = 0;
        double totalDelay = 0.0;
        double totalDelaySq = 0.0;
        uint64_t minID = 0;
        uint64_t maxID = 0;
        int64_t minDelay = std::numeric_limits<int64_t>::min();
        int64_t maxDelay = 0;
    };
    Stats stats;
    std::mutex statsMutex;
   
    auto calcStats =
        M::pureExporter<basic::TypedDataWithTopic<SendDataType>>(
            [&env,&stats,&statsMutex](basic::TypedDataWithTopic<SendDataType> &&data) {
                auto now = infra::withtime_utils::sinceEpoch<std::chrono::milliseconds>(env.now());
                auto delay = now-std::get<1>(data.content.value);
                auto id = std::get<0>(data.content.value);
                {
                    std::lock_guard<std::mutex> _(statsMutex);
                    ++stats.count;
                    stats.totalDelay += 1.0*delay;
                    stats.totalDelaySq += 1.0*delay*delay;
                    if (stats.minID == 0 || id < stats.minID) {
                        stats.minID = id;
                    }
                    if (id > stats.maxID) {
                        stats.maxID = id;
                    }
                    if (stats.minDelay > delay || stats.minDelay == std::numeric_limits<int64_t>::min()) {
                        stats.minDelay = delay;
                    }
                    if (stats.maxDelay < delay) {
                        stats.maxDelay = delay;
                    }
                }
            }
        );

    r.exportItem("calcStats", calcStats, r.importItem("importer", importer));

    if (summaryPeriod) {
        auto summaryClockImporter = 
            basic::real_time_clock::ClockImporter<TheEnvironment>
            ::createRecurringClockConstImporter<basic::VoidStruct>(
                env.now()
                , env.now()+std::chrono::hours(24)
                , std::chrono::seconds(*summaryPeriod)
                , basic::VoidStruct {}
            );
        auto perSummaryClockUpdate = M::pureExporter<basic::VoidStruct>(
            [&env,&stats,&statsMutex](basic::VoidStruct &&clockData) {
                double mean, sd;
                uint64_t missed;
                {
                    std::lock_guard<std::mutex> _(statsMutex);
                    mean = (stats.count > 0) ? (stats.totalDelay/stats.count) : 0.0;
                    sd = (stats.count > 1) ? (stats.totalDelaySq-mean*mean*stats.count)/(stats.count-1) : 0.0;
                    missed = (stats.count > 0) ? (stats.maxID-stats.minID+1-stats.count) : 0;
                }
                std::ostringstream oss;
                oss << "Got " << stats.count << " messages, mean delay " << mean << " ms, std delay " << sd << " ms, missed " << missed << " messages, min delay " << stats.minDelay << " ms, max delay " << stats.maxDelay << " ms";
                env.log(infra::LogLevel::Info, oss.str());
            }
        );
        r.exportItem("perSummaryClockUpdate", perSummaryClockUpdate, r.importItem("summaryClockImporter", summaryClockImporter));
    }
    r.finalize();

    infra::terminationController(infra::RunForever {&env});
}

int main(int argc, char **argv) {
    options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("mode", value<std::string>(), "sender or receiver")
        ("interval", value<unsigned>(), "interval time in milliseconds")
        ("bytes", value<unsigned>(), "bytes per message sent")
        ("transport", value<std::string>(), "mcast, zmq, redis or rabbitmq")
        ("address", value<std::string>(), "the address for measuring data")
        ("summaryPeriod", value<unsigned>(), "print summary every this number of seconds")
    ;
    variables_map vm;
    store(parse_command_line(argc, argv, desc), vm);
    notify(vm);

    if (vm.count("help")) {
        std::cout << desc << '\n';
        return 0;
    }
    if (!vm.count("mode")) {
        std::cerr << "No mode given!\n";
        return 1;
    }
    std::string modeStr = vm["mode"].as<std::string>();
    Mode mode;
    if (modeStr == "sender") {
        mode = Mode::Sender;
    } else if (modeStr == "receiver") {
        mode = Mode::Receiver;
    } else {
        std::cerr << "Wrong mode '" << modeStr << "', must be sender or receiver!\n";
        return 1;
    }
    unsigned interval = 0, bytes = 0;
    if (mode == Mode::Sender) {
        if (!vm.count("interval")) {
            std::cerr << "No interval given for sender!\n";
            return 1;
        }
        interval = vm["interval"].as<unsigned>();
        if (interval == 0) {
            std::cerr << "Interval cannot be zero for sender!\n";
            return 1;
        }
        if (!vm.count("bytes")) {
            std::cerr << "No bytes given for sender!\n";
        }
        bytes = vm["bytes"].as<unsigned>();
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
    auto address = transport::ConnectionLocator::parse(vm["address"].as<std::string>());

    std::optional<unsigned> summaryPeriod = std::nullopt;
    if (vm.count("summaryPeriod")) {
        summaryPeriod = vm["summaryPeriod"].as<unsigned>();
    }
    if (mode == Mode::Receiver && !summaryPeriod) {
        std::cerr << "No summary period given for receiver!\n";
        return 1;
    }

    if (mode == Mode::Sender) {
        runSender(interval, bytes, transport, address, summaryPeriod);
    } else {
        runReceiver(transport, address, summaryPeriod);
    }
}