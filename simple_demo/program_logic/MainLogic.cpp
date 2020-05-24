#include "MainLogic.hpp"
#include <unordered_set>
#include <mutex>
#include <cmath>
#include <iostream>
#include <sstream>

class MainLogicImpl {
private:
    std::function<void(std::string const &)> logger_;
    std::unordered_set<int32_t> outstandingCommands_;
    std::mutex mutex_;
    bool enabled_;
    double avgFactor_, avg_;
    std::optional<std::chrono::system_clock::time_point> avgTime_;
    int32_t idCounter_;

    static const double decaySpeed;

    std::optional<simple_demo::CalculateCommand> handleInput(std::tuple<std::chrono::system_clock::time_point, simple_demo::InputData> &&input) {
        if (!avgTime_) {
            avgTime_ = std::get<0>(input);
            avgFactor_ = 1.0;
            avg_ = std::get<1>(input).value();
            logger_("This is the first data received, we just use it to start averaging, so not creating command");
            return std::nullopt;
        }
        if (std::get<0>(input) < *avgTime_) {
            logger_("Time going backwards, not handling");
            return std::nullopt;
        }
        auto decayTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::get<0>(input)-(*avgTime_)).count();
        *avgTime_ = std::get<0>(input);
        double decay = std::exp(decayTime*0.001*decaySpeed);
        avgFactor_ = avgFactor_*decay + 1;
        avg_ = (avg_*decay + std::get<1>(input).value())/avgFactor_;

        if (std::get<1>(input).value() >= avg_*1.05) {
            std::lock_guard<std::mutex> _(mutex_);            
            if (!enabled_) {
                logger_("Not enabled, not sending new command");
                return std::nullopt;
            }
            if (outstandingCommands_.size() >= 2) {
                logger_("Too many outstanding commands, not sending new command");
                return std::nullopt;
            }
            simple_demo::CalculateCommand cmd;
            cmd.set_id(++idCounter_);
            cmd.set_value(std::get<1>(input).value());
            std::ostringstream oss;
            oss << "Created new command " << idCounter_ << " for " << std::get<1>(input).value();
            logger_(oss.str());
            outstandingCommands_.insert(cmd.id());
            return cmd;
        } else {
            std::ostringstream oss;
            oss << "Not creating new command for " << std::get<1>(input).value() << " because the avg is " << avg_;
            logger_(oss.str());
            return std::nullopt;
        }
    }

    void handleResult(simple_demo::CalculateResult &&result) {
        std::ostringstream oss;
        oss << "Got result for " << result.id() << ": " << result.result();
        logger_(oss.str());
        if (result.result() <= 0) {
            oss.str("");
            oss << "This is the final result for " << result.id();
            logger_(oss.str());
            {
                std::lock_guard<std::mutex> _(mutex_);
                outstandingCommands_.erase(result.id());
            }
        }
    }

public:
    MainLogicImpl(std::function<void(std::string const &)> logger)
        : logger_(logger), outstandingCommands_(), mutex_(), enabled_(true), avgFactor_(0.0), avg_(0.0), avgTime_(std::nullopt), idCounter_(0) {}
    ~MainLogicImpl() = default;
    std::optional<simple_demo::CalculateCommand> runLogic(
        int which
        , std::tuple<std::chrono::system_clock::time_point, simple_demo::InputData> &&input
        , std::tuple<std::chrono::system_clock::time_point, simple_demo::CalculateResult> &&result
    ) {
        switch (which) {
        case 0:
            return handleInput(std::move(input));
        case 1:
            handleResult(std::move(std::get<1>(result)));
            return std::nullopt;
        default:
            return std::nullopt;
        }
    }
    simple_demo::ConfigureResult configure(std::tuple<std::string, simple_demo::ConfigureCommand> &&cmd) {
        std::lock_guard<std::mutex> _(mutex_);
        enabled_ = std::get<1>(cmd).enabled();
        logger_(std::string("The logic is ")+(enabled_?"enabled":"disabled")+" by '"+std::get<0>(cmd)+"'");
        simple_demo::ConfigureResult res;
        res.set_enabled(enabled_);
        return res;
    }
    simple_demo::OutstandingCommandsResult queryOutstandingCommands(simple_demo::OutstandingCommandsQuery &&query) {
        logger_("Received outstanding command query");
        simple_demo::OutstandingCommandsResult res;
        std::lock_guard<std::mutex> _(mutex_);
        for (auto id : outstandingCommands_) {
            res.add_ids(id);
        }
        return res;
    }
    simple_demo::ClearCommandsResult clearCommands(std::tuple<std::string, simple_demo::ClearCommands> &&cmd) {
        logger_(std::string("Received clear commands request from '")+std::get<0>(cmd)+"'");
        simple_demo::ClearCommandsResult res;
        std::lock_guard<std::mutex> _(mutex_);
        auto count = std::get<1>(cmd).ids_size();
        for (auto ii=0; ii<count; ++ii) {
            auto id = std::get<1>(cmd).ids(ii);
            auto iter = outstandingCommands_.find(id);
            if (iter != outstandingCommands_.end()) {
                std::ostringstream oss;
                oss << "Clearing command " << id;
                logger_(oss.str());
                outstandingCommands_.erase(iter);
                res.add_ids(id);
            } else {
                std::ostringstream oss;
                oss << id << " is not there or has already been cleared";
                logger_(oss.str());
            }
        }
        return res;
    }
};

const double MainLogicImpl::decaySpeed = std::log(0.5);

MainLogic::MainLogic(std::function<void(std::string const &)> logger)
    : impl_(std::make_unique<MainLogicImpl>(logger)) {}
MainLogic::~MainLogic() {}
MainLogic::MainLogic(MainLogic &&) = default;
MainLogic &MainLogic::operator=(MainLogic &&) = default;
std::optional<simple_demo::CalculateCommand> MainLogic::runLogic(
    int which
    , std::tuple<std::chrono::system_clock::time_point, simple_demo::InputData> &&input
    , std::tuple<std::chrono::system_clock::time_point, simple_demo::CalculateResult> &&result
) {
    return impl_->runLogic(which, std::move(input), std::move(result));
}
simple_demo::ConfigureResult MainLogic::configure(std::tuple<std::string, simple_demo::ConfigureCommand> &&cmd) {
    return impl_->configure(std::move(cmd));
}
simple_demo::OutstandingCommandsResult MainLogic::queryOutstandingCommands(simple_demo::OutstandingCommandsQuery &&query) {
    return impl_->queryOutstandingCommands(std::move(query));
}
simple_demo::ClearCommandsResult MainLogic::clearCommands(std::tuple<std::string, simple_demo::ClearCommands> &&req) {
    return impl_->clearCommands(std::move(req));
}

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

class MainLogic2Impl {
private:
    std::function<void(std::string const &)> logger_;
    std::unordered_set<int32_t> outstandingCommands_;
    std::mutex mutex_;
    bool enabled_;
    int32_t idCounter_;

    std::optional<simple_demo::CalculateCommand> handleInput(double input) {
        std::lock_guard<std::mutex> _(mutex_);            
        if (!enabled_) {
            logger_("Not enabled, not sending new command");
            return std::nullopt;
        }
        if (outstandingCommands_.size() >= 2) {
            logger_("Too many outstanding commands, not sending new command");
            return std::nullopt;
        }
        simple_demo::CalculateCommand cmd;
        cmd.set_id(++idCounter_);
        cmd.set_value(input);
        std::ostringstream oss;
        oss << "Created new command " << idCounter_ << " for " << input;
        logger_(oss.str());
        outstandingCommands_.insert(cmd.id());
        return cmd;
    }

    void handleResult(simple_demo::CalculateResult &&result) {
        std::ostringstream oss;
        oss << "Got result for " << result.id() << ": " << result.result();
        logger_(oss.str());
        if (result.result() <= 0) {
            oss.str("");
            oss << "This is the final result for " << result.id();
            logger_(oss.str());
            {
                std::lock_guard<std::mutex> _(mutex_);
                outstandingCommands_.erase(result.id());
            }
        }
    }

public:
    MainLogic2Impl(std::function<void(std::string const &)> logger)
        : logger_(logger), outstandingCommands_(), mutex_(), enabled_(true), idCounter_(0) {}
    ~MainLogic2Impl() = default;
    std::optional<simple_demo::CalculateCommand> runLogic(
        int which
        , double input
        , simple_demo::CalculateResult &&result
    ) {
        switch (which) {
        case 0:
            return handleInput(input);
        case 1:
            handleResult(std::move(result));
            return std::nullopt;
        default:
            return std::nullopt;
        }
    }
    simple_demo::ConfigureResult configure(std::tuple<std::string, simple_demo::ConfigureCommand> &&cmd) {
        std::lock_guard<std::mutex> _(mutex_);
        enabled_ = std::get<1>(cmd).enabled();
        logger_(std::string("The logic is ")+(enabled_?"enabled":"disabled")+" by '"+std::get<0>(cmd)+"'");
        simple_demo::ConfigureResult res;
        res.set_enabled(enabled_);
        return res;
    }
    simple_demo::OutstandingCommandsResult queryOutstandingCommands(simple_demo::OutstandingCommandsQuery &&query) {
        logger_("Received outstanding command query");
        simple_demo::OutstandingCommandsResult res;
        std::lock_guard<std::mutex> _(mutex_);
        for (auto id : outstandingCommands_) {
            res.add_ids(id);
        }
        return res;
    }
    simple_demo::ClearCommandsResult clearCommands(std::tuple<std::string, simple_demo::ClearCommands> &&cmd) {
        logger_(std::string("Received clear commands request from '")+std::get<0>(cmd)+"'");
        simple_demo::ClearCommandsResult res;
        std::lock_guard<std::mutex> _(mutex_);
        auto count = std::get<1>(cmd).ids_size();
        for (auto ii=0; ii<count; ++ii) {
            auto id = std::get<1>(cmd).ids(ii);
            auto iter = outstandingCommands_.find(id);
            if (iter != outstandingCommands_.end()) {
                std::ostringstream oss;
                oss << "Clearing command " << id;
                logger_(oss.str());
                outstandingCommands_.erase(iter);
                res.add_ids(id);
            } else {
                std::ostringstream oss;
                oss << id << " is not there or has already been cleared";
                logger_(oss.str());
            }
        }
        return res;
    }
};

MainLogic2::MainLogic2(std::function<void(std::string const &)> logger)
    : impl_(std::make_unique<MainLogic2Impl>(logger)) {}
MainLogic2::~MainLogic2() {}
MainLogic2::MainLogic2(MainLogic2 &&) = default;
MainLogic2 &MainLogic2::operator=(MainLogic2 &&) = default;
std::optional<simple_demo::CalculateCommand> MainLogic2::runLogic(
    int which
    , double &&input
    , simple_demo::CalculateResult &&result
) {
    return impl_->runLogic(which, input, std::move(result));
}
simple_demo::ConfigureResult MainLogic2::configure(std::tuple<std::string, simple_demo::ConfigureCommand> &&cmd) {
    return impl_->configure(std::move(cmd));
}
simple_demo::OutstandingCommandsResult MainLogic2::queryOutstandingCommands(simple_demo::OutstandingCommandsQuery &&query) {
    return impl_->queryOutstandingCommands(std::move(query));
}
simple_demo::ClearCommandsResult MainLogic2::clearCommands(std::tuple<std::string, simple_demo::ClearCommands> &&req) {
    return impl_->clearCommands(std::move(req));
}