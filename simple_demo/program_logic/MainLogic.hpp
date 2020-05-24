#ifndef MAIN_LOGIC_HPP_
#define MAIN_LOGIC_HPP_

#include <optional>
#include <memory>
#include <functional>
#include "defs.pb.h"

class MainLogicImpl;

class MainLogic {
private:
    std::unique_ptr<MainLogicImpl> impl_;
public:
    MainLogic(std::function<void(std::string const &)> logger);
    ~MainLogic();
    MainLogic(MainLogic const &) = delete;
    MainLogic &operator=(MainLogic const &) = delete;
    MainLogic(MainLogic &&);
    MainLogic &operator=(MainLogic &&);
    std::optional<simple_demo::CalculateCommand> runLogic(
        int which
        , std::tuple<std::chrono::system_clock::time_point, simple_demo::InputData> &&input
        , std::tuple<std::chrono::system_clock::time_point, simple_demo::CalculateResult> &&result
    );
    simple_demo::ConfigureResult configure(std::tuple<std::string, simple_demo::ConfigureCommand> &&);
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
    MainLogic2(std::function<void(std::string const &)> logger);
    ~MainLogic2();
    MainLogic2(MainLogic2 const &) = delete;
    MainLogic2 &operator=(MainLogic2 const &) = delete;
    MainLogic2(MainLogic2 &&);
    MainLogic2 &operator=(MainLogic2 &&);
    std::optional<simple_demo::CalculateCommand> runLogic(
        int which
        , double &&input
        , simple_demo::CalculateResult &&result
    );
    simple_demo::ConfigureResult configure(std::tuple<std::string, simple_demo::ConfigureCommand> &&);
    simple_demo::OutstandingCommandsResult queryOutstandingCommands(simple_demo::OutstandingCommandsQuery &&);
    simple_demo::ClearCommandsResult clearCommands(std::tuple<std::string, simple_demo::ClearCommands> &&);
};

#endif