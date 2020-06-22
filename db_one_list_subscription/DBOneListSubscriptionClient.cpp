#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeMonad.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/transaction/SingleKeyTransactionInterface.hpp>
#include <tm_kit/basic/transaction/TITransactionFacilityOutputToData.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQOnOrderFacility.hpp>

#include "TransactionHelpers.hpp"

#include <boost/program_options.hpp>

#include <iostream>

using namespace dev::cd606::tm;
using namespace db_one_list_subscription;

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("command", po::value<std::string>(), "the command (subscribe, update, delete, unsubscribe)")
        ("name", po::value<std::string>(), "name for the command")
        ("amount", po::value<int>(), "amount for the command")
        ("stat", po::value<double>(), "stat for the command")
        ("old_version", po::value<int64_t>(), "old version for the command")
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
    if (cmd != "subscribe" && cmd != "update" && cmd != "delete" && cmd != "unsubscribe") {
        std::cerr << "Command must be subscribe, update, delete or unsubsribe\n";
        return 1;
    }

    std::string name;
    int amount = 0;
    double stat = 0.0;
    int64_t old_version = 0;
    std::string idStr;
    size_t old_data_count = 0;
    if (cmd == "subscribe") {
        //no param needed
    } else if (cmd == "unsubscribe") {
        if (!vm.count("id")) {
            std::cerr << "Please provide id for command\n";
            return 1;
        }
        idStr = vm["id"].as<std::string>();
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
    }

    using TI = basic::transaction::SingleKeyTransactionInterface<
        Key
        , Data
        , int64_t
        , transport::BoostUUIDComponent::IDType
        , DataSummary
        , DataDelta
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
            "db_one_list_subscription_client"
        )
    );

    R r(&env); 
    auto facility = transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::createTypedRPCOnOrderFacility
        <TI::BasicFacilityInput,TI::FacilityOutput>(
        transport::ConnectionLocator::parse("127.0.0.1::guest:guest:test_db_one_list_cmd_queue")
    );
    r.registerOnOrderFacility("facility", facility);

    auto insertIntoFlow = M::kleisli<M::KeyedData<TI::BasicFacilityInput,TI::FacilityOutput>>(
        basic::CommonFlowUtilComponents<M>::idFunc<M::KeyedData<TI::BasicFacilityInput,TI::FacilityOutput>>()
    );
    auto extractor = basic::CommonFlowUtilComponents<M>::extractIDAndDataFromKeyedData<TI::BasicFacilityInput,TI::FacilityOutput>();
    auto convertToData = 
        basic::CommonFlowUtilComponents<M>::withKey<TI::FacilityOutput>(
            infra::KleisliUtils<M>::liftMaybe<TI::FacilityOutput>(
                basic::transaction::TITransactionFacilityOutputToData<
                    M, Key, Data, int64_t
                    , true //mutex protected
                    , DataSummary, CheckSummary, DataDelta, ApplyDelta
                >()
            )
        );
    auto extractAndConvert = 
        M::kleisli<M::KeyedData<TI::BasicFacilityInput,TI::FacilityOutput>>(
            infra::KleisliUtils<M>::compose<M::KeyedData<TI::BasicFacilityInput,TI::FacilityOutput>>(
                std::move(extractor), std::move(convertToData)
            )
        );

    auto dataExporter = M::pureExporter<M::Key<TI::OneValue>>(
        [&env](M::Key<TI::OneValue> &&data) {
            std::ostringstream oss;
            TI::OneValue tr = data.key();
            if (tr.data) {
                oss << "Current data: [";
                for (auto const &item : *(tr.data)) {
                    oss << "{name:'" << item.first.name << "'"
                            << ",amount:" << item.second.amount
                            << ",stat:" << item.second.stat
                            << "} ";
                }
                oss << "]";
                oss << " (size: " << tr.data->size() << ")";
                oss << " (version: " << tr.version << ")";
                oss << " (id: " << data.id() << ")";
            } else {
                oss << "Current data was deleted";
            }
            env.log(infra::LogLevel::Info, oss.str());
        }
    );

    auto otherExporter = M::simpleExporter<M::KeyedData<TI::BasicFacilityInput,TI::FacilityOutput>>(
        [](M::InnerData<M::KeyedData<TI::BasicFacilityInput,TI::FacilityOutput>> &&data) {
            auto id = data.timedData.value.key.id();
            auto output = std::move(data.timedData.value.data);
            bool isFinal = data.timedData.finalFlag;
            auto *env = data.environment;

            std::visit([env,isFinal,&id](auto const &o) {
                using T = std::decay_t<decltype(o)>;
                if constexpr (std::is_same_v<TI::TransactionResult,T>) {
                    std::ostringstream oss;
                    std::visit([env,&id,&oss](auto const &tr) {
                        using T1 = std::decay_t<decltype(tr)>;
                        if constexpr (std::is_same_v<TI::TransactionSuccess, T1>) {
                            oss << "Got transaction success for " << env->id_to_string(id);
                        } else if constexpr (std::is_same_v<TI::TransactionFailurePermission, T1>) {
                            oss << "Got transaction failure by permission for " << env->id_to_string(id);
                        } else if constexpr (std::is_same_v<TI::TransactionFailurePrecondition, T1>) {
                            oss << "Got transaction failure by precondition for " << env->id_to_string(id);
                        } else if constexpr (std::is_same_v<TI::TransactionFailureConsistency, T1>) {
                            oss << "Got transaction failure by consistency for " << env->id_to_string(id);
                        } else if constexpr (std::is_same_v<TI::TransactionQueuedAsynchronously, T1>) {
                            oss << "Got transaction queued asynchronously for " << env->id_to_string(id);
                        } else {
                            oss << "Got unknown transaction failure for " << env->id_to_string(id);
                        }
                    }, o);
                    if (isFinal) {
                        oss << " [F]";
                    }
                    env->log(infra::LogLevel::Info, oss.str());
                } else if constexpr (std::is_same_v<TI::SubscriptionAck, T>) {
                    std::ostringstream oss;
                    oss << "Got subscription ack for " << env->id_to_string(id);
                    if (isFinal) {
                        oss << " [F]";
                    }
                    env->log(infra::LogLevel::Info, oss.str());
                } else if constexpr (std::is_same_v<TI::UnsubscriptionAck, T>) {
                    std::ostringstream oss;
                    oss << "Got unsubscription ack for " << env->id_to_string(id);
                    if (isFinal) {
                        oss << " [F]";
                    }
                    env->log(infra::LogLevel::Info, oss.str());
                }
            }, output.value);
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
        [&env,cmd,name,amount,stat,old_version,old_data_count,idStr](basic::VoidStruct &&) -> std::optional<TI::BasicFacilityInput> {
            if (cmd == "subscribe") {
                return TI::BasicFacilityInput {
                    { TI::Subscription {Key{}} }
                };
            } else if (cmd == "unsubscribe") {
                return TI::BasicFacilityInput {
                    { TI::Unsubscription {env.id_from_string(idStr), Key{}} }
                };
            } else if (cmd == "update") {
                db_delta delta;
                delta.inserts_updates.items.push_back(db_item {
                    db_key {name}, db_data {amount, stat}
                });

                return TI::BasicFacilityInput { 
                    { TI::Transaction {
                        TI::UpdateAction {Key{}, old_version, old_data_count, delta}
                    } }
                };
            } else if (cmd == "delete") {
                db_delta delta;
                delta.deletes.keys.push_back(db_key {name});

                return TI::BasicFacilityInput { 
                    { TI::Transaction {
                        TI::UpdateAction {Key{}, old_version, old_data_count, delta}
                    } }
                };
            } else {
                return std::nullopt;
            }
        }
    );

    auto keyify = M::kleisli<TI::BasicFacilityInput>(
        basic::CommonFlowUtilComponents<M>::keyify<TI::BasicFacilityInput>()
    );

    r.placeOrderWithFacility(
        r.execute("keyify", keyify, 
            r.execute("createCommand", createCommand, 
                r.importItem("importer", importer)))
        , facility
        , r.actionAsSink("insertIntoFlow", insertIntoFlow)
    );
    r.exportItem("dataExporter", dataExporter,
        r.execute("extractAndConvert", extractAndConvert, r.actionAsSource(insertIntoFlow))
    );
    r.exportItem("otherExporter", otherExporter, r.actionAsSource(insertIntoFlow));
    
    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "db_one_list_subscription_client");
    r.finalize();

    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "DB one list subscription client started");

    infra::terminationController(infra::RunForever {});
}