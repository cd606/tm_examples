#include "ProgressReporter.hpp"
#include <unordered_set>
#include <sstream>

namespace simple_demo_chain_version { namespace main_program_logic {

    class ProgressReporterImpl {
    private:
        std::unordered_set<int> myRequestIDs_;
    public:
        ProgressReporterImpl() : myRequestIDs_() {}
        ~ProgressReporterImpl() {}
        std::vector<std::string> reportProgress(
            int which
            , std::optional<PlaceRequest> &&placedRequest
            , ChainData &&latestChainData
        ) {
            if (which == 0) {
                if (placedRequest) {
                    myRequestIDs_.insert(placedRequest->id);
                    std::ostringstream oss;
                    oss << "[ProgressReporterImpl::reportProgress] new request placed with id "
                        << placedRequest->id
                        << " with value "
                        << placedRequest->value
                        ;
                    return { oss.str() };
                } else {
                    return {};
                }
            } else {
                return std::visit([this](auto &&update) -> std::vector<std::string> {
                    using T = std::decay_t<decltype(update)>;
                    std::vector<std::string> ret;
                    if constexpr (std::is_same_v<T, ConfirmRequestReceipt>) {
                        for (auto const &id : update.ids) {
                            if (myRequestIDs_.find(id) != myRequestIDs_.end()) {
                                std::ostringstream oss;
                                oss << "[ProgressReporterImpl::reportProgress] Request with id "
                                    << id
                                    << " has been confirmed"
                                ;
                                ret.push_back(oss.str());
                            }
                        }
                    } else if constexpr (std::is_same_v<T, RespondToRequest>) {
                        if (myRequestIDs_.find(update.id) != myRequestIDs_.end()) {
                            std::ostringstream oss;
                            oss << "[ProgressReporterImpl::reportProgress] Received response to request with id "
                                << update.id
                                << ": response value=" << update.response
                                << ", this " << (update.isFinalResponse?"is ":"is not ") << " the final response"
                            ;
                            ret.push_back(oss.str());
                        }
                    } else if constexpr (std::is_same_v<T, RequestCompleted>) {
                        if (myRequestIDs_.find(update.id) != myRequestIDs_.end()) {
                            std::ostringstream oss;
                            oss << "[ProgressReporterImpl::reportProgress] The request with id "
                                << update.id
                                << " has been completed: fashion=" << update.fashion
                            ;
                            ret.push_back(oss.str());
                        }
                    }
                    return ret;
                }, latestChainData.update);
            }
        }
    };

    ProgressReporter::ProgressReporter() : impl_(std::make_unique<ProgressReporterImpl>()) {}
    ProgressReporter::~ProgressReporter() {}
    std::vector<std::string> ProgressReporter::reportProgress(
        int which
        , std::optional<PlaceRequest> &&placedRequest
        , ChainData &&latestChainData
    ) {
        return impl_->reportProgress(
            which, std::move(placedRequest), std::move(latestChainData)
        );
    }

} }