#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/BasicWithTimeMonad.hpp>
#include <tm_kit/infra/RealTimeMonad.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/transaction/SingleKeyTransactionInterface.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQOnOrderFacility.hpp>

#include "dbData.pb.h"
#include "DBDataEq.hpp"

#include <boost/program_options.hpp>

#include <iostream>

using namespace dev::cd606::tm;
using namespace db_subscription;

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("command", po::value<std::string>(), "the command (subscribe, insert, update, delete, unsubscribe)")
        ("key", po::value<std::string>(), "key for the command")
        ("value1", po::value<int>(), "value1 for the command")
        ("value2", po::value<std::string>(), "value2 for the command")
        ("old_version", po::value<int64_t>(), "old version for the command")
        ("old_value1", po::value<int>(), "old value1 for the command")
        ("old_value2", po::value<std::string>(), "old value2 for the command")
        ("id", po::value<std::string>(), "id for the command")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    if (!vm.count("command")) {
        std::cerr << "Please provide command\n";
        return 1;
    }

    auto cmd = vm["command"].as<std::string>();
    if (cmd != "subscribe" && cmd != "insert" && cmd != "update" && cmd != "delete" && cmd != "unsubscribe") {
        std::cerr << "Command must be subscribe, insert, update, delete or unsubsribe\n";
        return 1;
    }

    std::string key;
    int value1 = 0;
    std::string value2;
    int old_value1 = 0;
    std::string old_value2;
    int64_t old_version = 0;
    std::string idStr;
    if (cmd == "subscribe") {
        if (!vm.count("key")) {
            std::cerr << "Please provide key for command\n";
            return 1;
        }
        key = vm["key"].as<std::string>();
    } else if (cmd == "unsubscribe") {
        if (!vm.count("key")) {
            std::cerr << "Please provide key for command\n";
            return 1;
        }
        key = vm["key"].as<std::string>();
        if (!vm.count("id")) {
            std::cerr << "Please provide id for command\n";
            return 1;
        }
        idStr = vm["id"].as<std::string>();
    } else if (cmd == "insert") {
        if (!vm.count("key")) {
            std::cerr << "Please provide key for command\n";
            return 1;
        }
        key = vm["key"].as<std::string>();
        if (!vm.count("value1")) {
            std::cerr << "Please provide value1 for command\n";
            return 1;
        }
        value1 = vm["value1"].as<int>();
        if (!vm.count("value2")) {
            std::cerr << "Please provide value2 for command\n";
            return 1;
        }
        value2 = vm["value2"].as<std::string>();
    } else if (cmd == "update") {
        if (!vm.count("key")) {
            std::cerr << "Please provide key for command\n";
            return 1;
        }
        key = vm["key"].as<std::string>();
        if (!vm.count("value1")) {
            std::cerr << "Please provide value1 for command\n";
            return 1;
        }
        value1 = vm["value1"].as<int>();
        if (!vm.count("value2")) {
            std::cerr << "Please provide value2 for command\n";
            return 1;
        }
        value2 = vm["value2"].as<std::string>();
        if (!vm.count("old_value1")) {
            std::cerr << "Please provide old value1 for command\n";
            return 1;
        }
        old_value1 = vm["old_value1"].as<int>();
        if (!vm.count("old_value2")) {
            std::cerr << "Please provide old value2 for command\n";
            return 1;
        }
        old_value2 = vm["old_value2"].as<std::string>();
        if (!vm.count("old_version")) {
            std::cerr << "Please provide old version for command\n";
            return 1;
        }
        old_version = vm["old_version"].as<int64_t>();
    } else if (cmd == "delete") {
        if (!vm.count("key")) {
            std::cerr << "Please provide key for command\n";
            return 1;
        }
        key = vm["key"].as<std::string>();
        if (!vm.count("old_value1")) {
            std::cerr << "Please provide old value1 for command\n";
            return 1;
        }
        old_value1 = vm["old_value1"].as<int>();
        if (!vm.count("old_value2")) {
            std::cerr << "Please provide old value2 for command\n";
            return 1;
        }
        old_value2 = vm["old_value2"].as<std::string>();
        if (!vm.count("old_version")) {
            std::cerr << "Please provide old version for command\n";
            return 1;
        }
        old_version = vm["old_version"].as<int64_t>();
    }

    using TI = basic::transaction::SingleKeyTransactionInterface<
        std::string
        , db_data
        , int64_t
        , transport::BoostUUIDComponent::IDType
    >;
    
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::real_time_clock::ClockComponent>,
        transport::BoostUUIDComponent,
        transport::ClientSideSimpleIdentityAttacherComponent<
            std::string
            , TI::BasicFacilityInput>,
        transport::rabbitmq::RabbitMQComponent
    >;
    using M = infra::RealTimeMonad<TheEnvironment>;
    using R = infra::MonadRunner<M>;

    TheEnvironment env;
    env.transport::ClientSideSimpleIdentityAttacherComponent<std::string,TI::BasicFacilityInput>::operator=(
        transport::ClientSideSimpleIdentityAttacherComponent<std::string,TI::BasicFacilityInput>(
            "db_subscription_client"
        )
    );

    R r(&env); 
    auto facility = transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::createTypedRPCOnOrderFacility
        <TI::BasicFacilityInput,TI::FacilityOutput>(
        transport::ConnectionLocator::parse("127.0.0.1::guest:guest:test_db_cmd_queue")
    );
    r.registerOnOrderFacility("facility", facility);

    auto exporter = M::simpleExporter<M::KeyedData<TI::BasicFacilityInput,TI::FacilityOutput>>(
        [](M::InnerData<M::KeyedData<TI::BasicFacilityInput,TI::FacilityOutput>> &&data) {
            auto id = data.timedData.value.key.id();
            auto input = data.timedData.value.key.key();
            auto output = std::move(data.timedData.value.data);
            bool isFinal = data.timedData.finalFlag;
            auto *env = data.environment;

            switch (output.value.index()) {
            case 0:
                {
                    TI::TransactionResult const &tr = std::get<0>(output.value);
                    std::ostringstream oss;
                    switch (tr.index()) {
                    case 0:
                        oss << "Got transaction success for " << env->id_to_string(id);
                        break;
                    case 1:
                        oss << "Got transaction failure by permission for " << env->id_to_string(id);
                        break;
                    case 2:
                        oss << "Got transaction failure by precondition for " << env->id_to_string(id);
                        break;
                    case 3:
                        oss << "Got transaction queued asynchronously for " << env->id_to_string(id);
                        break;
                    default:
                        oss << "Got unknown transaction failure for " << env->id_to_string(id);
                        break;
                    }
                    if (isFinal) {
                        oss << " [F]";
                    }
                    env->log(infra::LogLevel::Info, oss.str());
                }
                break;
            case 1:
                {
                    std::ostringstream oss;
                    oss << "Got subscription ack for " << env->id_to_string(id);
                    if (isFinal) {
                        oss << " [F]";
                    }
                    env->log(infra::LogLevel::Info, oss.str());
                }
                break;
            case 2:
                {
                    std::ostringstream oss;
                    oss << "Got unsubscription ack for " << env->id_to_string(id);
                    if (isFinal) {
                        oss << " [F]";
                    }
                    env->log(infra::LogLevel::Info, oss.str());
                }
                break;
            case 3:
                {
                    std::ostringstream oss;
                    TI::OneValue const &tr = std::get<3>(output.value);
                    if (tr.data) {
                        oss << "Got insert: [name='" << tr.groupID << "'"
                            << ",value1="<< tr.data->value1()
                            << ",value2='" << tr.data->value2() << "'"
                            << ",version=" << tr.version
                            << "] for " << env->id_to_string(id);
                    } else {
                        oss << "Got delete: [name='" << tr.groupID << "'"
                            << ",version=" << tr.version
                            << "] for " << env->id_to_string(id);
                    }
                    if (isFinal) {
                        oss << " [F]";
                    }
                    env->log(infra::LogLevel::Info, oss.str());
                }
                break;
            case 4:
                {
                    std::ostringstream oss;
                    TI::OneDelta const &tr = std::get<4>(output.value);
                    oss << "Got update: [name='" << tr.groupID << "'"
                        << ",value1="<< tr.data.value1()
                        << ",value2='" << tr.data.value2() << "'"
                        << ",version=" << tr.version
                        << "] for " << env->id_to_string(id);
                    if (isFinal) {
                        oss << " [F]";
                    }
                    env->log(infra::LogLevel::Info, oss.str());
                }
                break;
            }
            if (isFinal) {
                env->log(infra::LogLevel::Info, "Got final update, exiting");
                exit(0);
            }
        }
    );

    auto importer = basic::real_time_clock::template ClockImporter<TheEnvironment>
                    ::template createOneShotClockConstImporter<basic::VoidStruct>(
        env.now()+std::chrono::milliseconds(100)
        , basic::VoidStruct {}
    );

    auto createCommand = M::liftMaybe<basic::VoidStruct>(
        [&env,cmd,key,value1,value2,old_value1,old_value2,old_version,idStr](basic::VoidStruct &&) -> std::optional<TI::BasicFacilityInput> {
            if (cmd == "subscribe") {
                return TI::BasicFacilityInput {
                    { TI::Subscription {key} }
                };
            } else if (cmd == "unsubscribe") {
                return TI::BasicFacilityInput {
                    { TI::Unsubscription {env.id_from_string(idStr), key} }
                };
            } else if (cmd == "insert") {
                db_data item;
                item.set_value1(value1);
                item.set_value2(value2);
                return TI::BasicFacilityInput { 
                    { TI::Transaction {
                        TI::InsertAction {key, item}
                    } }
                };
            } else if (cmd == "update") {
                db_data item;
                item.set_value1(value1);
                item.set_value2(value2);
                db_data old_item;
                old_item.set_value1(old_value1);
                old_item.set_value2(old_value2);
                return TI::BasicFacilityInput { 
                    { TI::Transaction {
                        TI::UpdateAction {key, old_version, old_item, item}
                    } }
                };
            } else if (cmd == "delete") {
                db_data old_item;
                old_item.set_value1(old_value1);
                old_item.set_value2(old_value2);
                return TI::BasicFacilityInput { 
                    { TI::Transaction {
                        TI::DeleteAction {key, old_version, old_item}
                    } }
                };
            } else {
                return std::nullopt;
            }
        }
    );

    auto keyify = M::liftPure<TI::BasicFacilityInput>(
        [](TI::BasicFacilityInput &&x) {
            return infra::withtime_utils::keyify
                <TI::BasicFacilityInput,TheEnvironment>(std::move(x));
        }
    );

    r.placeOrderWithFacility(
        r.execute("keyify", keyify, 
            r.execute("createCommand", createCommand, 
                r.importItem("importer", importer)))
        , facility
        , r.exporterAsSink("exporter", exporter)
    );
    
    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "db_subscription_client");
    r.finalize();

    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "DB subscription client started");

    infra::terminationController(infra::RunForever {});
}