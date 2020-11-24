#ifndef MAIN_PROGRAM_OPERATION_LOGIC_HPP_
#define MAIN_PROGRAM_OPERATION_LOGIC_HPP_

#include <optional>
#include <memory>
#include <functional>
#include <chrono>

namespace simple_demo_chain_version { namespace main_program_logic {

    class ExponentialAverageImpl;

    class ExponentialAverage {
    private:
        std::unique_ptr<ExponentialAverageImpl> impl_;
    public:
        ExponentialAverage(double decaySpeed);
        ~ExponentialAverage();
        ExponentialAverage(ExponentialAverage &&);
        ExponentialAverage &operator=(ExponentialAverage &&);
        void add(std::tuple<std::chrono::system_clock::time_point, double> &&);
        std::optional<double> readResult() const;
    };

    class OperationLogicImpl;

    class OperationLogic {
    private:
        std::unique_ptr<OperationLogicImpl> impl_;
    public:
        OperationLogic(std::function<void(std::string const &)> logger, std::function<void(bool)> statusUpdater);
        ~OperationLogic();
        OperationLogic(OperationLogic const &) = delete;
        OperationLogic &operator=(OperationLogic const &) = delete;
        OperationLogic(OperationLogic &&);
        OperationLogic &operator=(OperationLogic &&);
        std::optional<double> runLogic(double &&input);
        void setEnabled(bool enabled);
    };

} }

#endif
