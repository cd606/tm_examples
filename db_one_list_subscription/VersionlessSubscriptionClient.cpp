#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/transaction/v2/TransactionLogicCombination.hpp>
#include <tm_kit/basic/transaction/v2/DataStreamClientCombination.hpp>
#include <tm_kit/basic/transaction/named_value_store/DataModel.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>

#include "ReadOnlyDBOneListData.hpp"

#include <boost/program_options.hpp>

#include <iostream>

using namespace dev::cd606::tm;

void diMain(std::string const &cmd, std::string const &idStr) {
    using DI = basic::transaction::complex_key_value_store::as_collection::DI<DBKey, DBData>;
    using GS = basic::transaction::complex_key_value_store::as_collection::GS<transport::CrossGuidComponent::IDType, DBKey, DBData>;
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::CrossGuidComponent,
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
            "db_one_list_subscription_client"
        )
    ); 

    R r(&env); 

    auto facility = transport::MultiTransportRemoteFacilityManagingUtils<R>::setupSimpleRemoteFacility
        <GS::Input,GS::Output>(
        r, "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_subscription_queue_2"
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
            oss << "updates=[";
            int ii = 0;
            for (auto const &item : update.data.data) {
                if (ii > 0) {
                    oss << ",";
                }
                ++ii;
                oss << "{key=" << item.groupID;
                if (item.data) {
                    oss << ",data=[";
                    int jj = 0;
                    for (auto const &row : *(item.data)) {
                        if (jj > 0) {
                            oss << ',';
                        }
                        ++jj;
                        oss << "{DBKey=" << row.first;
                        oss << ",DBData=" << row.second;
                        oss << "}";
                    }
                    oss << "]";
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
        [&env,cmd,idStr](basic::VoidStruct &&) -> std::optional<GS::Input> {
            if (cmd == "subscribe") {
                return GS::Input {
                    GS::Subscription { std::vector<DI::Key> {DI::Key {}} }
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
                    GS::SnapshotRequest { std::vector<DI::Key> {DI::Key {}} }
                };
            } else {
                return std::nullopt;
            }
        }
    );

    auto keyify = M::kleisli<GS::Input>(
        basic::CommonFlowUtilComponents<M>::keyify<GS::Input>()
    );

    auto initialImporter = M::constFirstPushImporter(
        basic::VoidStruct {}
    );

    auto createdCommand = r.execute("createCommand", createCommand, r.importItem("initialImporter", initialImporter));
    auto keyedCommand = r.execute("keyify", keyify, std::move(createdCommand));
    auto clientOutputs = basic::transaction::complex_key_value_store::as_collection::Combinations<R,DBKey,DBData>
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

void tiMain(std::string const &cmd, std::string const &name, int amount, double stat, size_t oldDataCount) {
    using TI = basic::transaction::complex_key_value_store::as_collection::TI<DBKey, DBData>;

    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::CrossGuidComponent,
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
            "db_one_list_subscription_client"
        )
    );

    R r(&env); 

    auto facility = transport::MultiTransportRemoteFacilityManagingUtils<R>::setupSimpleRemoteFacility
        <TI::Transaction,TI::TransactionResponse>(
        r, "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_transaction_queue_2"
    );

    auto initialImporter = M::simpleImporter<basic::VoidStruct>(
        [](M::PublisherCall<basic::VoidStruct> &p) {
            p(basic::VoidStruct {});
        }
        , infra::LiftParameters<std::chrono::system_clock::time_point>()
            .SuggestThreaded(true)
    );

    auto createCommand = M::liftMaybe<basic::VoidStruct>(
        [cmd,name,amount,stat,oldDataCount](basic::VoidStruct &&) -> std::optional<TI::Transaction> {
            if (cmd == "update") {
                TI::DataDelta delta;
                delta.inserts_updates.push_back({
                    DBKey {name}, DBData {amount, stat}
                });

                return TI::Transaction { {
                    TI::UpdateAction {
                        basic::VoidStruct {}, std::nullopt, oldDataCount, delta
                    }
                } };
            } else if (cmd == "delete") {
                TI::DataDelta delta;
                delta.deletes.push_back(DBKey {name});

                return TI::Transaction { {
                    TI::UpdateAction {
                        basic::VoidStruct {}, std::nullopt, oldDataCount, delta
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
            auto const &resp = r.timedData.value.data.value;
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
        ("command", po::value<std::string>(), "the command (subscribe, update, delete, unsubscribe, list, snapshot)")
        ("name", po::value<std::string>(), "name for the command")
        ("amount", po::value<int>(), "amount for the command")
        ("stat", po::value<double>(), "stat for the command")
        ("old_data_count", po::value<size_t>(), "old data count for the command")
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
    if (cmd != "subscribe" && cmd != "update" && cmd != "delete" && cmd != "unsubscribe" && cmd != "list" && cmd != "snapshot") {
        std::cerr << "Command must be subscribe, update, delete, unsubsribe, list or snapshot\n";
        return 1;
    }

    std::string name;
    int amount = 0;
    double stat = 0.0;
    std::string idStr;
    size_t old_data_count = 0;
    if (cmd == "subscribe") {
        //no param needed
        diMain(cmd, idStr);
    } else if (cmd == "unsubscribe") {
        if (!vm.count("id")) {
            std::cerr << "Please provide id for command\n";
            return 1;
        }
        idStr = vm["id"].as<std::string>();
        diMain(cmd, idStr);
    } else if (cmd == "list") {
        diMain(cmd, idStr);
    } else if (cmd == "snapshot") {
        diMain(cmd, idStr);
    } else if (cmd == "update") {
        if (!vm.count("name")) {
            std::cerr << "Please provide name for command\n";
            return 1;
        }
        name = vm["name"].as<std::string>();
        if (!vm.count("amount")) {
            std::cerr << "Please provide amount for command\n";
            return 1;
        }
        amount = vm["amount"].as<int>();
        if (!vm.count("stat")) {
            std::cerr << "Please provide stat for command\n";
            return 1;
        }
        stat = vm["stat"].as<double>();
        if (!vm.count("old_data_count")) {
            std::cerr << "Please provide old data count for command\n";
            return 1;
        }
        old_data_count = vm["old_data_count"].as<size_t>();
        tiMain(cmd, name, amount, stat, old_data_count);
    } else if (cmd == "delete") {
        if (!vm.count("name")) {
            std::cerr << "Please provide name for command\n";
            return 1;
        }
        name = vm["name"].as<std::string>();
        if (!vm.count("old_data_count")) {
            std::cerr << "Please provide old data count for command\n";
            return 1;
        }
        old_data_count = vm["old_data_count"].as<size_t>();
        tiMain(cmd, name, amount, stat, old_data_count);
    }
}