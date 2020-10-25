#include "MainProgramStateFolder.hpp"

namespace simple_demo_chain_version { namespace main_program_logic {
    void MainProgramStateFolder::foldInPlace(MainProgramStateFolder::ResultType &state, std::string_view const &id, ChainData const *item) {
        state.latestID = id;
        state.updateTimestamp = item->timestamp;
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
        }, item->update);
    }
} }