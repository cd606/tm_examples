#ifndef CALCULATOR_FACILITY_INPUT_HANDLER_HPP_
#define CALCULATOR_FACILITY_INPUT_HANDLER_HPP_

#include "defs.pb.h"
#include "simple_demo_chain_version/chain_data/ChainData.hpp"
#include "simple_demo_chain_version/calculator_logic/CalculatorStateFolder.hpp"
#include "simple_demo_chain_version/external_logic/ExternalCalculator.hpp"

#include <tm_kit/infra/RealTimeApp.hpp>

using namespace dev::cd606::tm;

namespace simple_demo_chain_version { namespace calculator_logic {

    //This worker simply puts the result of external calculation on the chain
    
    class CalculatorFacilityInputHandler {
    public:
        using InputType = ExternalCalculatorOutput;
        using ResponseType = simple_demo_chain_version::ChainData;

        template <class Env>
        using RealInput = typename infra::RealTimeApp<Env>::template TimedDataType<
            typename infra::RealTimeApp<Env>::template Key<InputType>
        >;

        template <class Env, class Chain>
        static void initialize(Env *env, Chain *chain) {}

        template <class Env, class Chain>
        static std::tuple<
            ResponseType
            , std::optional<std::tuple<std::string, simple_demo_chain_version::ChainData>>
        > handleInput(Env *env, Chain *chain, RealInput<Env> const &input, CalculatorState const &state) {
            int64_t now = infra::withtime_utils::sinceEpoch<std::chrono::milliseconds>(env->now());
            simple_demo_chain_version::ChainData d {
                now
                , RespondToRequest {
                    input.value.key().id
                    , input.value.key().output 
                    , (input.value.key().output < 0)
                }
            };
            return {
                d
                , std::tuple<std::string, simple_demo_chain_version::ChainData> {
                    ""
                    , d
                }
            };
        }
    };
} }

#endif