#include "CalculatorIdleWorker.hpp"

namespace simple_demo_chain_version { namespace calculator_logic {
    std::tuple<
        std::optional<CalculatorIdleWorker::OffChainUpdateType>
        , std::vector<std::tuple<std::string, simple_demo_chain_version::ChainData>>
    > CalculatorIdleWorker::realWork(int64_t now, CalculatorState const &state) {
        std::unordered_map<int, double> valueRef;
        if (!state.newlyPlacedRequests.empty()) {
            std::vector<std::tuple<int,double>> canAccept;
            std::optional<int> firstToTimeout = std::nullopt;
            for (auto const &item : state.newlyPlacedRequests) {
                if (item.second.placedTimestamp + 1000 >= now) {
                    canAccept.push_back({item.first, item.second.requestValue});
                } else {
                    firstToTimeout = item.first;
                }
            }
            if (!canAccept.empty()) {
                std::vector<int> ids;
                for (auto const &item : canAccept) {
                    ids.push_back(std::get<0>(item));
                    valueRef.insert({std::get<0>(item), std::get<1>(item)});
                }
                simple_demo_chain_version::ChainData d {
                    now
                    , ConfirmRequestReceipt {std::move(ids)}
                };
                return {
                    OffChainUpdateType {d, valueRef}
                    , {std::tuple<std::string, simple_demo_chain_version::ChainData> {
                        ""
                        , d
                    }}
                };
            } else if (firstToTimeout) {
                simple_demo_chain_version::ChainData d {
                    now
                    , RequestCompleted {
                        *firstToTimeout
                        , RequestCompletedFashion::TimeoutBeforeAcceptance
                    }
                };
                return {
                    OffChainUpdateType {d, valueRef}
                    , {std::tuple<std::string, simple_demo_chain_version::ChainData> {
                        ""
                        , d
                    }}
                };
            }
        } else if (!state.requestsBeingHandled.empty()) {
            for (auto const &item : state.requestsBeingHandled) {
                if (item.second.finalResponseReceived) {
                    simple_demo_chain_version::ChainData d {
                        now
                        , RequestCompleted {item.first, RequestCompletedFashion::Fulfilled}
                    };
                    return {
                        OffChainUpdateType {d, valueRef}
                        , {std::tuple<std::string, simple_demo_chain_version::ChainData> {
                            ""
                            , d
                        }, std::tuple<std::string, simple_demo_chain_version::ChainData> {
                            ""
                            , d
                        }}
                    };
                } else if (item.second.latestResponseTimestamp+5000 < now) {
                    if (item.second.latestResponseTimestamp != 0) {
                        simple_demo_chain_version::ChainData d {
                            now
                            , RequestCompleted {item.first, RequestCompletedFashion::PartiallyHandledThenTimeout}
                        };
                        return {
                            OffChainUpdateType {d, valueRef}
                            , {std::tuple<std::string, simple_demo_chain_version::ChainData> {
                                ""
                                , d
                            }}
                        };
                    } else if (item.second.acceptedTimestamp+5000 < now) {
                        simple_demo_chain_version::ChainData d {
                            now
                            , RequestCompleted {item.first, RequestCompletedFashion::AcceptedThenTimeoutBeforeResponse}
                        };
                        return {
                            OffChainUpdateType {d, valueRef}
                            , {std::tuple<std::string, simple_demo_chain_version::ChainData> {
                                ""
                                , d
                            }}
                        };
                    }
                }
            }
        }
        return {
            std::nullopt
            , {}
        };
    }
} }