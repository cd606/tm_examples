#include "CalculatorStateFolder.hpp"
#include <tm_kit/transport/ConvertChainIDStringToGroup.hpp>

namespace simple_demo_chain_version { namespace calculator_logic {
    void CalculatorStateFolder::foldInPlace(ResultType &state, std::string_view const &storageIDView, ChainData const &item) const {
        state.latestID = storageIDView;
        auto ts = item.timestamp;
        state.updateTimestamp = ts;
        std::visit([this,ts,&storageIDView,&state](auto const &content) {
            using T = std::decay_t<decltype(content)>;
            if constexpr (std::is_same_v<T, simple_demo_chain_version::PlaceRequest>) {
                uint32_t group = dev::cd606::tm::transport::chain_utils::convertChainIDStringToGroup(storageIDView, 10);
                if (!useTheseGroups_ || (useTheseGroups_->find(group) != useTheseGroups_->end())) {
                    OneRequestState oneReq;
                    oneReq.placedTimestamp = ts;
                    oneReq.acceptedTimestamp = 0;
                    oneReq.latestResponseTimestamp = 0;
                    oneReq.finalResponseReceived = false;
                    oneReq.requestValue = content.value;
                    state.newlyPlacedRequests.insert({content.id, oneReq});
                }
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
        }, item.update);
    }
} }