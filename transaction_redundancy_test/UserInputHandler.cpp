#include <linenoise.h>
#include <iostream>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeMonad.hpp>
#include <tm_kit/infra/IntIDComponent.hpp>

#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/transaction/TransactionClient.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/redis/RedisComponent.hpp>
#include <tm_kit/transport/redis/RedisOnOrderFacility.hpp>

#include "TransactionInterface.hpp"

using namespace dev::cd606::tm;
using namespace test;

int main() {
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

    auto dataStorePtr = std::make_shared<basic::transaction::current::TransactionDataStore<DI>>();
    r.preservePointer(dataStorePtr);

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

    std::optional<TheEnvironment::IDType> subscriptionID;

    auto subscriptionCommandParser = M::liftMaybe<std::vector<std::string>>(
        [&env,dataStorePtr,&subscriptionID](std::vector<std::string> const &parts) -> std::optional<GS::Input> {
            if (parts.empty()) {
                return std::nullopt;
            }
            if (parts[0] == "subscribe") {
                if (parts.size() != 1) {
                    env.log(infra::LogLevel::Info, "Subscribe command usage: subscribe");
                    return std::nullopt;
                }
                return GS::Input { GS::Subscription {
                    { Key {} }
                } };
            } else if (parts[0] == "exit") {
                if (subscriptionID) {
                    return GS::Input { GS::Unsubscription {
                        *subscriptionID
                    } };
                } else {
                    env.log(infra::LogLevel::Info, "Nothing to unsubscribe, exiting");
                    exit(0);
                }
            } else {
                return std::nullopt;
            }
        }
    );
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

    auto keyifyGS = M::kleisli<GS::Input>(
        basic::CommonFlowUtilComponents<M>::keyify<GS::Input>()
    );
    auto keyifyTI = M::kleisli<TI::Transaction>(
        basic::CommonFlowUtilComponents<M>::keyify<TI::Transaction>()
    );

    auto gsFacility = transport::redis::RedisOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::createTypedRPCOnOrderFacility
        <GS::Input,GS::Output>(
        transport::ConnectionLocator::parse("127.0.0.1:6379:::test_etcd_subscription_queue")
    );
    auto tiFacility = transport::redis::RedisOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::createTypedRPCOnOrderFacility
        <TI::Transaction,TI::TransactionResponse>(
        transport::ConnectionLocator::parse("127.0.0.1:6379:::test_etcd_transaction_queue")
    );

    auto gsReceiver = M::kleisli<M::KeyedData<GS::Input,GS::Output>>(
        basic::CommonFlowUtilComponents<M>::extractIDAndDataFromKeyedData<GS::Input,GS::Output>()
    );
    auto tiReceiver = M::kleisli<M::KeyedData<TI::Transaction,TI::TransactionResponse>>(
        basic::CommonFlowUtilComponents<M>::extractIDAndDataFromKeyedData<TI::Transaction,TI::TransactionResponse>()
    );

    using DM = basic::transaction::v2::TransactionDeltaMerger<
        DI, true, M::PossiblyMultiThreaded
        , ApplyVersionSlice
        , ApplyDataSlice
    >;
    auto getFullData = M::kleisli<M::Key<GS::Output>>(
        basic::CommonFlowUtilComponents<M>::withKey<GS::Output>(
            infra::KleisliUtils<M>::compose<GS::Output>(
                basic::CommonFlowUtilComponents<M>::pickOneFromWrappedValue<GS::Output,DI::Update>()
                , infra::KleisliUtils<M>::liftPure<DI::Update>(DM {dataStorePtr})
            )
        )
    );
    auto fullDataPrinter = M::pureExporter<M::Key<DI::Update>>(
        [&env](M::Key<DI::Update> &&fullData) {
            auto theUpdate = std::move(fullData.key());
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

    auto subscriptionIDManager = M::pureExporter<M::Key<GS::Output>>(
        [&env,&subscriptionID](M::Key<GS::Output> &&o) {
            auto id = o.id();
            std::visit([&env,&subscriptionID,&id](auto &&x) {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T, GS::Subscription>) {
                    subscriptionID = id;
                } else if constexpr (std::is_same_v<T, GS::Unsubscription>) {
                    if (subscriptionID && (*subscriptionID == x.originalSubscriptionID)) {
                        env.log(infra::LogLevel::Info, "Got unsubscription callback, exiting");
                        exit(0);
                    }
                }
            }, std::move(o.key().value));
        }
    );

    auto transactionResponsePrinter = M::pureExporter<M::Key<TI::TransactionResponse>>(
        [&env](M::Key<TI::TransactionResponse> &&r) {
            std::ostringstream oss;
            oss << "Got transaction response {";
            oss << "globalVersion=" << r.key().value.globalVersion;
            oss << ",requestDecision=" << r.key().value.requestDecision;
            oss << "}";
            env.log(infra::LogLevel::Info, oss.str());
        }
    );

    auto commandParts = r.importItem("lineImporter", lineImporter);
    auto gsCmd = r.execute("subscriptionCommandParser", subscriptionCommandParser, commandParts.clone());
    auto tiCmd = r.execute("transactionCommandParser", transactionCommandParser, commandParts.clone());

    r.placeOrderWithFacility(
        r.execute("keyifyGS", keyifyGS, std::move(gsCmd))
        , "gsFacility", gsFacility
        , r.actionAsSink("gsReceiver", gsReceiver)
    );
    r.placeOrderWithFacility(
        r.execute("keyifyTI", keyifyTI, std::move(tiCmd))
        , "tiFacility", tiFacility
        , r.actionAsSink("tiReceiver", tiReceiver)
    );

    r.exportItem("transactionResponsePrinter", transactionResponsePrinter
        , r.actionAsSource(tiReceiver));

    r.exportItem("subscriptionIDManager", subscriptionIDManager
        , r.actionAsSource(gsReceiver));

    r.exportItem("fullDataPrinter", fullDataPrinter
        , r.execute("getFullData", getFullData, r.actionAsSource(gsReceiver)));

    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "transaction_redundancy_test_client");
    r.finalize();

    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "Transaction redundancy test client started");

    infra::terminationController(infra::RunForever {});

    return 0;
}