#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeMonad.hpp>
#include <tm_kit/infra/SinglePassIterationMonad.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/ByteDataWithTopicRecordFileImporterExporter.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockImporter.hpp>

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
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

using namespace dev::cd606::tm;
using namespace boost::program_options;

int main(int argc, char **argv) {
    options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("transport", value<std::string>(), "mcast, zmq, redis or rabbitmq")
        ("address", value<std::string>(), "the address to republish on")
        ("input", value<std::string>(), "input from this file")
        ("speed", value<double>(), "republish speed factor, default 1.0")
        ("calibratePointHistorical", value<std::string>(), "calibrate time point (for historical data), format is HH:MM[:SS]")
        ("calibratePointActual", value<std::string>(), "calibrate time point (for actual clock), format is HH:MM (no second!), or +N (where N is count of minutes)")
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
    auto address = transport::ConnectionLocator::parse(vm["address"].as<std::string>());
    if (!vm.count("input")) {
        std::cerr << "No output file given!\n";
        return 1;
    }
    auto input = vm["input"].as<std::string>();
    double speed = 1.0;
    if (vm.count("speed")) {
        speed = vm["speed"].as<double>();
    }
    if (!vm.count("calibratePointHistorical")) {
        std::cerr << "No historical calibrate point given!\n";
        return 1;
    }
    auto calibratePointHistorical = boost::trim_copy(vm["calibratePointHistorical"].as<std::string>());
    if ((calibratePointHistorical.length() != 5 && calibratePointHistorical.length() != 8) || calibratePointHistorical[2] != ':') {
        std::cerr << "Historical calibrate point must be in HH:MM[:SS] format!\n";
        return 1;
    }
    int hour_hist, min_hist, sec_hist;
    try {
        hour_hist = boost::lexical_cast<int>(calibratePointHistorical.substr(0,2));
        min_hist = boost::lexical_cast<int>(calibratePointHistorical.substr(3,2));
        if (calibratePointHistorical.length() == 8) {
            sec_hist = boost::lexical_cast<int>(calibratePointHistorical.substr(6,2));
        } else {
            sec_hist = 0;
        }
    } catch (boost::bad_lexical_cast const &) {
        std::cerr << "Historical calibrate point must be in HH:MM[:SS] format!\n";
        return 1;
    }
    if (hour_hist < 0 || hour_hist >= 24 || min_hist < 0 || min_hist >= 60 || sec_hist < 0 || sec_hist >= 60) {
        std::cerr << "Historical calibrate point must be in HH:MM[:SS] format!\n";
        return 1;
    }
    if (!vm.count("calibratePointActual")) {
        std::cerr << "No Actual calibrate point given!\n";
        return 1;
    }
    auto calibratePointActual = boost::trim_copy(vm["calibratePointActual"].as<std::string>());
    std::optional<int> calibrateActualMinutes = std::nullopt;
    int hour_act=-1, min_act=-1;
    if (calibratePointActual.length() > 1 && calibratePointActual[0] == '+') {
        try {
            calibrateActualMinutes = boost::lexical_cast<int>(calibratePointActual.substr(1));
        } catch (boost::bad_lexical_cast const &) {
            calibrateActualMinutes = std::nullopt;
        }
    }
    if (!calibrateActualMinutes) {
        if (calibratePointActual.length() != 5 || calibratePointActual[2] != ':') {
            std::cerr << "Actual calibrate point must be in HH:MM or +N format!\n";
            return 1;
        }      
        try {
            hour_act = boost::lexical_cast<int>(calibratePointActual.substr(0,2));
            min_act = boost::lexical_cast<int>(calibratePointActual.substr(3,2));
        } catch (boost::bad_lexical_cast const &) {
            std::cerr << "Actual calibrate point must be in HH:MM or +N format!\n";
            return 1;
        }
        if (hour_act < 0 || hour_act >= 24 || min_act < 0 || min_act >= 60) {
            std::cerr << "Actual calibrate point must be in HH:MM or +N format!\n";
            return 1;
        }
    }

    std::chrono::system_clock::time_point firstTimePoint;

    {
        //initialization, try to find the date
        std::ifstream ifs(input);
        using TheEnvironment = infra::Environment<
            infra::CheckTimeComponent<true>,
            basic::TrivialBoostLoggingComponent,
            basic::single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>,
            transport::BoostUUIDComponent
        >;
        using Monad = infra::SinglePassIterationMonad<TheEnvironment>;
        using FileComponent = basic::ByteDataWithTopicRecordFileImporterExporter<Monad>;

        TheEnvironment env;
        infra::MonadRunner<Monad> r(&env);
        
        auto importer = FileComponent::createImporter<basic::ByteDataWithTopicRecordFileFormat<std::chrono::microseconds>>(
            ifs, 
            {(std::byte) 0x01,(std::byte) 0x23,(std::byte) 0x45,(std::byte) 0x67},
            {(std::byte) 0x76,(std::byte) 0x54,(std::byte) 0x32,(std::byte) 0x10}
        );
        auto finalizer = basic::CommonFlowUtilComponents<Monad>::simpleFinalizer<basic::ByteDataWithTopic>();
        auto getTime = Monad::simpleExporter<basic::ByteDataWithTopic>([&firstTimePoint](Monad::InnerData<basic::ByteDataWithTopic> &&d) {
            firstTimePoint = d.timedData.timePoint;
        });

        r.exportItem("getTime", getTime
            , r.execute("finalizer", finalizer
                , r.importItem("importer", importer)));

        r.finalize();

        infra::terminationController(infra::ImmediatelyTerminate {});

        ifs.close();
    }
    {
        //Now we do the full replay
        std::ifstream ifs(input);
        
        using TheEnvironment = infra::Environment<
            infra::CheckTimeComponent<true>,
            basic::TrivialBoostLoggingComponent,
            basic::real_time_clock::ClockComponent,
            transport::BoostUUIDComponent,
            transport::rabbitmq::RabbitMQComponent,
            transport::multicast::MulticastComponent,
            transport::zeromq::ZeroMQComponent,
            transport::redis::RedisComponent
        >;
        using Monad = infra::RealTimeMonad<TheEnvironment>;
        using FileComponent = basic::ByteDataWithTopicRecordFileImporterExporter<Monad>;

        TheEnvironment env;
        auto dateStr = infra::withtime_utils::localTimeString(firstTimePoint).substr(0, 10);
        std::ostringstream oss;
        oss << dateStr << 'T' 
            << std::setw(2) << std::setfill('0') << hour_hist
            << ':'
            << std::setw(2) << std::setfill('0') << min_hist
            << ':'
            << std::setw(2) << std::setfill('0') << sec_hist
            << ".000";
        TheEnvironment::ClockSettings settings;
        if (calibrateActualMinutes) {
            settings = TheEnvironment::clockSettingsWithStartPointCorrespondingToNextAlignment(
                *calibrateActualMinutes
                , oss.str()
                , speed
            );
        } else {
            settings = TheEnvironment::clockSettingsWithStartPoint(
                hour_act*100+min_act
                , oss.str()
                , speed
            );
        }
        env.basic::real_time_clock::ClockComponent::operator=(
            basic::real_time_clock::ClockComponent(settings)
        );
        infra::MonadRunner<Monad> r(&env);
        
        auto importer = FileComponent::createImporter<basic::ByteDataWithTopicRecordFileFormat<std::chrono::microseconds>,true>(
            ifs, 
            {(std::byte) 0x01,(std::byte) 0x23,(std::byte) 0x45,(std::byte) 0x67},
            {(std::byte) 0x76,(std::byte) 0x54,(std::byte) 0x32,(std::byte) 0x10}
        );
        auto publisher =
                (transport == "rabbitmq")
            ?
                transport::rabbitmq::RabbitMQImporterExporter<TheEnvironment>
                ::createExporter(address)
            :
                (
                    (transport == "mcast")
                ?
                    transport::multicast::MulticastImporterExporter<TheEnvironment>
                    ::createExporter(address)
                :
                    (
                        (transport == "zmq")
                    ?
                        transport::zeromq::ZeroMQImporterExporter<TheEnvironment>
                        ::createExporter(address)
                    :
                        transport::redis::RedisImporterExporter<TheEnvironment>
                        ::createExporter(address)                   
                    )
                )
            ;

        auto filter = Monad::kleisli<basic::ByteDataWithTopic>(
            basic::CommonFlowUtilComponents<Monad>
                ::pureFilter<basic::ByteDataWithTopic>(
                    [](basic::ByteDataWithTopic const &d) {
                        return (!d.content.empty());
                    }
                )
        );

        r.exportItem("publisher", publisher
            , r.execute("filter", filter, r.importItem("importer", importer)));

        auto exiter = Monad::simpleExporter<basic::ByteDataWithTopic>(
            [](Monad::InnerData<basic::ByteDataWithTopic> &&d) {
                if (d.timedData.finalFlag) {
                    d.environment->log(infra::LogLevel::Info, "Got the final update!");
                    std::thread([]() {
                        std::this_thread::sleep_for(std::chrono::seconds(10));
                        exit(0);
                    }).detach();
                }
            }
        );
        r.exportItem("exiter", exiter, r.importItem(importer));

        r.finalize();

        infra::terminationController(infra::TerminateAtTimePoint {
            env.actualTime(
                firstTimePoint+std::chrono::hours(24)
            )
        });

        ifs.close();
    }
    return 0;
}
