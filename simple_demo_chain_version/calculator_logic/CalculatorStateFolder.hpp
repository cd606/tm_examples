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
        ((typename Chain::StorageIDType, latestID)) \
        ((int64_t, updateTimestamp))
#else
    #define CalculatorStateFields \
        (((std::unordered_map<int, simple_demo_chain_version::calculator_logic::OneRequestState>), newlyPlacedRequests)) \
        (((std::unordered_map<int, simple_demo_chain_version::calculator_logic::OneRequestState>), requestsBeingHandled)) \
        ((typename Chain::StorageIDType, latestID)) \
        ((int64_t, updateTimestamp))
#endif

    TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT(((typename, Chain)), CalculatorState, CalculatorStateFields);

    template <class Env, class Chain>
    class CalculatorStateFolder {
    public:
        using ResultType = CalculatorState<Chain>;
        static ResultType initialize(Env *env, Chain *chain) {
            if constexpr (Chain::SupportsExtraData) {
                auto val = chain->template loadExtraData<ResultType>(
                    "calculator_state"
                );
                if (val) {
                    return *val;
                } else {
                    return ResultType();
                }
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
            auto ts = p->timestamp;
            state.updateTimestamp = ts;
            std::visit([ts,&state](auto const &content) {
                using T = std::decay_t<decltype(content)>;
                if constexpr (std::is_same_v<T, simple_demo_chain_version::PlaceRequest>) {
                    OneRequestState oneReq;
                    oneReq.placedTimestamp = ts;
                    oneReq.acceptedTimestamp = 0;
                    oneReq.latestResponseTimestamp = 0;
                    oneReq.finalResponseReceived = false;
                    oneReq.requestValue = content.value;
                    state.newlyPlacedRequests.insert({content.id, oneReq});
                } else if constexpr (std::is_same_v<T, simple_demo_chain_version::ConfirmRequestReceipt>) {
                    for (auto const &id : content.ids) {
                        auto iter = state.newlyPlacedRequests.find(id);
                        if (iter != state.newlyPlacedRequests.end()) {
                            OneRequestState newOneReq = iter->second;
                            newOneReq.acceptedTimestamp = ts;
                            state.requestsBeingHandled.insert({id, newOneReq});
                            state.newlyPlacedRequests.erase(iter);
                        }
                    }
                } else if constexpr (std::is_same_v<T, simple_demo_chain_version::RespondToRequest>) {
                    auto iter = state.requestsBeingHandled.find(content.id);
                    if (iter != state.requestsBeingHandled.end()) {
                        iter->second.latestResponseTimestamp = ts;
                        if (content.isFinalResponse) {
                            iter->second.finalResponseReceived = true;
                        }
                    }
                } else if constexpr (std::is_same_v<T, simple_demo_chain_version::RequestCompleted>) {
                    state.requestsBeingHandled.erase(content.id);
                    state.newlyPlacedRequests.erase(content.id);
                }
            }, p->update);
        }
        static std::chrono::system_clock::time_point extractTime(ResultType const &st) {
            return infra::withtime_utils::epochDurationToTime<std::chrono::milliseconds>(st.updateTimestamp);
        }
    };

} }

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(simple_demo_chain_version::calculator_logic::OneRequestState, OneRequestStateFields);
TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT_SERIALIZE_NO_FIELD_NAMES(((typename, Chain)), simple_demo_chain_version::calculator_logic::CalculatorState, CalculatorStateFields);

#endif