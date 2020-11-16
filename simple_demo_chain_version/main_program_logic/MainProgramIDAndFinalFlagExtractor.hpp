#ifndef MAIN_PROGRAM_ID_AND_FINAL_FLAG_EXTRACTOR_HPP_
#define MAIN_PROGRAM_ID_AND_FINAL_FLAG_EXTRACTOR_HPP_

#include "simple_demo_chain_version/chain_data/ChainData.hpp"
#include <mutex>
#include <unordered_map>

namespace simple_demo_chain_version { namespace main_program_logic {

    template <class Env>
    class MainProgramIDAndFinalFlagExtractor {
    private:
        std::mutex mutex_;
        std::unordered_map<int, typename Env::IDType> idMap_;
    public:
        MainProgramIDAndFinalFlagExtractor() : mutex_(), idMap_() {}
        MainProgramIDAndFinalFlagExtractor(MainProgramIDAndFinalFlagExtractor &&e) : mutex_(), idMap_(std::move(e.idMap_)) {}
        auto extract(ChainData const &data) 
        -> std::vector<std::tuple<typename Env::IDType, bool>> {
            return std::visit([this](auto const &u) -> std::vector<std::tuple<typename Env::IDType, bool>> {
                using T = std::decay_t<decltype(u)>;
                if constexpr (std::is_same_v<T, simple_demo_chain_version::PlaceRequest>) {
                    std::lock_guard<std::mutex> _(mutex_);
                    auto externalID = Env::id_from_bytes(basic::byteDataView(u.externalID));
                    idMap_.insert({u.id, externalID});
                    return {{externalID, false}};
                } else if constexpr (std::is_same_v<T, simple_demo_chain_version::ConfirmRequestReceipt>) {
                    std::lock_guard<std::mutex> _(mutex_);
                    std::vector<std::tuple<typename Env::IDType, bool>> ret;
                    for (auto const &id : u.ids) {
                        auto iter = idMap_.find(id);
                        if (iter != idMap_.end()) {
                            ret.push_back({iter->second, false});
                        }
                    }
                    return ret;
                } else if constexpr (std::is_same_v<T, simple_demo_chain_version::RespondToRequest>) {
                    std::lock_guard<std::mutex> _(mutex_);
                    auto iter = idMap_.find(u.id);
                    if (iter != idMap_.end()) {
                        return {{iter->second, false}};
                    } else {
                        return {};
                    }
                } else if constexpr (std::is_same_v<T, simple_demo_chain_version::RequestCompleted>) {
                    std::lock_guard<std::mutex> _(mutex_);
                    auto iter = idMap_.find(u.id);
                    if (iter != idMap_.end()) {
                        return {{iter->second, true}};
                    } else {
                        return {};
                    }             
                } else {
                    return {};
                }
            }, data.update);
        }
        static bool writeSucceeded(bool x) {
            return x;
        }
    };

} }

#endif