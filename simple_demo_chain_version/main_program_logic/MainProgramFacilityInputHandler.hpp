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
    public:
        using InputType = double;
        using ResponseType = std::optional<ChainData>;

        using RealInput = typename infra::RealTimeApp<Env>::template TimedDataType<
            typename infra::RealTimeApp<Env>::template Key<InputType>
        >;

        static void initialize(Env *env, Chain *chain) {}

        static std::tuple<
            ResponseType
            , std::optional<std::tuple<typename Chain::StorageIDType, ChainData>>
        > handleInput(Env *env, Chain *chain, RealInput &&input, MainProgramState<Chain> const &state) {
            if (state.outstandingIDs.size() < 2) {
                int64_t now = infra::withtime_utils::sinceEpoch<std::chrono::milliseconds>(env->now());
                ChainData d {
                    now
                    , PlaceRequest {
                        state.max_id_sofar+1
                        , input.value.key()
                    }
                };
                return {
                    d
                    , std::tuple<typename Chain::StorageIDType, ChainData> {
                        Chain::template newStorageID<Env>()
                        , d
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
    };
} }

#endif