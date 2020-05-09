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

#endif