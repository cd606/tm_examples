#include "OperationLogic.hpp"
#include <unordered_set>
#include <mutex>
#include <cmath>
#include <iostream>
#include <sstream>

namespace simple_demo_chain_version { namespace main_program_logic {

    class ExponentialAverageImpl {
    private:
        double decaySpeed_;
        double avgFactor_, avg_;
        std::optional<std::chrono::system_clock::time_point> avgTime_;
    public:
        ExponentialAverageImpl(double decaySpeed) : decaySpeed_(decaySpeed), avgFactor_(0.0), avg_(0.0), avgTime_(std::nullopt) {}
        void add(std::tuple<std::chrono::system_clock::time_point, double> &&d) {
            if (!avgTime_) {
                avgTime_ = std::get<0>(d);
                avgFactor_ = 1.0;
                avg_ = std::get<1>(d);
                return;
            }
            if (std::get<0>(d) < *avgTime_) {
                return;
            }
            auto decayTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::get<0>(d)-(*avgTime_)).count();
            *avgTime_ = std::get<0>(d);
            double decay = std::exp(decayTime*0.001*decaySpeed_);
            avgFactor_ = avgFactor_*decay + 1;
            avg_ = (avg_*decay + std::get<1>(d))/avgFactor_;
        }
        std::optional<double> readResult() const {
            return avg_;
        }
    };

    ExponentialAverage::ExponentialAverage(double decaySpeed) : impl_(std::make_unique<ExponentialAverageImpl>(decaySpeed)) {}
    ExponentialAverage::~ExponentialAverage() {}
    ExponentialAverage::ExponentialAverage(ExponentialAverage &&) = default;
    ExponentialAverage &ExponentialAverage::operator=(ExponentialAverage &&) = default;
    void ExponentialAverage::add(std::tuple<std::chrono::system_clock::time_point, double> &&d) {
        impl_->add(std::move(d));
    }
    std::optional<double> ExponentialAverage::readResult() const {
        return impl_->readResult();
    }

    class OperationLogicImpl {
    private:
        std::function<void(std::string const &)> logger_;
        std::function<void(bool)> statusUpdater_;

        bool enabled_;
    public:
        OperationLogicImpl(std::function<void(std::string const &)> logger, std::function<void(bool)> statusUpdater)
            : logger_(logger), statusUpdater_(statusUpdater), enabled_(true) {
            statusUpdater_(true);
        }
        ~OperationLogicImpl() = default;
        std::optional<double> runLogic(double input) {
            if (enabled_) {
                return input;
            } else {
                return std::nullopt;
            }
        }
        ConfigureResult configure(std::tuple<std::string, ConfigureCommand> &&cmd) {
            enabled_ = std::get<1>(cmd).enabled();
            logger_(std::string("The logic is ")+(enabled_?"enabled":"disabled")+" by '"+std::get<0>(cmd)+"'");
            statusUpdater_(enabled_);
            ConfigureResult res;
            res.set_enabled(enabled_);
            return res;
        }
    };

    OperationLogic::OperationLogic(std::function<void(std::string const &)> logger, std::function<void(bool)> statusUpdater)
        : impl_(std::make_unique<OperationLogicImpl>(logger, statusUpdater)) {}
    OperationLogic::~OperationLogic() {}
    OperationLogic::OperationLogic(OperationLogic &&) = default;
    OperationLogic &OperationLogic::operator=(OperationLogic &&) = default;
    std::optional<double> OperationLogic::runLogic(double &&input) {
        return impl_->runLogic(input);
    }
    ConfigureResult OperationLogic::configure(std::tuple<std::string, ConfigureCommand> &&cmd) {
        return impl_->configure(std::move(cmd));
    }

} }
