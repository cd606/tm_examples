#ifndef CALCULATOR_IDLE_WORKER_HPP_
#define CALCULATOR_IDLE_WORKER_HPP_

#include "defs.pb.h"
#include "simple_demo_chain_version/chain_data/ChainData.hpp"
#include "simple_demo_chain_version/calculator_logic/CalculatorStateFolder.hpp"

#include <thread>

namespace simple_demo_chain_version { namespace calculator_logic {

    //This worker scans the ids and finds the first one(s) that can be marked
    //as completed. It also accepts newly placed requests.
    
    class CalculatorIdleWorker {
    private:
        int64_t lastSaveTime = 0;
    public:
        struct OffChainUpdateType {
            simple_demo_chain_version::ChainData action;
            std::unordered_map<int, double> valueRef;
        };

        template <class Env, class Chain>
        void initialize(Env *env, Chain *chain) {}

        template <class Env, class Chain>
        std::tuple<
            std::optional<OffChainUpdateType>
            , std::optional<std::tuple<std::string, simple_demo_chain_version::ChainData>>
        > work(Env *env, Chain *chain, CalculatorState const &state) {
            int64_t now = infra::withtime_utils::sinceEpoch<std::chrono::milliseconds>(env->now());
            if constexpr (Chain::SupportsExtraData) {
                if (now > lastSaveTime+10000) {
                    //save state every 10 seconds
                    std::thread([chain,state]() {
                        chain->template saveExtraData<CalculatorState>(
                            "calculator_state", state
                        );
                    }).detach();
                    lastSaveTime = now;
                }
            }
            return realWork(now, state);
        }

        std::tuple<
            std::optional<OffChainUpdateType>
            , std::optional<std::tuple<std::string, simple_demo_chain_version::ChainData>>
        > realWork(int64_t now, CalculatorState const &state);
    };
} }

#endif