#include <linenoise.h>
#include <iostream>
#include <fstream>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeMonad.hpp>
#include <tm_kit/infra/IntIDComponent.hpp>

#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/transaction/TransactionClient.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>
#include <tm_kit/basic/MonadRunnerUtils.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/redis/RedisComponent.hpp>
#include <tm_kit/transport/redis/RedisOnOrderFacility.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacility.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListener.hpp>
#include <tm_kit/transport/HeartbeatMessageToRemoteFacilityCommand.hpp>

#include "TransactionInterface.hpp"

using namespace dev::cd606::tm;
using namespace test;

int main(int argc, char **argv) {
    using DI = basic::transaction::current::DataStreamInterface<
        int64_t
        , Key
        , Version
        , Data
        , VersionSlice
        , DataSlice
        , std::less<int64_t>
        , CompareVersion
    >;

    using TI = basic::transaction::current::TransactionInterface<
        int64_t
        , Key
        , Version
        , Data
        , DataSummary
        , VersionSlice
        , Command
        , DataSlice
    >;

    using GS = basic::transaction::current::GeneralSubscriberTypes<
        boost::uuids::uuid, DI
    >;

    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::real_time_clock::ClockComponent>,
        transport::BoostUUIDComponent,
        transport::ClientSideSimpleIdentityAttacherComponent<
            std::string
            , TI::Transaction>,
        transport::ClientSideSimpleIdentityAttacherComponent<
            std::string
            , GS::Input>,
        transport::redis::RedisComponent
    >;

    using M = infra::RealTimeMonad<TheEnvironment>;
    using R = infra::MonadRunner<M>;

    TheEnvironment env;
    env.transport::ClientSideSimpleIdentityAttacherComponent<std::string,TI::Transaction>::operator=(
        transport::ClientSideSimpleIdentityAttacherComponent<std::string,TI::Transaction>(
            "transaction_redundancy_test_client"
        )
    );
    env.transport::ClientSideSimpleIdentityAttacherComponent<std::string,GS::Input>::operator=(
        transport::ClientSideSimpleIdentityAttacherComponent<std::string,GS::Input>(
            "transaction_redundancy_test_client"
        )
    );

    R r(&env);

    //We start by listening to the heartbeat from servers

    auto createHeartbeatListenKey = M::constFirstPushKeyImporter(
        transport::MultiTransportBroadcastListenerInput { {
            transport::MultiTransportBroadcastListenerAddSubscription {
                transport::MultiTransportBroadcastListenerConnectionType::Redis
                , transport::ConnectionLocator::parse("127.0.0.1:6379")
                , "heartbeats.transaction_test_server"
            }
        } }
    );
    auto heartbeat = basic::MonadRunnerUtilComponents<R>::importWithTrigger(
        r
        , r.importItem("createHeartbeatListenKey", createHeartbeatListenKey)
        , M::onOrderFacilityWithExternalEffects(
            new transport::MultiTransportBroadcastListener<TheEnvironment, transport::HeartbeatMessage>()
        )
        , std::nullopt //the trigger response is not needed
        , "heartbeatListener"
    );

    //Now that we have the heartbeats, we process them into 
    //management commands for subscription facility

    auto discardTopicFromHeartbeat = M::liftPure<basic::TypedDataWithTopic<transport::HeartbeatMessage>>(
        [](basic::TypedDataWithTopic<transport::HeartbeatMessage> &&d) {
            return std::move(d.content);
        }
    );

    auto createSubscriptionFacilityCommand = transport::heartbeatMessageToRemoteFacilityCommand<M>(
        std::regex("transaction redundancy test server")
        , std::regex("subscription_queue")
        , std::chrono::seconds(3)
    );

    auto facilityCommandTimer = basic::real_time_clock::ClockImporter<TheEnvironment>
        ::createRecurringClockImporter<basic::VoidStruct>(
            std::chrono::system_clock::now()
            , std::chrono::system_clock::now()+std::chrono::hours(24)
            , std::chrono::seconds(5)
            , [](typename TheEnvironment::TimePointType const &tp) {
                return basic::VoidStruct {};
            }
        );

    auto subscriptionFacility = M::vieOnOrderFacility(
        new transport::MultiTransportRemoteFacility<
            TheEnvironment
            , GS::Input
            , GS::Output
            , std::string
            , transport::MultiTransportRemoteFacilityDispatchStrategy::Designated
        >
    );

    auto subscriptionFacilityCmd = r.execute(
        "createSubscriptionFacilityCommand", createSubscriptionFacilityCommand
        , r.execute(
            "discardTopicFromHeartbeat", discardTopicFromHeartbeat
            , std::move(heartbeat)
        )
    );
    auto facilityCommandTimerInput = r.importItem("facilityCommandTimer", facilityCommandTimer);
    r.connect(
        facilityCommandTimerInput.clone()
        , r.actionAsSink_2_1(createSubscriptionFacilityCommand)
    ); //"2_1" means using the second ("1", since we count from 0) input sink of an action with "2" inputs

    //Now the subscription facility is there, we can subscribe
    //Please note that we MUST issue a subscribe command for each
    //new server added into the subscription facility

    using FullSubscriptionOutput = M::KeyedData<std::tuple<transport::ConnectionLocator,GS::Input>,GS::Output>;

    auto vieInputInject = [](transport::MultiTransportRemoteFacilityAction &&action) {
        return std::move(action);
    };
    auto vieSelfLoopCreator = infra::KleisliUtils<M>::liftMaybe<transport::MultiTransportRemoteFacilityActionResult>(
        [](transport::MultiTransportRemoteFacilityActionResult &&res)
            -> std::optional<M::Key<std::tuple<transport::ConnectionLocator,GS::Input>>> {
            if (std::get<0>(res).actionType != transport::MultiTransportRemoteFacilityActionType::Register) {
                return std::nullopt;
            }
            return infra::withtime_utils::keyify<
                std::tuple<transport::ConnectionLocator,GS::Input>
                , TheEnvironment
            >(std::tuple<transport::ConnectionLocator,GS::Input> {
                std::get<0>(res).connectionLocator 
                , GS::Input { GS::Subscription {
                    { Key {} }
                } }
            });
        }
    );
    auto synchronizer = [](transport::MultiTransportRemoteFacilityActionResult &&res, transport::MultiTransportRemoteFacilityAction &&) -> transport::MultiTransportRemoteFacilityActionResult {
        return std::move(res);
    };

    auto subscriptionFacilityLoopOutput = basic::MonadRunnerUtilComponents<R>::setupVIEFacilitySelfLoopAndWait(
        r
        , std::move(subscriptionFacilityCmd)
        , std::move(vieInputInject)
        , std::move(vieSelfLoopCreator)
        , subscriptionFacility
        , std::move(synchronizer)
        , "subscriptionFacilityLoop"
        , "subscriptionFacility"
    );

    //Now we can print the subscription data

    auto dataStorePtr = std::make_shared<basic::transaction::current::TransactionDataStore<DI>>();
    r.preservePointer(dataStorePtr);

    using DM = basic::transaction::v2::TransactionDeltaMerger<
        DI, true, M::PossiblyMultiThreaded
        , ApplyVersionSlice
        , ApplyDataSlice
    >;
    //getFullData might be as well written as three nodes in the graph
    //but by operating in KleisliUtils, we can merge them into one node
    auto getFullData = M::kleisli<FullSubscriptionOutput>(
        infra::KleisliUtils<M>::compose<FullSubscriptionOutput>(
            infra::KleisliUtils<M>::liftPure<FullSubscriptionOutput>(
                [](FullSubscriptionOutput &&data) -> GS::Output {
                    return std::move(data.data);
                }
            )
            , infra::KleisliUtils<M>::compose<GS::Output>(
                basic::CommonFlowUtilComponents<M>::pickOneFromWrappedValue<GS::Output,DI::Update>()
                , infra::KleisliUtils<M>::liftPure<DI::Update>(DM {dataStorePtr})
            )
        )
    );
    auto fullDataPrinter = M::pureExporter<DI::Update>(
        [&env](DI::Update &&theUpdate) {
            std::ostringstream oss;
            oss << "Got full data update {";
            oss << "globalVersion=" << theUpdate.version;
            oss << ",dataItems=[";
            int ii = 0;
            for (auto const &item : theUpdate.data) {
                std::visit([&oss,&ii](auto const &x) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(x)>, DI::OneFullUpdateItem>) {
                        if (ii > 0) {
                            oss << ',';
                        }
                        oss << "{version=" << x.version;
                        oss << ",data=";
                        if (x.data) {
                            oss << *(x.data);
                        } else {
                            oss << "(deleted)";
                        }
                        oss << "}";
                        ++ii;
                    }
                }, item);
            }
            oss << "]}";
            env.log(infra::LogLevel::Info, oss.str());
        }
    );

    r.exportItem("fullDataPrinter", fullDataPrinter
        , r.execute("getFullData", getFullData, subscriptionFacilityLoopOutput.facilityOutput.clone()));

    //Next, we manage the subscription IDs

    std::unordered_map<transport::ConnectionLocator, TheEnvironment::IDType> ids;
    std::mutex idMutex;

    auto subscriptionIDManager = M::pureExporter<FullSubscriptionOutput>(
        [&env,&ids,&idMutex](FullSubscriptionOutput &&o) {
            std::lock_guard<std::mutex> _(idMutex);
            auto id = o.key.id();
            auto locator = std::get<0>(o.key.key());
            std::visit([&env,&ids,&id,&locator](auto &&x) {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T, GS::Subscription>) {
                    ids[locator] = id;
                    std::ostringstream oss;
                    oss << "Subscription ID for " << locator.toSerializationFormat() << " is " << id;
                    env.log(infra::LogLevel::Info, oss.str());
                } else if constexpr (std::is_same_v<T, GS::Unsubscription>) {
                    auto iter = ids.find(locator);
                    if (iter != ids.end() && iter->second == x.originalSubscriptionID) {
                        ids.erase(iter);
                        std::ostringstream oss;
                        oss << "Subscription ID for " << locator.toSerializationFormat() << " removed because it has been unsubscribed";
                        env.log(infra::LogLevel::Info, oss.str());
                        if (ids.empty()) {
                            env.log(infra::LogLevel::Info, "All unsubscribed, exiting");
                            exit(0);
                        }
                    } 
                }
            }, std::move(o.data.value));
        }
    );
    auto subscriptionIDRemover = M::pureExporter<transport::MultiTransportRemoteFacilityActionResult>(
        [&env,&ids,&idMutex](transport::MultiTransportRemoteFacilityActionResult &&res) {
            if (std::get<0>(res).actionType == transport::MultiTransportRemoteFacilityActionType::Deregister) {
                std::lock_guard<std::mutex> _(idMutex);
                ids.erase(std::get<0>(res).connectionLocator);
                std::ostringstream oss;
                oss << "Subscription ID for " << std::get<0>(res).connectionLocator.toSerializationFormat() << " removed because the server is disconnected";
                env.log(infra::LogLevel::Info, oss.str());
            }
        }
    );

    r.exportItem("subscriptionIDManager", subscriptionIDManager
        , subscriptionFacilityLoopOutput.facilityOutput.clone());
    r.exportItem("subscriptionIDRemover", subscriptionIDRemover
        , subscriptionFacilityLoopOutput.processedFacilityExtraOutput.clone());

    //Next, we set up the main command parser, which will, among others
    //, generate an "exit" command

    auto lineImporter = M::simpleImporter<std::vector<std::string>>(
        [](M::PublisherCall<std::vector<std::string>> &pub) {
            linenoiseHistorySetMaxLen(100);
            char *p;
            while ((p = linenoise(">>")) != nullptr) {
                if (p[0] != '\0') {
                    linenoiseHistoryAdd(p);
                }
                std::string s = boost::trim_copy(std::string {p});
                std::vector<std::string> parts;
                boost::split(parts, s, boost::is_any_of(" \t"));
                pub(std::move(parts));
            }
            pub(std::vector<std::string> {"exit"});
        }
        , infra::LiftParameters<std::chrono::system_clock::time_point>()
            .SuggestThreaded(true)
    );

    //We add the exit handler here to unsubscribe from all servers

    auto exitCommandHandler = M::liftMulti<std::vector<std::string>>(
        [&ids,&idMutex](std::vector<std::string> const &parts) 
            -> std::vector<M::Key<std::tuple<transport::ConnectionLocator, GS::Input>>> {
            if (!parts.empty() && parts[0] == "exit") {
                std::lock_guard<std::mutex> _(idMutex);
                std::vector<M::Key<std::tuple<transport::ConnectionLocator, GS::Input>>> ret;
                for (auto const &item : ids) {
                    ret.push_back(
                        infra::withtime_utils::keyify<
                            std::tuple<transport::ConnectionLocator, GS::Input>
                            , TheEnvironment
                        >(
                            std::tuple<transport::ConnectionLocator, GS::Input> {
                                item.first
                                , GS::Input {
                                    GS::Unsubscription {
                                        item.second
                                    }
                                }
                            }
                        )
                    );
                }
                return ret;
            } else {
                return {};
            }
        }
    );

    subscriptionFacilityLoopOutput.callIntoFacility(
        r
        , r.execute(
            "exitCommandHandler", exitCommandHandler
            , r.importItem("lineImporter", lineImporter)
        )
    );

    //Now we can handle the transaction part.
    //Transaction part is managed somewhat similarly to the subscription
    //part, but since we only have to send a transaction command to one
    //server, it is simpler. The command parsing takes up most of the space
    //in this part

    auto createTransactionFacilityCommand = transport::heartbeatMessageToRemoteFacilityCommand<M>(
        std::regex("transaction redundancy test server")
        , std::regex("transaction_queue")
        , std::chrono::seconds(3)
    );

    auto transactionFacility = M::localOnOrderFacility(
        new transport::MultiTransportRemoteFacility<
            TheEnvironment
            , TI::Transaction
            , TI::TransactionResponse
            , std::string
            , transport::MultiTransportRemoteFacilityDispatchStrategy::Random
        >
    );

    auto transactionFacilityCmd = r.execute(
        "createTransactionFacilityCommand", createTransactionFacilityCommand
        , r.actionAsSource(discardTopicFromHeartbeat)
    );
    r.connect(
        facilityCommandTimerInput.clone()
        , r.actionAsSink_2_1(createTransactionFacilityCommand)
    ); 
    r.connect(std::move(transactionFacilityCmd), r.localFacilityAsSink("transactionFacility", transactionFacility));

    auto transactionCommandParser = M::liftMaybe<std::vector<std::string>>(
        [&env,dataStorePtr](std::vector<std::string> const &parts) -> std::optional<TI::Transaction> {
            if (parts.empty()) {
                return std::nullopt;
            }
            if (parts[0] == "inject") {
                if (parts.size() != 3) {
                    env.log(infra::LogLevel::Info, "Inject command usage: inject account amount");
                    return std::nullopt;
                }
                try {
                    uint32_t amount = boost::lexical_cast<uint32_t>(parts[2]);
                    basic::transaction::current::TransactionDataStore<DI>::Lock _(dataStorePtr->mutex_);
                    auto currentData = dataStorePtr->dataMap_[Key {}];
                    if (!currentData.data) {
                        env.log(infra::LogLevel::Info, "Please wait until we have data before sending inject command");
                        return std::nullopt;
                    }
                    auto versionIter = currentData.version.accounts.find(parts[1]);
                    std::map<std::string, std::optional<int64_t>> currentAccountVer;
                    if (versionIter != currentData.version.accounts.end()) {
                        currentAccountVer[parts[1]] = versionIter->second;
                    }
                    auto dataIter = currentData.data->accounts.find(parts[1]);
                    std::map<std::string, std::optional<AccountData>> currentAccount;
                    if (dataIter != currentData.data->accounts.end()) {
                        currentAccount[parts[1]] = dataIter->second;
                    }
                    return TI::Transaction { TI::UpdateAction {
                        Key {}
                        , VersionSlice {
                            currentData.version.overallStat
                            , currentAccountVer
                            , std::nullopt
                        }
                        , DataSlice {
                            currentData.data->overallStat
                            , currentAccount
                            , std::nullopt
                        }
                        , InjectData {
                            parts[1]
                            , amount
                        }
                    } };
                } catch (boost::bad_lexical_cast const &) {
                    env.log(infra::LogLevel::Info, "Inject command usage: inject account amount");
                    return std::nullopt;
                }
            } else if (parts[0] == "transfer") {
                if (parts.size() != 4) {
                    env.log(infra::LogLevel::Info, "Transfer command usage: transfer from to amount");
                    return std::nullopt;
                }
                try {
                    uint32_t amount = boost::lexical_cast<uint32_t>(parts[3]);
                    basic::transaction::current::TransactionDataStore<DI>::Lock _(dataStorePtr->mutex_);
                    auto currentData = dataStorePtr->dataMap_[Key {}];
                    if (!currentData.data) {
                        env.log(infra::LogLevel::Info, "Please wait until we have data before sending transfer command");
                        return std::nullopt;
                    }
                    auto versionIter = currentData.version.accounts.find(parts[1]);
                    std::map<std::string, std::optional<int64_t>> currentAccountVer;
                    if (versionIter == currentData.version.accounts.end()) {
                        env.log(infra::LogLevel::Info, "No version info for account '"+parts[1]+"'");
                        return std::nullopt;
                    }
                    currentAccountVer[parts[1]] = versionIter->second;
                    versionIter = currentData.version.accounts.find(parts[2]);
                    if (versionIter == currentData.version.accounts.end()) {
                        env.log(infra::LogLevel::Info, "No version info for account '"+parts[2]+"'");
                        return std::nullopt;
                    }
                    currentAccountVer[parts[2]] = versionIter->second;
                    auto dataIter = currentData.data->accounts.find(parts[1]);
                    std::map<std::string, std::optional<AccountData>> currentAccount;
                    if (dataIter == currentData.data->accounts.end()) {
                        env.log(infra::LogLevel::Info, "No data for account '"+parts[1]+"'");
                        return std::nullopt;
                    }
                    currentAccount[parts[1]] = dataIter->second;
                    dataIter = currentData.data->accounts.find(parts[2]);
                    if (dataIter == currentData.data->accounts.end()) {
                        env.log(infra::LogLevel::Info, "No data for account '"+parts[2]+"'");
                        return std::nullopt;
                    }
                    currentAccount[parts[2]] = dataIter->second;
                    return TI::Transaction { TI::UpdateAction {
                        Key {}
                        , VersionSlice {
                            std::nullopt
                            , currentAccountVer
                            , currentData.version.pendingTransfers
                        }
                        , DataSlice {
                            std::nullopt
                            , currentAccount
                            , currentData.data->pendingTransfers
                        }
                        , TransferData {
                            parts[1]
                            , parts[2]
                            , amount
                        }
                    } };
                } catch (boost::bad_lexical_cast const &) {
                    env.log(infra::LogLevel::Info, "Transfer command usage: transfer from to amount");
                    return std::nullopt;
                }
            } else if (parts[0] == "process") {
                if (parts.size() != 1) {
                    env.log(infra::LogLevel::Info, "Process command usage: process");
                    return std::nullopt;
                }
                //we deliberately don't have version/summary check for process
                return TI::Transaction { TI::UpdateAction {
                    Key {}
                    , std::nullopt
                    , std::nullopt
                    , ProcessPendingTransfers {}
                } };
            } else if (parts[0] == "close") {
                if (parts.size() != 2) {
                    env.log(infra::LogLevel::Info, "Close command usage: close acocunt");
                    return std::nullopt;
                }
                basic::transaction::current::TransactionDataStore<DI>::Lock _(dataStorePtr->mutex_);
                auto currentData = dataStorePtr->dataMap_[Key {}];
                if (!currentData.data) {
                    env.log(infra::LogLevel::Info, "Please wait until we have data before sending close command");
                    return std::nullopt;
                }
                auto versionIter = currentData.version.accounts.find(parts[1]);
                std::map<std::string, std::optional<int64_t>> currentAccountVer;
                if (versionIter == currentData.version.accounts.end()) {
                    env.log(infra::LogLevel::Info, "No version info for account '"+parts[1]+"'");
                    return std::nullopt;
                }
                currentAccountVer[parts[1]] = versionIter->second;
                auto dataIter = currentData.data->accounts.find(parts[1]);
                std::map<std::string, std::optional<AccountData>> currentAccount;
                if (dataIter == currentData.data->accounts.end()) {
                    env.log(infra::LogLevel::Info, "No data for account '"+parts[1]+"'");
                    return std::nullopt;
                }
                currentAccount[parts[1]] = dataIter->second;
                return TI::Transaction { TI::UpdateAction {
                    Key {}
                    , VersionSlice {
                        currentData.version.overallStat
                        , currentAccountVer
                        , std::nullopt
                    }
                    , DataSlice {
                        currentData.data->overallStat
                        , currentAccount
                        , std::nullopt
                    }
                    , CloseAccount { parts[1] }
                } };
            } else {
                return std::nullopt;
            }
        }
    );

    auto keyifyTI = M::kleisli<TI::Transaction>(
        basic::CommonFlowUtilComponents<M>::keyify<TI::Transaction>()
    );
    auto tiReceiver = M::kleisli<M::KeyedData<TI::Transaction,TI::TransactionResponse>>(
        basic::CommonFlowUtilComponents<M>::extractDataFromKeyedData<TI::Transaction,TI::TransactionResponse>()
    );
    auto transactionResponsePrinter = M::pureExporter<TI::TransactionResponse>(
        [&env](TI::TransactionResponse &&r) {
            std::ostringstream oss;
            oss << "Got transaction response {";
            oss << "globalVersion=" << r.value.globalVersion;
            oss << ",requestDecision=" << r.value.requestDecision;
            oss << "}";
            env.log(infra::LogLevel::Info, oss.str());
        }
    );

    r.placeOrderWithLocalFacility(
        r.execute(
            "keyifyTI", keyifyTI
            , r.execute(
                "transactionCommandParser", transactionCommandParser
                , r.importItem(lineImporter)
            )
        )
        , transactionFacility
        , r.actionAsSink("tiReceiver", tiReceiver)
    );
    r.exportItem(
        "transactionResponsePrinter", transactionResponsePrinter
        , r.actionAsSource(tiReceiver)
    );

    //We are done, now print the graph and run

    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "transaction_redundancy_test_client");

    if (argc > 1) {
        std::ofstream ofs(argv[1]);
        r.writeGraphVizDescription(ofs, "transaction_redundancy_test_client");
    }
    
    r.finalize();

    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "Transaction redundancy test client started");

    infra::terminationController(infra::RunForever {});

    return 0;
}