#ifndef MAIN_LOGIC_HPP_
#define MAIN_LOGIC_HPP_

#include <optional>
#include <memory>
#include <functional>
#include <chrono>
#include <variant>
#include "defs.pb.h"
#include "simple_demo/data_structures/ConfigureStructures.hpp"

class MainLogicImpl;

class MainLogic {
private:
    std::unique_ptr<MainLogicImpl> impl_;
public:
    MainLogic(std::function<void(std::string const &)> logger, std::function<void(bool)> statusUpdater);
    ~MainLogic();
    MainLogic(MainLogic const &) = delete;
    MainLogic &operator=(MainLogic const &) = delete;
    MainLogic(MainLogic &&);
    MainLogic &operator=(MainLogic &&);
    std::optional<simple_demo::CalculateCommand> runLogic(
        std::tuple<std::chrono::system_clock::time_point, std::variant<simple_demo::InputData, simple_demo::CalculateResult>> &&input
    );
    simple_demo::ConfigureResultPOCO configure(std::tuple<std::string, simple_demo::ConfigureCommandPOCO> &&);
    simple_demo::OutstandingCommandsResult queryOutstandingCommands(simple_demo::OutstandingCommandsQuery &&);
    simple_demo::ClearCommandsResult clearCommands(std::tuple<std::string, simple_demo::ClearCommands> &&);
};

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

class MainLogic2Impl;

class MainLogic2 {
private:
    std::unique_ptr<MainLogic2Impl> impl_;
public:
    MainLogic2(std::function<void(std::string const &)> logger, std::function<void(bool)> statusUpdater);
    ~MainLogic2();
    MainLogic2(MainLogic2 const &) = delete;
    MainLogic2 &operator=(MainLogic2 const &) = delete;
    MainLogic2(MainLogic2 &&);
    MainLogic2 &operator=(MainLogic2 &&);
    std::optional<simple_demo::CalculateCommand> runLogic(
        std::variant<double, simple_demo::CalculateResult> &&input
    );
    simple_demo::ConfigureResultPOCO configure(std::tuple<std::string, simple_demo::ConfigureCommandPOCO> &&);
    simple_demo::OutstandingCommandsResult queryOutstandingCommands(simple_demo::OutstandingCommandsQuery &&);
    simple_demo::ClearCommandsResult clearCommands(std::tuple<std::string, simple_demo::ClearCommands> &&);
};

#endif
