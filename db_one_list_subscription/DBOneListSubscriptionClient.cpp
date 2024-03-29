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
#include <tm_kit/transport/multicast/MulticastComponent.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>
#include <tm_kit/transport/SyntheticMultiTransportFacility.hpp>

#include "DBData.hpp"

#include <boost/program_options.hpp>

#include <iostream>

using namespace dev::cd606::tm;
using namespace db_one_list_subscription;

void diMain(std::string const &cmd, std::string const &idStr, bool synthetic) {
    using DI = basic::transaction::named_value_store::DI<db_data>;
    using GS = basic::transaction::named_value_store::GS<transport::CrossGuidComponent::IDType,db_data>;
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::CrossGuidComponent,
        transport::rabbitmq::RabbitMQComponent,
        transport::multicast::MulticastComponent,
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

    R::FacilitioidConnector<GS::Input,GS::Output> facilityConn;

    if (synthetic) {
        facilityConn = transport::SyntheticMultiTransportFacility<R>
            ::client<
                basic::CBOR, basic::CBOR 
                , GS::Input, GS::Output
                , true
            >(
                r 
                , "synthetic_facility/"
                , "multicast://224.0.0.1:34567:::db_one_list_subscription_input"
                , "synthetic_subscription_input"
                , "multicast://224.0.0.1:34567:::db_one_list_subscription_output"
                , "synthetic_subscription_output"
            );
    } else {
        auto facility = transport::MultiTransportRemoteFacilityManagingUtils<R>::setupSimpleRemoteFacilityWithProtocol
            <basic::CBOR,GS::Input,GS::Output>(
            r, "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_subscription_queue"
        );
        r.registerOnOrderFacility("facility", facility);
        facilityConn = r.facilityConnector(facility);
    }

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
                    oss << ",data=[";
                    int jj = 0;
                    for (auto const &row : *(item.data)) {
                        if (jj > 0) {
                            oss << ',';
                        }
                        ++jj;
                        oss << "{name='" << row.first << "'";
                        oss << ",amount=" << row.second.amount;
                        oss << ",stat=" << row.second.stat;
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
                return basic::transaction::named_value_store::subscription<M,db_data>();
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
                return basic::transaction::named_value_store::snapshotRequest<M,db_data>();
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
    auto clientOutputs = basic::transaction::v2::dataStreamClientCombination<
        R, DI
        , basic::transaction::named_value_store::VersionMerger
        , basic::transaction::named_value_store::ApplyDelta<db_data>
    >(
        r 
        , "outputHandling"
        , facilityConn
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

void tiMain(std::string const &cmd, std::string const &name, int amount, double stat, int64_t oldVersion, size_t oldDataCount, bool synthetic) {
    using TI = basic::transaction::named_value_store::TI<db_data>;

    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::CrossGuidComponent,
        transport::rabbitmq::RabbitMQComponent,
        transport::multicast::MulticastComponent,
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

    R::FacilitioidConnector<TI::Transaction, TI::TransactionResponse> facilityConn;
    
    if (synthetic) {
        facilityConn = transport::SyntheticMultiTransportFacility<R>
            ::client<
                basic::CBOR, basic::CBOR 
                , TI::Transaction, TI::TransactionResponse
                , true
            >(
                r 
                , "transaction_facility/"
                , "multicast://224.0.0.1:34568:::db_one_list_transaction_input"
                , "synthetic_transaction_input"
                , "multicast://224.0.0.1:34568:::db_one_list_transaction_output"
                , "synthetic_transaction_output"
            );
    } else {
        auto facility = transport::MultiTransportRemoteFacilityManagingUtils<R>::setupSimpleRemoteFacilityWithProtocol
            <basic::CBOR,TI::Transaction,TI::TransactionResponse>(
            r, "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_transaction_queue"
        );
        r.registerOnOrderFacility("facility", facility);
        facilityConn = r.facilityConnector(facility);
    }

    auto initialImporter = M::simpleImporter<basic::VoidStruct>(
        [](M::PublisherCall<basic::VoidStruct> &p) {
            p(basic::VoidStruct {});
        }
        , infra::LiftParameters<std::chrono::system_clock::time_point>()
            .SuggestThreaded(true)
    );

    auto createCommand = M::liftMaybe<basic::VoidStruct>(
        [cmd,name,amount,stat,oldVersion,oldDataCount](basic::VoidStruct &&) -> std::optional<TI::Transaction> {
            if (cmd == "update") {
                basic::transaction::named_value_store::CollectionDelta<db_data> delta;
                delta.inserts_updates.push_back({
                    name, db_data {amount, stat}
                });

                return TI::Transaction { {
                    TI::UpdateAction {
                        basic::VoidStruct {}, oldVersion, oldDataCount, delta
                    }
                } };
            } else if (cmd == "delete") {
                basic::transaction::named_value_store::CollectionDelta<db_data> delta;
                delta.deletes.push_back(name);

                return TI::Transaction { {
                    TI::UpdateAction {
                        basic::VoidStruct {}, oldVersion, oldDataCount, delta
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
    facilityConn(r, std::move(keyedCommand), r.exporterAsSink("printResponse", printResponse));

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
        ("old_version", po::value<int64_t>(), "old version for the command")
        ("old_data_count", po::value<size_t>(), "old data count for the command")
        ("id", po::value<std::string>(), "id for the command")
        ("synthetic", "use synthetic clients")
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
    int64_t old_version = 0;
    std::string idStr;
    size_t old_data_count = 0;
    bool synthetic = (vm.count("synthetic") != 0);
    if (cmd == "subscribe") {
        //no param needed
        diMain(cmd, idStr, synthetic);
    } else if (cmd == "unsubscribe") {
        if (!vm.count("id")) {
            std::cerr << "Please provide id for command\n";
            return 1;
        }
        idStr = vm["id"].as<std::string>();
        diMain(cmd, idStr, synthetic);
    } else if (cmd == "list") {
        diMain(cmd, idStr, synthetic);
    } else if (cmd == "snapshot") {
        diMain(cmd, idStr, synthetic);
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
        if (!vm.count("old_version")) {
            std::cerr << "Please provide old version for command\n";
            return 1;
        }
        old_version = vm["old_version"].as<int64_t>();
        if (!vm.count("old_data_count")) {
            std::cerr << "Please provide old data count for command\n";
            return 1;
        }
        old_data_count = vm["old_data_count"].as<size_t>();
        tiMain(cmd, name, amount, stat, old_version, old_data_count, synthetic);
    } else if (cmd == "delete") {
        if (!vm.count("name")) {
            std::cerr << "Please provide name for command\n";
            return 1;
        }
        name = vm["name"].as<std::string>();
        old_version = vm["old_version"].as<int64_t>();
        if (!vm.count("old_data_count")) {
            std::cerr << "Please provide old data count for command\n";
            return 1;
        }
        old_data_count = vm["old_data_count"].as<size_t>();
        tiMain(cmd, name, amount, stat, old_version, old_data_count, synthetic);
    }
}
