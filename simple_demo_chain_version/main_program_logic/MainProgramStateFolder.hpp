#ifndef MAIN_PROGRAM_STATE_FOLDER_HPP_
#define MAIN_PROGRAM_STATE_FOLDER_HPP_

#include "defs.pb.h"
#include "simple_demo_chain_version/chain_data/ChainData.hpp"

#include <tm_kit/infra/ChronoUtils.hpp>
#include <unordered_set>

using namespace dev::cd606::tm;

namespace simple_demo_chain_version { namespace main_program_logic {

    #define MainProgramStateFields \
        ((int, max_id_sofar)) \
        ((std::unordered_set<int>, outstandingIDs)) \
        ((std::string, latestID)) \
        ((int64_t, updateTimestamp))

    TM_BASIC_CBOR_CAPABLE_STRUCT(MainProgramState, MainProgramStateFields);

    class MainProgramStateFolder {
    public:
        using ResultType = MainProgramState;
        template <class Chain>
        static ResultType initialize(void *env, Chain *chain) {
            if constexpr (Chain::SupportsExtraData) {
                auto val = chain->template loadExtraData<ResultType>(
                    "main_program_state"
                );
                if (val) {
                    return *val;
                } else {
                    ResultType res;
                    res.max_id_sofar = 0;
                    res.updateTimestamp = 0;
                    return res;
                }
            } else {
                ResultType res;
                res.max_id_sofar = 0;
                res.updateTimestamp = 0;
                return res;
            }
        }
        static std::string const &chainIDForValue(ResultType const &r) {
            return r.latestID;
        }
        static void foldInPlace(ResultType &state, std::string_view const &id, ChainData const *item);
        static std::chrono::system_clock::time_point extractTime(ResultType const &st) {
            return infra::withtime_utils::epochDurationToTime<std::chrono::milliseconds>(st.updateTimestamp);
        }
    };

} }

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(simple_demo_chain_version::main_program_logic::MainProgramState, MainProgramStateFields);

#endif