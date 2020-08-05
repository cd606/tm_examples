#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/BasicWithTimeApp.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/KleisliUtils.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/transaction/v2/TransactionLogicCombination.hpp>
#include <tm_kit/basic/transaction/v2/DataStreamClientCombination.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

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

void diMain(std::string const &cmd, std::string const &key, std::string const &idStr) {
    using DI = basic::transaction::v2::DataStreamInterface<
        int64_t
        , std::string
        , int64_t
        , db_data
    >;
    using GS = basic::transaction::v2::GeneralSubscriberTypes<
        boost::uuids::uuid, DI
    >;
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::real_time_clock::ClockComponent>,
        transport::BoostUUIDComponent,
        transport::rabbitmq::RabbitMQComponent,
        transport::ClientSideSimpleIdentityAttacherComponent<
            std::string
            , GS::Input>
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    env.transport::ClientSideSimpleIdentityAttacherComponent<std::string,GS::Input>::operator=(
        transport::ClientSideSimpleIdentityAttacherComponent<std::string,GS::Input>(
            "db_subscription_client"
        )
    ); 

    R r(&env); 

    auto facility = transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::createTypedRPCOnOrderFacility
        <GS::Input,GS::Output>(
        transport::ConnectionLocator::parse("127.0.0.1::guest:guest:test_db_cmd_subscription_queue")
    );
    r.registerOnOrderFacility("facility", facility);

    auto printAck = M::simpleExporter<M::KeyedData<GS::Input,GS::Output>>(
        [&env](M::InnerData<M::KeyedData<GS::Input,GS::Output>> &&o) {
            auto id = o.timedData.value.key.id();
            std::visit([&id,&env](auto const &x) {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T,GS::Subscription>) {
                    std::ostringstream oss;
                    oss << "Got subscription ack for " << env.id_to_string(id)
                        << " on " << x.keys.size() << " keys";
                    env.log(infra::LogLevel::Info, oss.str());
                } else if constexpr (std::is_same_v<T,GS::Unsubscription>) {
                    std::ostringstream oss;
                    oss << "Got unsubscription ack for " << env.id_to_string(x.originalSubscriptionID)
                        << " from " << env.id_to_string(id);
                    env.log(infra::LogLevel::Info, oss.str());
                } else if constexpr (std::is_same_v<T,GS::SubscriptionInfo>) {
                    std::ostringstream oss;
                    oss << "Got subscription info " << x;
                    env.log(infra::LogLevel::Info, oss.str());
                } else if constexpr (std::is_same_v<T,GS::UnsubscribeAll>) {
                    std::ostringstream oss;
                    oss << "Got unsubscribe-all ack from " << env.id_to_string(id);
                    env.log(infra::LogLevel::Info, oss.str());
                }
            }, o.timedData.value.data.value);
            if (o.timedData.finalFlag) {
                env.log(infra::LogLevel::Info, "Got final update, exiting");
                env.exit();
            }
        }
    );

    auto printFullUpdate = M::pureExporter<M::KeyedData<GS::Input,DI::FullUpdate>>(
        [&env](M::KeyedData<GS::Input,DI::FullUpdate> &&update) {
            std::ostringstream oss;
            oss << "Got full update {";
            oss << "globalVersion=" << update.data.version;
            oss << ",updates=[";
            int ii = 0;
            for (auto const &item : update.data.data) {
                if (ii > 0) {
                    oss << ",";
                }
                ++ii;
                oss << "{key=" << item.groupID;
                oss << ",version=" << item.version;
                if (item.data) {
                    oss << ",data={value1=" << item.data->value1() << ",value2='" << item.data->value2() << "'}";
                } else {
                    oss << ",data=(deleted)";
                }
                oss << "}";
            }
            oss << "]";
            oss << "} (from " << env.id_to_string(update.key.id()) << ")";
            env.log(infra::LogLevel::Info, oss.str());
        }
    );

    auto createCommand = M::liftMaybe<basic::VoidStruct>(
        [&env,cmd,key,idStr](basic::VoidStruct &&) -> std::optional<GS::Input> {
            if (cmd == "subscribe") {
                return GS::Input {
                    GS::Subscription { std::vector<DI::Key> {key} }
                };
            } else if (cmd == "unsubscribe") {
                if (idStr == "all") {
                    return GS::Input {
                        GS::UnsubscribeAll {}
                    };
                } else {
                    return GS::Input {
                        GS::Unsubscription {env.id_from_string(idStr)}
                    };
                }
            } else if (cmd == "list") {
                return GS::Input {
                    GS::ListSubscriptions {}
                };
            } else {
                return std::nullopt;
            }
        }
    );

    auto keyify = M::template kleisli<typename GS::Input>(
        basic::CommonFlowUtilComponents<M>::template keyify<typename GS::Input>()
    );

    auto initialImporter = M::constFirstPushImporter(
        basic::VoidStruct {}
    );

    auto createdCommand = r.execute("createCommand", createCommand, r.importItem("initialImporter", initialImporter));
    auto keyedCommand = r.execute("keyify", keyify, std::move(createdCommand));
    
    auto clientOutputs = basic::transaction::v2::dataStreamClientCombination<R, DI, typename GS::Input>(
        r 
        , "outputHandling"
        , R::facilityConnector(facility)
        , std::move(keyedCommand)
    );
    r.exportItem("printAck", printAck, clientOutputs.rawSubscriptionOutputs.clone());
    r.exportItem("printFullUpdate", printFullUpdate, clientOutputs.fullUpdates.clone());

    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "db_subscription_client");
    r.finalize();

    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "DB subscription client started");

    infra::terminationController(infra::RunForever {&env});
}

void tiMain(std::string const &cmd, std::string const &key, int value1, std::string const &value2
        , int oldValue1, std::string const &oldValue2, int oldVersion, bool force) {
    using TI = basic::transaction::v2::TransactionInterface<
        int64_t
        , std::string
        , int64_t
        , db_data
    >;

    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::real_time_clock::ClockComponent>,
        transport::BoostUUIDComponent,
        transport::rabbitmq::RabbitMQComponent,
        transport::ClientSideSimpleIdentityAttacherComponent<
            std::string
            , TI::Transaction>
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    env.transport::ClientSideSimpleIdentityAttacherComponent<std::string,TI::Transaction>::operator=(
        transport::ClientSideSimpleIdentityAttacherComponent<std::string,TI::Transaction>(
            "db_subscription_client"
        )
    );

    R r(&env); 

    auto facility = transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::createTypedRPCOnOrderFacility
        <TI::Transaction,TI::TransactionResponse>(
        transport::ConnectionLocator::parse("127.0.0.1::guest:guest:test_db_cmd_transaction_queue")
    );

    auto initialImporter = M::simpleImporter<basic::VoidStruct>(
        [](M::PublisherCall<basic::VoidStruct> &p) {
            p(basic::VoidStruct {});
        }
        , infra::LiftParameters<std::chrono::system_clock::time_point>()
            .SuggestThreaded(true)
    );

    auto createCommand = M::liftMaybe<basic::VoidStruct>(
        [cmd,key,value1,value2,oldValue1,oldValue2,oldVersion,force](basic::VoidStruct &&) -> std::optional<TI::Transaction> {
            if (cmd == "insert") {
                db_data item;
                item.set_value1(value1);
                item.set_value2(value2);
                return TI::Transaction { {
                    TI::InsertAction {key, item}
                } };
            } else if (cmd == "update") {
                db_data item;
                item.set_value1(value1);
                item.set_value2(value2);
                db_data old_item;
                old_item.set_value1(oldValue1);
                old_item.set_value2(oldValue2);
                if (force) {
                    return TI::Transaction { {
                        TI::UpdateAction {
                            key, std::nullopt, std::nullopt, item
                        }
                    } };
                } else {
                    return TI::Transaction { {
                        TI::UpdateAction {
                            key, oldVersion, old_item, item
                        }
                    } };
                }
            } else if (cmd == "delete") {
                db_data old_item;
                old_item.set_value1(oldValue1);
                old_item.set_value2(oldValue2);
                if (force) {
                    return TI::Transaction { {
                        TI::DeleteAction {
                            key, std::nullopt, std::nullopt
                        }
                    } };
                } else {
                    return TI::Transaction { {
                        TI::DeleteAction {
                            key, oldVersion, old_item
                        }
                    } };
                }
            } else {
                return std::nullopt;
            }
        }
    );

    auto keyify = M::template kleisli<typename TI::Transaction>(
        basic::CommonFlowUtilComponents<M>::template keyify<typename TI::Transaction>()
    );

    auto printResponse = M::simpleExporter<M::KeyedData<TI::Transaction,TI::TransactionResponse>>(
        [&env](M::InnerData<M::KeyedData<TI::Transaction,TI::TransactionResponse>> &&r) {
            auto const &resp = r.timedData.value.data.value;
            std::ostringstream oss;
            oss << "Got transaction response {";
            oss << "globalVersion=" << resp.globalVersion;
            oss << ",requestDecision=" << resp.requestDecision;
            oss << "}";
            env.log(infra::LogLevel::Info, oss.str());
            if (r.timedData.finalFlag) {
                env.log(infra::LogLevel::Info, "Got final update, exiting");
                env.exit();
            }
        }
    );

    auto createdCommand = r.execute("createCommand", createCommand, r.importItem("initialImporter", initialImporter));
    auto keyedCommand = r.execute("keyify", keyify, std::move(createdCommand));
    r.placeOrderWithFacility(std::move(keyedCommand), "facility", facility, r.exporterAsSink("printResponse", printResponse));

    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "db_subscription_client");
    r.finalize();

    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "DB subscription client started");

    infra::terminationController(infra::RunForever {&env});
}

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("command", po::value<std::string>(), "the command (subscribe, insert, update, delete, unsubscribe, list)")
        ("key", po::value<std::string>(), "key for the command")
        ("value1", po::value<int>(), "value1 for the command")
        ("value2", po::value<std::string>(), "value2 for the command")
        ("old_version", po::value<int64_t>(), "old version for the command")
        ("old_value1", po::value<int>(), "old value1 for the command")
        ("old_value2", po::value<std::string>(), "old value2 for the command")
        ("id", po::value<std::string>(), "id for the command")
        ("force", "ignore checks in update/delete")
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
    if (cmd != "subscribe" && cmd != "insert" && cmd != "update" && cmd != "delete" && cmd != "unsubscribe" && cmd != "list") {
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
    bool force = false;
    if (cmd == "subscribe") {
        if (!vm.count("key")) {
            std::cerr << "Please provide key for command\n";
            return 1;
        }
        key = vm["key"].as<std::string>();
        diMain(cmd, key, idStr);
    } else if (cmd == "unsubscribe") {
        if (!vm.count("id")) {
            std::cerr << "Please provide id for command\n";
            return 1;
        }
        idStr = vm["id"].as<std::string>();
        diMain(cmd, key, idStr);
    } else if (cmd == "list") {
        diMain(cmd, key, idStr);
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
        tiMain(cmd, key, value1, value2, old_value1, old_value2, old_version, force);
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
        force = vm.count("force");
        tiMain(cmd, key, value1, value2, old_value1, old_value2, old_version, force);
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
        force = vm.count("force");
        tiMain(cmd, key, value1, value2, old_value1, old_value2, old_version, force);    
    }
}