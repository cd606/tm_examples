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
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>

#include "ReadOnlyDBData.hpp"

#include <boost/program_options.hpp>

#include <iostream>

using namespace dev::cd606::tm;

void diMain(std::string const &cmd, std::string const &key, std::string const &idStr) {
    using DI = basic::transaction::complex_key_value_store::per_item::DI<DBKey, DBData>;
    using GS = basic::transaction::complex_key_value_store::per_item::GS<boost::uuids::uuid, DBKey, DBData>;
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

    auto facility = transport::MultiTransportRemoteFacilityManagingUtils<R>::setupSimpleRemoteFacilityWithProtocol
        <basic::CBOR,GS::Input,GS::Output>(
        r, "rabbitmq://127.0.0.1::guest:guest:test_db_cmd_subscription_queue_2"
    );
    r.registerOnOrderFacility("facility", facility);

    auto printAck = M::simpleExporter<M::KeyedData<GS::Input,GS::Output>>(
        [&env](M::InnerData<M::KeyedData<GS::Input,GS::Output>> &&o) {
            auto id = o.timedData.value.key.id();
            std::visit([&id,&env](auto const &x) {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T,typename GS::Subscription>) {
                    std::ostringstream oss;
                    oss << "Got subscription ack for " << env.id_to_string(id)
                        << " on " << x.keys.size() << " keys";
                    env.log(infra::LogLevel::Info, oss.str());
                } else if constexpr (std::is_same_v<T,typename GS::Unsubscription>) {
                    std::ostringstream oss;
                    oss << "Got unsubscription ack for " << env.id_to_string(x.originalSubscriptionID)
                        << " from " << env.id_to_string(id);
                    env.log(infra::LogLevel::Info, oss.str());
                } else if constexpr (std::is_same_v<T,typename GS::SubscriptionInfo>) {
                    std::ostringstream oss;
                    oss << "Got subscription info " << x;
                    env.log(infra::LogLevel::Info, oss.str());
                } else if constexpr (std::is_same_v<T,typename GS::UnsubscribeAll>) {
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
            oss << "updates=[";
            int ii = 0;
            for (auto const &item : update.data.data) {
                if (ii > 0) {
                    oss << ",";
                }
                ++ii;
                oss << "{key=" << item.groupID;
                if (item.data) {
                    oss << ",data=" << *(item.data);
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
                    GS::Subscription { std::vector<DI::Key> {DBKey {key}} }
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
            } else if (cmd == "snapshot") {
                return GS::Input {
                    GS::SnapshotRequest { std::vector<DI::Key> {DBKey {key}} }
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
    
    auto clientOutputs = basic::transaction::complex_key_value_store::per_item::Combinations<R,DBKey,DBData>
        ::dataStreamClientCombinationFunc()
    (
        r 
        , "outputHandling"
        , R::facilityConnector(facility)
        , std::move(keyedCommand)
        , nullptr
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

void tiMain(std::string const &cmd, std::string const &key, int value1, std::string const &value2) {
    using TI = basic::transaction::complex_key_value_store::per_item::TI<DBKey, DBData>;

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

    auto facility = transport::MultiTransportRemoteFacilityManagingUtils<R>::setupSimpleRemoteFacilityWithProtocol
        <basic::CBOR, TI::Transaction,TI::TransactionResponse>(
        r, "rabbitmq://127.0.0.1::guest:guest:test_db_cmd_transaction_queue_2"
    );

    auto initialImporter = M::simpleImporter<basic::VoidStruct>(
        [](M::PublisherCall<basic::VoidStruct> &p) {
            p(basic::VoidStruct {});
        }
        , infra::LiftParameters<std::chrono::system_clock::time_point>()
            .SuggestThreaded(true)
    );

    auto createCommand = M::liftMaybe<basic::VoidStruct>(
        [cmd,key,value1,value2](basic::VoidStruct &&) -> std::optional<TI::Transaction> {
            if (cmd == "insert") {
                DBData item {value1, value2};
                return TI::Transaction { {
                    TI::InsertAction {DBKey {key}, item}
                } };
            } else if (cmd == "update") {
                DBData item {value1, value2};
                return TI::Transaction { {
                    TI::UpdateAction {
                        DBKey {key}, std::nullopt, std::nullopt, item
                    }
                } };
            } else if (cmd == "delete") {
                return TI::Transaction { {
                    TI::DeleteAction {
                        DBKey {key}, std::nullopt, std::nullopt
                    }
                } };
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
            auto const &resp = r.timedData.value.data;
            std::ostringstream oss;
            oss << "Got transaction response {";
            oss << "requestDecision=" << resp.requestDecision;
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
        ("command", po::value<std::string>(), "the command (subscribe, insert, update, delete, unsubscribe, list, snapshot)")
        ("key", po::value<std::string>(), "key for the command")
        ("value1", po::value<int>(), "value1 for the command")
        ("value2", po::value<std::string>(), "value2 for the command")
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
    if (cmd != "subscribe" && cmd != "insert" && cmd != "update" && cmd != "delete" && cmd != "unsubscribe" && cmd != "list" && cmd != "snapshot") {
        std::cerr << "Command must be subscribe, insert, update, delete, unsubsribe, list or snapshot\n";
        return 1;
    }

    std::string key;
    int value1 = 0;
    std::string value2;
    std::string idStr;
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
        tiMain(cmd, key, value1, value2);
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
        tiMain(cmd, key, value1, value2);
    } else if (cmd == "delete") {
        if (!vm.count("key")) {
            std::cerr << "Please provide key for command\n";
            return 1;
        }
        key = vm["key"].as<std::string>();
        tiMain(cmd, key, value1, value2);    
    } else if (cmd == "snapshot") {
        if (!vm.count("key")) {
            std::cerr << "Please provide key for command\n";
            return 1;
        }
        key = vm["key"].as<std::string>();
        diMain(cmd, key, idStr);
    }
}
