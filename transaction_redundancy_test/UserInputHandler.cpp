#include <linenoise.h>
#include <iostream>
#include <fstream>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/IntIDComponent.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/transaction/TransactionClient.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>
#include <tm_kit/basic/AppRunnerUtils.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/redis/RedisComponent.hpp>
#include <tm_kit/transport/redis/RedisOnOrderFacility.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacility.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListener.hpp>
#include <tm_kit/transport/HeartbeatMessageToRemoteFacilityCommand.hpp>

#include "TransactionInterface.hpp"

using namespace dev::cd606::tm;
using namespace test;

int main(int argc, char **argv) {
    std::optional<int> autoRounds = std::nullopt;
    if (argc > 1) {
        autoRounds = std::stoi(argv[1]);
    }
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
        transport::CrossGuidComponent::IDType, DI
    >;

    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent, true, true>,
        transport::CrossGuidComponent,
        transport::ClientSideSimpleIdentityAttacherComponent<
            std::string
            , TI::Transaction>,
        transport::ClientSideSimpleIdentityAttacherComponent<
            std::string
            , GS::Input>,
        transport::redis::RedisComponent
    >;

    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

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

    //We create the facilities
    //We use a very big helper function,
    //the meanings of the parameters are commented one by one

    //The returned value is a 2-tuple, each tuple element is a variadic-tuple
    //, with one element in such variadic-tuple corresponding to a facility.

    auto facilities =
        transport::MultiTransportRemoteFacilityManagingUtils<R>
        ::SetupRemoteFacilities<
            std::tuple<
                std::tuple<std::string, GS::Input, GS::Output>
            >
            , std::tuple<
                std::tuple<std::string, TI::Transaction, TI::TransactionResponse>
            >
        >::run(
            //The runner
            r 
            //The protocol/address/topic to listen for heartbeat
            , transport::MultiTransportBroadcastListenerAddSubscription {
                transport::MultiTransportBroadcastListenerConnectionType::Redis
                , transport::ConnectionLocator::parse("127.0.0.1:6379")
                , "heartbeats.transaction_test_server"
            }
            //The server identifier in heartbeat
            , std::regex("transaction redundancy test server")
            //The names of the facilities in the server-side graph
            //(in case of doubt, look at the generated server-side graph)
            //The "distinguished" ones must come first and the "non-distinguished"
            //ones come afterwards. In the "distinguished" ones, the order 
            //is the actual order we will issue registration, so if one of them
            //depends on another, the dependent must come later. All the
            //"non-distinguished" ones are registered together in the end.
            //"Distinguished" ones are the ones that client wants to interact
            //severally, for example, here for subscription, the client needs to
            //issue one subscription per server instance, therefore they are 
            //"distinguished". The "non-distinguished" ones are fungible, the client
            //randomly interacts with any of them and does not need to know
            //which is which
            , {
                "transaction_server_components/subscription_handler"
                , "transaction_server_components/transaction_handler"
            }
            //The TTL of heartbeat. If heartbeat is missing for this amount of time
            //, we need to de-register the facilities
            , std::chrono::seconds(3)
            //How often do we check for TTL liveliness
            , std::chrono::seconds(5)
            //For the "distinguished" ones, we always send one initial request 
            //right after registration, and this parameter provides the initial
            //request objects for the "distinguished" ones, preserving the order.
            //Note that we do not actually provide the initial request objects, but
            //provide producers for such objects. This allows for more flexibility.
            , {
                []() -> GS::Input {
                    return GS::Input { GS::Subscription {
                    { Key {} }
                    } };
                }
            }
            //For the "distinguished" ones, we also always wait for the initial
            //reply to the initial request to come back, before moving on to 
            //register the next "distinguished" one (or the "non-distinguished"
            //ones), here we provide the checkers (always given as predicates
            //taking in const references) for each "distinguished" facility, 
            //preserving the order
            , {
                [](GS::Input const &, GS::Output const &o) -> bool {
                    return std::visit(
                        [](auto const &x) -> bool {
                            auto ret = std::is_same_v<std::decay_t<decltype(x)>, GS::Subscription>;
                            return ret;
                        }, o.value
                    );
                }
            }
            //These are the client-side graph node names for all the facilities
            //, keeping the same order as the server-side names
            , {
                "subscription", "transaction"
            }
            //This is the prefix to use for all the components created in this
            //helper function during graph generation
            , "facilities"
        );

    auto subscriptionFacility = std::get<0>(std::get<0>(facilities));
    auto transactionFacility = std::get<0>(std::get<1>(facilities));

    //Now we manage the subscription data

    using SubscriptionOutput = M::KeyedData<std::tuple<transport::ConnectionLocator,GS::Input>,GS::Output>;
    using FullSubscriptionData = M::KeyedData<std::tuple<transport::ConnectionLocator,GS::Input>,DI::FullUpdate>;

    std::shared_ptr<basic::transaction::current::TransactionDataStore<DI>> dataStorePtr;
    auto subscriptionOutputs = basic::transaction::v2::basicDataStreamClientCombination<
        R, DI, std::tuple<transport::ConnectionLocator, GS::Input>
        , ApplyVersionSlice
        , ApplyDataSlice
    >(
        r 
        , "subscripionOutputHandling"
        , subscriptionFacility.feedOrderResults
        , &dataStorePtr
    );
    if (!autoRounds) {
        //The reason we need this extra step is that
        //FullSubscriptionData, being a KeyedData, will have
        //separate versions for different ConnectionLocators,
        //when we extract only the FullUpdate part, then they will
        //share a version stream, so we won't print duplicates
        auto extractDataForPrint = M::liftPure<FullSubscriptionData>(
            [](FullSubscriptionData &&theUpdate) -> DI::FullUpdate {
                return std::move(theUpdate.data);
            }
        );
        auto fullDataPrinter = M::pureExporter<DI::FullUpdate>(
            [&env](DI::FullUpdate &&theUpdate) {
                env.log(infra::LogLevel::Info, "Got full data update, will print");
                std::ostringstream oss;
                oss << "Got full data update {";
                oss << "globalVersion=" << theUpdate.version;
                oss << ",dataItems=[";
                int ii = 0;
                for (auto const &item : theUpdate.data) {
                    if (ii > 0) {
                        oss << ',';
                    }
                    oss << "{version=" << item.version;
                    oss << ",data=";
                    if (item.data) {
                        oss << *(item.data);
                    } else {
                        oss << "(deleted)";
                    }
                    oss << "}";
                    ++ii;
                }
                oss << "]}";
                env.log(infra::LogLevel::Info, oss.str());
            }
        );

        r.exportItem("fullDataPrinter", fullDataPrinter
            , r.execute("extractDataForPrint", extractDataForPrint
                , std::move(subscriptionOutputs))
        );
    }

    //Next, we manage the subscription IDs

    std::unordered_map<transport::ConnectionLocator, TheEnvironment::IDType> ids;
    std::mutex idMutex;

    auto subscriptionIDManager = M::pureExporter<SubscriptionOutput>(
        [&env,&ids,&idMutex](SubscriptionOutput &&o) {
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
                            env.exit();
                        }
                    } 
                }
            }, std::move(o.data.value));
        }
    );
    auto subscriptionIDRemover = M::pureExporter<std::tuple<transport::ConnectionLocator, bool>>(
        [&env,&ids,&idMutex](std::tuple<transport::ConnectionLocator, bool> &&x) {
            if (!std::get<1>(x)) {
                std::lock_guard<std::mutex> _(idMutex);
                ids.erase(std::get<0>(x));
                std::ostringstream oss;
                oss << "Subscription ID for " << std::get<0>(x).toSerializationFormat() << " removed because the server is disconnected";
                env.log(infra::LogLevel::Info, oss.str());
            }
        }
    );

    subscriptionFacility.feedOrderResults(
        r 
        , r.exporterAsSink("subscriptionIDManager", subscriptionIDManager)
    );
    subscriptionFacility.feedConnectionChanges(
        r 
        , r.exporterAsSink("subscriptionIDRemover", subscriptionIDRemover)
    );

    //Next, we set up the main command parser, which will, among others
    //, generate an "exit" command

    R::Sourceoid<std::vector<std::string>> cmdSource;
    if (autoRounds) {
        auto cmdTimer = 
            basic::real_time_clock::ClockImporter<TheEnvironment>
                ::createOneShotClockConstImporter<basic::VoidStruct>(
                    env.now()+std::chrono::seconds(5)
                    , basic::VoidStruct {}
                ); 
        auto cmdCreator = M::liftMulti<basic::VoidStruct>(
            [&autoRounds](basic::VoidStruct &&) -> std::vector<std::vector<std::string>> {
                std::vector<std::vector<std::string>> ret;
                for (int counter=0; counter < *autoRounds; ++counter) {
                    ret.push_back({"transfer", "A", "B", "100"});
                    ret.push_back({"transfer", "B", "A", "100"});
                    ret.push_back({"process"});
                    ret.push_back({"inject", "C", "100"});
                    ret.push_back({"close", "C"});
                }
                return ret;
            }
        );
        r.registerImporter("cmdTimer", cmdTimer);
        r.registerAction("cmdCreator", cmdCreator);
        r.execute(cmdCreator, r.importItem(cmdTimer));
        auto extractDataForSync = M::liftPure<FullSubscriptionData>(
            [](FullSubscriptionData &&theUpdate) -> DI::FullUpdate {
                return std::move(theUpdate.data);
            }
        );
        auto sync = basic::CommonFlowUtilComponents<M>::synchronizer2<
            std::vector<std::string>
            , DI::FullUpdate
        >(
            [](std::vector<std::string> &&v, DI::FullUpdate &&) -> std::vector<std::string> {
                return std::move(v);
            }
        );
        r.execute("cmdAndDataSync", sync
            , r.actionAsSource(cmdCreator)
            , r.execute("extractDataForSync", extractDataForSync, std::move(subscriptionOutputs)));
        cmdSource = r.sourceAsSourceoid(r.actionAsSource(sync));
    } else {
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
        r.registerImporter("lineImporter", lineImporter);
        cmdSource = r.sourceAsSourceoid(r.importItem(lineImporter));
    }
    
    //We add the exit handler here to unsubscribe from all servers

    auto exitCommandHandler = M::liftMulti<std::vector<std::string>>(
        [&env,&ids,&idMutex](std::vector<std::string> const &parts) 
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
                if (ret.empty()) {
                    env.log(infra::LogLevel::Info, "Nothing to unsubscribe, exiting");
                    env.exit();
                }
                return ret;
            } else {
                return {};
            }
        }
    );

    r.registerAction("exitCommandHandler", exitCommandHandler);
    cmdSource(r, r.actionAsSink(exitCommandHandler));

    subscriptionFacility.orderReceiver(
        r
        , r.actionAsSource(exitCommandHandler)
    );

    //Now we can handle the transaction part.
    
    auto transactionCommandParser = M::liftMaybe<std::vector<std::string>>(
        [&env,dataStorePtr,&autoRounds](std::vector<std::string> const &parts) -> std::optional<TI::Transaction> {
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
                    /*
                    if (autoRounds) {
                        return TI::Transaction { TI::UpdateAction {
                            Key {}
                            , std::nullopt
                            , std::nullopt
                            , InjectData {
                                parts[1]
                                , amount
                            }
                        } };
                    }*/
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
                    /*
                    if (autoRounds) {
                        return TI::Transaction { TI::UpdateAction {
                            Key {}
                            , std::nullopt
                            , std::nullopt
                            , TransferData {
                                parts[1]
                                , parts[2]
                                , amount
                            }
                        } };
                    }*/
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
                if (autoRounds) {
                    return TI::Transaction { TI::UpdateAction {
                        Key {}
                        , std::nullopt
                        , std::nullopt
                        , CloseAccount { parts[1] }
                    } };
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
    std::chrono::steady_clock::time_point t1;
    bool first = true;
    auto transactionResponsePrinter = M::pureExporter<TI::TransactionResponse>(
        [&env,&autoRounds,&t1,&first](TI::TransactionResponse &&r) {
            static int success = 0, fail = 0;
            if (autoRounds) {
                if (first) {
                    t1 = std::chrono::steady_clock::now();
                    first = false;
                }
                if (r.value.requestDecision == basic::transaction::v2::RequestDecision::Success) {
                    ++success;
                } else {
                    ++fail;
                }
                /*
                std::ostringstream oss;
                oss << "Success " << success << ", fail " << fail;
                env.log(infra::LogLevel::Info, oss.str());
                */
                if (success+fail == 5*(*autoRounds)) {
                    std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
                    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(t2-t1).count();
                    std::ostringstream oss;
                    oss << "Success " << success << ", fail " << fail << ", span " << micros << " micros";
                    env.log(infra::LogLevel::Info, oss.str());
                }
            } else {
                std::ostringstream oss;
                oss << "Got transaction response {";
                oss << "globalVersion=" << r.value.globalVersion;
                oss << ",requestDecision=" << r.value.requestDecision;
                oss << "}";
                env.log(infra::LogLevel::Info, oss.str());
            }
        }
    );

    r.registerAction("transactionCommandParser", transactionCommandParser);
    cmdSource(r, r.actionAsSink(transactionCommandParser));
    transactionFacility(
        r
        , r.execute(
            "keyifyTI", keyifyTI
            , r.actionAsSource(transactionCommandParser)
        )
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

    infra::terminationController(infra::RunForever {&env});

    return 0;
}