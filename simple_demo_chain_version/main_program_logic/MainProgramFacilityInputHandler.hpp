#ifndef MAIN_PROGRAM_FACILITY_INPUT_HANDLER_HPP_
#define MAIN_PROGRAM_FACILITY_INPUT_HANDLER_HPP_

#include "defs.pb.h"
#include "simple_demo_chain_version/chain_data/ChainData.hpp"
#include "simple_demo_chain_version/main_program_logic/MainProgramStateFolder.hpp"

#include <tm_kit/infra/RealTimeApp.hpp>

using namespace dev::cd606::tm;

namespace simple_demo_chain_version { namespace main_program_logic {

    //This worker simply puts request on the chain
    
    template <class Env, class Chain>
    class MainProgramFacilityInputHandler {
    private:
        int64_t lastSaveTime_ = 0;
        Env *env_ = nullptr;
    public:
        using InputType = double;
        using ResponseType = std::optional<PlaceRequest>;

        using RealInput = typename infra::RealTimeApp<Env>::template TimedDataType<
            typename infra::RealTimeApp<Env>::template Key<InputType>
        >;

        void initialize(Env *env, Chain *chain) {
            env_ = env;
        }

        static std::tuple<
            ResponseType
            , std::optional<std::tuple<typename Chain::StorageIDType, ChainData>>
        > handleInput(Env *env, Chain *chain, RealInput &&input, MainProgramState<Chain> const &state) {
            if (state.outstandingIDs.size() < 2) {
                int64_t now = infra::withtime_utils::sinceEpoch<std::chrono::milliseconds>(env->now());
                PlaceRequest r {
                    state.max_id_sofar+1
                    , input.value.key()
                };
                return {
                    r
                    , std::tuple<typename Chain::StorageIDType, ChainData> {
                        Chain::template newStorageID<Env>()
                        , ChainData {now, r}
                    }
                };
            } else {
                env->log(infra::LogLevel::Info, "Not sending request because there are too many outstanding requests");
                return {
                    std::nullopt
                    , std::nullopt
                };
            }
        }

        void idleCallback(Chain *chain, MainProgramState<Chain> const &state) {
            int64_t now = infra::withtime_utils::sinceEpoch<std::chrono::milliseconds>(env_->now());
            if (now > lastSaveTime_+10000) {
                //save state at most every 10 seconds
                std::thread([chain,state]() {
                    chain->template saveExtraData<MainProgramState<Chain>>(
                        "main_program_state", state
                    );
                }).detach();
                lastSaveTime_ = now;
            }
        }
    };
} }

#endif