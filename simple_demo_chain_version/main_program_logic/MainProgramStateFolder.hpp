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
        ((typename Chain::StorageIDType, latestID)) \
        ((int64_t, updateTimestamp))

    TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT(((typename, Chain)), MainProgramState, MainProgramStateFields);

    template <class Env, class Chain>
    class MainProgramStateFolder {
    public:
        using ResultType = MainProgramState<Chain>;
        static ResultType initialize(Env *env, Chain *chain) {
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
        static typename Chain::StorageIDType chainIDForValue(ResultType const &r) {
            return r.latestID;
        }
        static void foldInPlace(ResultType &state, typename Chain::ItemType const &item) {
            static_assert(std::is_same_v<typename Chain::DataType, simple_demo_chain_version::ChainData>);

            state.latestID = Chain::extractStorageID(item);
            ChainData const *p = Chain::extractData(item);
            if (!p) {
                return;
            }
            state.updateTimestamp = p->timestamp;
            std::visit([&state](auto const &content) {
                using T = std::decay_t<decltype(content)>;
                if constexpr (std::is_same_v<T, simple_demo_chain_version::PlaceRequest>) {
                    if (state.max_id_sofar < content.id) {
                        state.max_id_sofar = content.id;
                    }
                    state.outstandingIDs.insert(content.id);
                } else if constexpr (std::is_same_v<T, simple_demo_chain_version::RequestCompleted>) {
                    state.outstandingIDs.erase(content.id);
                }
            }, p->update);
        }
        static std::chrono::system_clock::time_point extractTime(ResultType const &st) {
            return infra::withtime_utils::epochDurationToTime<std::chrono::milliseconds>(st.updateTimestamp);
        }
    };

} }

TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT_SERIALIZE_NO_FIELD_NAMES(((typename, Chain)), simple_demo_chain_version::main_program_logic::MainProgramState, MainProgramStateFields);

#endif