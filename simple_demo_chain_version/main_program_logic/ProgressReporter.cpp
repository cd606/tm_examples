#include "ProgressReporter.hpp"
#include <unordered_set>
#include <sstream>

namespace simple_demo_chain_version { namespace main_program_logic {

    std::vector<std::string> ProgressReporter::reportProgress(
        std::tuple<std::string, std::optional<ChainData>> &&latestChainData
    ) {
        auto const &id = std::get<0>(latestChainData);
        if (!std::get<1>(latestChainData)) {
            std::ostringstream oss;
            oss << "[ProgressReporterImpl::reportProgress] new request placement failure for '"
                << id
                << "'"
                ;
            return { oss.str() };
        }
        return std::visit([&id](auto const &update) -> std::vector<std::string> {
            using T = std::decay_t<decltype(update)>;
            std::vector<std::string> ret;
            if constexpr (std::is_same_v<T, PlaceRequest>) {
                std::ostringstream oss;
                oss << "[ProgressReporterImpl::reportProgress] new request placed with id "
                    << update.id
                    << " with value "
                    << update.value
                    << " for '"
                    << id 
                    << "'"
                    ;
                return { oss.str() };
            } else if constexpr (std::is_same_v<T, ConfirmRequestReceipt>) {
                for (auto const &updateID : update.ids) {
                    std::ostringstream oss;
                    oss << "[ProgressReporterImpl::reportProgress] Request with id "
                        << updateID
                        << " has been confirmed for ids including '"
                        << id
                        << "'"
                    ;
                    ret.push_back(oss.str());
                }
            } else if constexpr (std::is_same_v<T, RespondToRequest>) {
                std::ostringstream oss;
                oss << "[ProgressReporterImpl::reportProgress] Received response to request with id "
                    << update.id
                    << ": response value=" << update.response
                    << " for '"
                    << id
                    << "', this " << (update.isFinalResponse?"is ":"is not ") << "the final response"
                ;
                ret.push_back(oss.str());
            } else if constexpr (std::is_same_v<T, RequestCompleted>) {
                std::ostringstream oss;
                oss << "[ProgressReporterImpl::reportProgress] The request with id "
                    << update.id
                    << " for '"
                    << id
                    << "' has been completed: fashion=" << update.fashion
                ;
                ret.push_back(oss.str());
            }
            return ret;
        }, std::get<1>(latestChainData)->update);
    }

} }