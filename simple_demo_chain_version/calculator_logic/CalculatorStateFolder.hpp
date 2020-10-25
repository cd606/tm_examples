#ifndef CALCULATOR_STATE_FOLDER_HPP_
#define CALCULATOR_STATE_FOLDER_HPP_

#include "defs.pb.h"
#include "simple_demo_chain_version/chain_data/ChainData.hpp"

#include <tm_kit/infra/ChronoUtils.hpp>
#include <unordered_map>

using namespace dev::cd606::tm;

namespace simple_demo_chain_version { namespace calculator_logic {

    #define OneRequestStateFields \
        ((int64_t, placedTimestamp)) \
        ((int64_t, acceptedTimestamp)) \
        ((int64_t, latestResponseTimestamp)) \
        ((bool, finalResponseReceived)) \
        ((double, requestValue))

    TM_BASIC_CBOR_CAPABLE_STRUCT(OneRequestState, OneRequestStateFields);

#ifdef _MSC_VER 
    #define CalculatorStateFields \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::unordered_map<int, simple_demo_chain_version::calculator_logic::OneRequestState>), newlyPlacedRequests)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::unordered_map<int, simple_demo_chain_version::calculator_logic::OneRequestState>), requestsBeingHandled)) \
        ((std::string, latestID)) \
        ((int64_t, updateTimestamp))
#else
    #define CalculatorStateFields \
        (((std::unordered_map<int, simple_demo_chain_version::calculator_logic::OneRequestState>), newlyPlacedRequests)) \
        (((std::unordered_map<int, simple_demo_chain_version::calculator_logic::OneRequestState>), requestsBeingHandled)) \
        ((std::string, latestID)) \
        ((int64_t, updateTimestamp))
#endif

    TM_BASIC_CBOR_CAPABLE_STRUCT(CalculatorState, CalculatorStateFields);

    class CalculatorStateFolder {
    public:
        using ResultType = CalculatorState;
        template <class Chain>
        static ResultType initialize(void *, Chain *chain) {
            if constexpr (Chain::SupportsExtraData) {
                auto val = chain->template loadExtraData<ResultType>(
                    "calculator_state"
                );
                if (val) {
                    return *val;
                } else {
                    return ResultType();
                }
            } else {
                return ResultType();
            }
        }
        static std::string const &chainIDForValue(ResultType const &r) {
            return r.latestID;
        }
        static void foldInPlace(ResultType &state, std::string_view const &storageIDView, ChainData const *item);
        static std::chrono::system_clock::time_point extractTime(ResultType const &st) {
            return infra::withtime_utils::epochDurationToTime<std::chrono::milliseconds>(st.updateTimestamp);
        }
    };

} }

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(simple_demo_chain_version::calculator_logic::OneRequestState, OneRequestStateFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(simple_demo_chain_version::calculator_logic::CalculatorState, CalculatorStateFields);

#endif