#ifndef CALCULATOR_IDLE_WORKER_HPP_
#define CALCULATOR_IDLE_WORKER_HPP_

#include "defs.pb.h"
#include "simple_demo_chain_version/chain_data/ChainData.hpp"
#include "simple_demo_chain_version/calculator_logic/CalculatorStateFolder.hpp"

#include <thread>

namespace simple_demo_chain_version { namespace calculator_logic {

    //This worker scans the ids and finds the first one(s) that can be marked
    //as completed. It also accepts newly placed requests.
    
    template <class Env, class Chain>
    class CalculatorIdleWorker {
    private:
        int64_t lastSaveTime = 0;
    public:
        struct OffChainUpdateType {
            simple_demo_chain_version::ChainData action;
            std::unordered_map<int, double> valueRef;
        };

        void initialize(Env *env, Chain *chain) {}

        std::tuple<
            std::optional<OffChainUpdateType>
            , std::optional<std::tuple<typename Chain::StorageIDType, simple_demo_chain_version::ChainData>>
        > work(Env *env, Chain *chain, CalculatorState<Chain> const &state) {
            int64_t now = infra::withtime_utils::sinceEpoch<std::chrono::milliseconds>(env->now());
            if constexpr (Chain::SupportsExtraData) {
                if (now > lastSaveTime+10000) {
                    //save state every 10 seconds
                    std::thread([chain,state]() {
                        chain->template saveExtraData<CalculatorState<Chain>>(
                            "calculator_state", state
                        );
                    }).detach();
                    lastSaveTime = now;
                }
            }
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
                        , std::tuple<typename Chain::StorageIDType, simple_demo_chain_version::ChainData> {
                            Chain::template newStorageID<Env>()
                            , d
                        }
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
                        , std::tuple<typename Chain::StorageIDType, simple_demo_chain_version::ChainData> {
                            Chain::template newStorageID<Env>()
                            , d
                        }
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
                            , std::tuple<typename Chain::StorageIDType, simple_demo_chain_version::ChainData> {
                                Chain::template newStorageID<Env>()
                                , d
                            }
                        };
                    } else if (item.second.latestResponseTimestamp+5000 < now) {
                        if (item.second.latestResponseTimestamp != 0) {
                            simple_demo_chain_version::ChainData d {
                                now
                                , RequestCompleted {item.first, RequestCompletedFashion::PartiallyHandledThenTimeout}
                            };
                            return {
                                OffChainUpdateType {d, valueRef}
                                , std::tuple<typename Chain::StorageIDType, simple_demo_chain_version::ChainData> {
                                    Chain::template newStorageID<Env>()
                                    , d
                                }
                            };
                        } else if (item.second.acceptedTimestamp+5000 < now) {
                            simple_demo_chain_version::ChainData d {
                                now
                                , RequestCompleted {item.first, RequestCompletedFashion::AcceptedThenTimeoutBeforeResponse}
                            };
                            return {
                                OffChainUpdateType {d, valueRef}
                                , std::tuple<typename Chain::StorageIDType, simple_demo_chain_version::ChainData> {
                                    Chain::template newStorageID<Env>()
                                    , d
                                }
                            };
                        }
                    }
                }
            }
            return {
                std::nullopt
                , std::nullopt
            };
        }
    };
} }

#endif