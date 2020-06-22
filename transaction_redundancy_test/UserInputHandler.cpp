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
#include <tm_kit/basic/transaction/SingleKeyAsyncWatchableTransactionHandlerComponent.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/redis/RedisComponent.hpp>
#include <tm_kit/transport/redis/RedisOnOrderFacility.hpp>

#include "TransactionInterface.hpp"

using namespace dev::cd606::tm;
using namespace test;

int main() {
    using TI = basic::transaction::SingleKeyTransactionInterface<
        TransactionKey
        , TransactionData
        , TransactionDataVersion
        , transport::BoostUUIDComponent::IDType
        , TransactionDataSummary
        , TransactionDataDelta
        , infra::ArrayComparerWithSkip<int64_t, 3>
    >;
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::real_time_clock::ClockComponent>,
        transport::BoostUUIDComponent,
        transport::ClientSideSimpleIdentityAttacherComponent<
            std::string
            , TI::BasicFacilityInput>,
        transport::redis::RedisComponent
    >;
    using M = infra::RealTimeMonad<TheEnvironment>;
    using R = infra::MonadRunner<M>;
    TheEnvironment env;
    env.transport::ClientSideSimpleIdentityAttacherComponent<std::string,TI::BasicFacilityInput>::operator=(
        transport::ClientSideSimpleIdentityAttacherComponent<std::string,TI::BasicFacilityInput>(
            "transaction_redundancy_test_client"
        )
    );

    using VD = infra::VersionedData<TransactionDataVersion, std::optional<TransactionData>, infra::ArrayComparerWithSkip<int64_t, 3>>;
    VD currentData;
    std::mutex dataMutex;

    auto lineImporter = M::simpleImporter<std::string>(
        [](M::PublisherCall<std::string> &pub) {
            linenoiseHistorySetMaxLen(100);
            char *p;
            while ((p = linenoise(">>")) != nullptr) {
                if (p[0] != '\0') {
                    linenoiseHistoryAdd(p);
                }
                pub(std::string {p});
            }
            exit(0);
        }
        , infra::LiftParameters<std::chrono::system_clock::time_point>()
            .SuggestThreaded(true)
    );

    auto parser = M::liftMaybe<std::string>(
        [&env,&currentData,&dataMutex](std::string const &s) -> std::optional<TI::BasicFacilityInput> {
            auto trimmed = boost::trim_copy(s);
            if (trimmed.empty()) {
                return std::nullopt;
            }
            std::vector<std::string> parts;
            boost::split(parts, trimmed, boost::is_any_of(" "));
            if (parts[0] == "subscribe") {
                if (parts.size() != 1) {
                    env.log(infra::LogLevel::Info, "Subscribe command usage: subscribe");
                    return std::nullopt;
                }
                return TI::BasicFacilityInput {
                        { TI::Subscription {
                            TransactionKey {}
                        } }
                    };
            } else if (parts[0] == "inject") {
                if (parts.size() != 3) {
                    env.log(infra::LogLevel::Info, "Inject command usage: inject A_incr B_incr");
                    return std::nullopt;
                }
                try {
                    uint32_t aIncr = boost::lexical_cast<uint32_t>(parts[1]);
                    uint32_t bIncr = boost::lexical_cast<uint32_t>(parts[2]);
                    {
                        std::lock_guard<std::mutex> _(dataMutex);
                        return TI::BasicFacilityInput {
                            { TI::Transaction {
                                TI::UpdateAction {
                                    TransactionKey {}
                                    , currentData.version
                                    , TransactionDataSummary {}
                                    , TransactionDataDelta {
                                        InjectRequest {
                                            {}
                                            , InjectData {
                                                aIncr, bIncr
                                            }
                                        }
                                    }
                                }
                            } }
                        };
                    }
                } catch (boost::bad_lexical_cast const &) {
                    env.log(infra::LogLevel::Info, "Inject command usage: inject A_incr B_incr");
                    return std::nullopt;
                }
            } else if (parts[0] == "transfer") {
                if (parts.size() != 2) {
                    env.log(infra::LogLevel::Info, "Transfer command usage: transfer amount");
                    return std::nullopt;
                }
                try {
                    int32_t amt = boost::lexical_cast<int32_t>(parts[1]);
                    {
                        std::lock_guard<std::mutex> _(dataMutex);
                        return TI::BasicFacilityInput {
                            { TI::Transaction {
                                TI::UpdateAction {
                                    TransactionKey {}
                                    , currentData.version
                                    , TransactionDataSummary {}
                                    , TransactionDataDelta {
                                        TransferRequest {
                                            {}
                                            , TransferData {
                                                amt
                                            }
                                        }
                                    }
                                }
                            } }
                        };
                    }
                } catch (boost::bad_lexical_cast const &) {
                    env.log(infra::LogLevel::Info, "Transfer command usage: transfer amount");
                    return std::nullopt;
                }
            } else if (parts[0] == "process") {
                if (parts.size() != 1) {
                    env.log(infra::LogLevel::Info, "Process command usage: process");
                    return std::nullopt;
                }
                {
                    std::lock_guard<std::mutex> _(dataMutex);
                    return TI::BasicFacilityInput {
                        { TI::Transaction {
                            TI::UpdateAction {
                                TransactionKey {}
                                , currentData.version
                                , TransactionDataSummary {}
                                , TransactionDataDelta {
                                    ProcessRequest {}
                                }
                            }
                        } }
                    };
                }
            }
            return std::nullopt;
        }
    );

    auto keyify = M::kleisli<TI::BasicFacilityInput>(
        basic::CommonFlowUtilComponents<M>::keyify<TI::BasicFacilityInput>()
    );

    auto facility = transport::redis::RedisOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::createTypedRPCOnOrderFacility
        <TI::BasicFacilityInput,TI::FacilityOutput>(
        transport::ConnectionLocator::parse("127.0.0.1:6379:::test_etcd_transaction_queue")
    );

    auto extractValue = M::liftMaybe<M::KeyedData<TI::BasicFacilityInput,TI::FacilityOutput>>(
        [&env](M::KeyedData<TI::BasicFacilityInput,TI::FacilityOutput> &&data) -> std::optional<VD> {
            return std::visit([&env](auto &&d) -> std::optional<VD> {
                using T = std::decay_t<decltype(d)>;
                if constexpr (std::is_same_v<T, TI::OneValue>) {
                    return VD {
                        std::move(d.version), std::move(d.data)
                    };
                } else if constexpr (std::is_same_v<T, TI::TransactionFailurePermission>) {
                    env.log(infra::LogLevel::Info, "transaction failed because of permission");
                    return std::nullopt;
                } else if constexpr (std::is_same_v<T, TI::TransactionFailurePrecondition>) {
                    env.log(infra::LogLevel::Info, "transaction failed because of precondition");
                    return std::nullopt;
                } else if constexpr (std::is_same_v<T, TI::TransactionSuccess>) {
                    env.log(infra::LogLevel::Info, "transaction succeeded");
                    return std::nullopt;
                } else {
                    return std::nullopt;
                }
            }, std::move(data.data.value));
        }
    );
    auto exporter = M::pureExporter<VD>(
        [&env,&currentData,&dataMutex](VD &&data) {
            std::ostringstream oss;
            {
                std::lock_guard<std::mutex> _(dataMutex);
                currentData = std::move(data);
                
                oss << "{accountA=";
                if (currentData.data) {
                    oss << currentData.data->accountA;
                } else {
                    oss << "(none)";
                }
                oss << ",accountB=";
                if (currentData.data) {
                    oss << currentData.data->accountB;
                } else {
                    oss << "(none)";
                }
                oss << ",pendingTransfers=";
                if (currentData.data) {
                    oss << "[";
                    for (auto const &t : currentData.data->pendingTransfers.items) {
                        oss << t << ' ';
                    }
                    oss << "]";
                    oss << " (" << currentData.data->pendingTransfers.items.size() << " items)";
                } else {
                    oss << "(none)";
                }
                oss << " (version:[";
                for (auto const &v : currentData.version) {
                    oss << v << ' ';
                };
                oss << "])";                
            }
            env.log(infra::LogLevel::Info, oss.str());
        }
    );

    R r(&env);
    r.placeOrderWithFacility(
        r.execute("keyify", keyify, 
            r.execute("parser", parser, 
                r.importItem("lineImporter", lineImporter)))
        , "facility", facility
        , r.actionAsSink("extractValue", extractValue)
    );
    r.exportItem("exporter", exporter, r.actionAsSource(extractValue));

    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "transaction_redundancy_test_client");
    r.finalize();

    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "Transaction redundancy test client started");

    infra::terminationController(infra::RunForever {});
}