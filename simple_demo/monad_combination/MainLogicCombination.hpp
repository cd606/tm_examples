#ifndef MAIN_LOGIC_COMBINATION_HPP_
#define MAIN_LOGIC_COMBINATION_HPP_

#include <simple_demo/program_logic/MainLogic.hpp>
#include <boost/hana/functional/curry.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>
#include "defs.pb.h"

template <class R>
struct MainLogicInput {
    using M = typename R::MonadType;
    typename R::template Source<simple_demo::InputData> dataSource;
    typename R::template FacilitioidConnector<simple_demo::CalculateCommand, simple_demo::CalculateResult> commandConnector;
    typename R::template FacilityWrapper<std::tuple<std::string, simple_demo::ConfigureCommand>, simple_demo::ConfigureResult> configWrapper;
    typename R::template FacilityWrapper<simple_demo::OutstandingCommandsQuery, simple_demo::OutstandingCommandsResult> queryWrapper;
    typename R::template FacilityWrapper<std::tuple<std::string, simple_demo::ClearCommands>, simple_demo::ClearCommandsResult> clearCommandsWrapper;
};

enum class LogicChoice {
    One, Two
};

template <class R>
inline void MainLogicCombination(R &r, typename R::EnvironmentType &env, MainLogicInput<R> &&input, LogicChoice logicChoice) {
    using M = typename R::MonadType;
    using namespace std::placeholders;

    if (logicChoice == LogicChoice::One) {
        auto mainLogicPtr = std::make_shared<MainLogic>(
            [&env](std::string const &s) {
                env.log(dev::cd606::tm::infra::LogLevel::Info, s);
            }
        );
        r.preservePointer(mainLogicPtr);

        auto logic = M::template enhancedMaybe2<
                simple_demo::InputData
                , simple_demo::CalculateResult
            >(
                boost::hana::curry<4>(std::mem_fn(&MainLogic::runLogic))(mainLogicPtr.get())
                , dev::cd606::tm::infra::LiftParameters<std::chrono::system_clock::time_point>()
                    .RequireMask(dev::cd606::tm::infra::FanInParamMask("01")) //only the first input (InputData) is required
                    .DelaySimulator(
                        [](int which, std::chrono::system_clock::time_point const &) -> std::chrono::system_clock::duration {
                            if (which == 0) {
                                return std::chrono::milliseconds(100);
                            } else {
                                return std::chrono::milliseconds(0);
                            }
                        }
                    )
            );
        auto cmd = r.execute("logic", logic, std::move(input.dataSource));

        auto extractResult = M::template liftPure<
                typename M::template KeyedData<
                    simple_demo::CalculateCommand
                    , simple_demo::CalculateResult
                >
            >(
            [](typename M::template KeyedData<
                simple_demo::CalculateCommand
                , simple_demo::CalculateResult
            > &&result) -> simple_demo::CalculateResult {
                return std::move(result.data);
            }
        );
        r.execute(logic, r.actionAsSource("extractResult", extractResult));
        auto keyify = M::template liftPure<simple_demo::CalculateCommand>(dev::cd606::tm::infra::withtime_utils::keyify<simple_demo::CalculateCommand, typename M::EnvironmentType>);
        input.commandConnector(
            r,
            r.execute("keyify", keyify, std::move(cmd)),
            r.actionAsSink(extractResult)
        );
        if (input.configWrapper) {
            auto cfgFacility = M::template liftPureOnOrderFacility<
                std::tuple<std::string, simple_demo::ConfigureCommand>
            >(
                boost::hana::curry<2>(std::mem_fn(&MainLogic::configure))(mainLogicPtr.get())
            );
            r.registerOnOrderFacility("cfgFacility", cfgFacility);
            (*input.configWrapper)(r, cfgFacility);
            r.markStateSharing(cfgFacility, logic, "enabled");
        }
        if (input.queryWrapper) {
            auto queryFacility = M::template liftPureOnOrderFacility<
                simple_demo::OutstandingCommandsQuery
            >(
                boost::hana::curry<2>(std::mem_fn(&MainLogic::queryOutstandingCommands))(mainLogicPtr.get())
            );
            r.registerOnOrderFacility("queryFacility", queryFacility);
            (*input.queryWrapper)(r, queryFacility);
            r.markStateSharing(queryFacility, logic, "outstanding_cmds");
        }
        if (input.clearCommandsWrapper) {
            auto clearCommandsFacility = M::template liftPureOnOrderFacility<
                std::tuple<std::string, simple_demo::ClearCommands>
            >(
                boost::hana::curry<2>(std::mem_fn(&MainLogic::clearCommands))(mainLogicPtr.get())
            );
            r.registerOnOrderFacility("clearCommandsFacility", clearCommandsFacility);
            (*input.clearCommandsWrapper)(r, clearCommandsFacility);
            r.markStateSharing(clearCommandsFacility, logic, "outstanding_cmds");
        }

    } else if (logicChoice == LogicChoice::Two) {
        auto mainLogicPtr = std::make_shared<MainLogic2>(
            [&env](std::string const &s) {
                env.log(dev::cd606::tm::infra::LogLevel::Info, s);
            }
        );
        r.preservePointer(mainLogicPtr);

        auto extractDouble = M::template liftPure<simple_demo::InputData>(
            [](simple_demo::InputData &&d) -> double {
                return d.value();
            }
        );
        auto exponentialAverage = dev::cd606::tm::basic::CommonFlowUtilComponents<M>
            ::template wholeHistoryActionWithInputAttached<double>(
            ExponentialAverage(std::log(0.5))
        );
        auto assembleMainLogicInput = M::template liftPure<std::tuple<double,double>>(
            [](std::tuple<double,double> &&x) -> MainLogic2::MainLogicInput {
                return {std::get<0>(x), std::get<1>(x)};
            }
        );
        auto logic = M::template liftMaybe2<
                MainLogic2::MainLogicInput
                , simple_demo::CalculateResult
            >(
                boost::hana::curry<4>(std::mem_fn(&MainLogic2::runLogic))(mainLogicPtr.get())
                , dev::cd606::tm::infra::LiftParameters<std::chrono::system_clock::time_point>()
                    .RequireMask(dev::cd606::tm::infra::FanInParamMask("01")) //only the first input (InputData) is required
                    .DelaySimulator(
                        [](int which, std::chrono::system_clock::time_point const &) -> std::chrono::system_clock::duration {
                            if (which == 0) {
                                return std::chrono::milliseconds(100);
                            } else {
                                return std::chrono::milliseconds(0);
                            }
                        }
                    )
            );
        auto cmd = r.execute("logic", logic, 
                    r.execute("assemble", assembleMainLogicInput, 
                        r.execute("exponentialAvg", exponentialAverage, 
                            r.execute("extractDouble", extractDouble, std::move(input.dataSource)))));

        auto extractResult = M::template liftPure<
                typename M::template KeyedData<
                    simple_demo::CalculateCommand
                    , simple_demo::CalculateResult
                >
            >(
            [](typename M::template KeyedData<
                simple_demo::CalculateCommand
                , simple_demo::CalculateResult
            > &&result) -> simple_demo::CalculateResult {
                return std::move(result.data);
            }
        );
        r.execute(logic, r.actionAsSource("extractResult", extractResult));
        auto keyify = M::template liftPure<simple_demo::CalculateCommand>(dev::cd606::tm::infra::withtime_utils::keyify<simple_demo::CalculateCommand, typename M::EnvironmentType>);
        input.commandConnector(
            r,
            r.execute("keyify", keyify, std::move(cmd)),
            r.actionAsSink(extractResult)
        );
        if (input.configWrapper) {
            auto cfgFacility = M::template liftPureOnOrderFacility<
                std::tuple<std::string, simple_demo::ConfigureCommand>
            >(
                boost::hana::curry<2>(std::mem_fn(&MainLogic2::configure))(mainLogicPtr.get())
            );
            r.registerOnOrderFacility("cfgFacility", cfgFacility);
            (*input.configWrapper)(r, cfgFacility);
            r.markStateSharing(cfgFacility, logic, "enabled");
        }
        if (input.queryWrapper) {
            auto queryFacility = M::template liftPureOnOrderFacility<
                simple_demo::OutstandingCommandsQuery
            >(
                boost::hana::curry<2>(std::mem_fn(&MainLogic2::queryOutstandingCommands))(mainLogicPtr.get())
            );
            r.registerOnOrderFacility("queryFacility", queryFacility);
            (*input.queryWrapper)(r, queryFacility);
            r.markStateSharing(queryFacility, logic, "outstanding_cmds");
        }
        if (input.clearCommandsWrapper) {
            auto clearCommandsFacility = M::template liftPureOnOrderFacility<
                std::tuple<std::string, simple_demo::ClearCommands>
            >(
                boost::hana::curry<2>(std::mem_fn(&MainLogic2::clearCommands))(mainLogicPtr.get())
            );
            r.registerOnOrderFacility("clearCommandsFacility", clearCommandsFacility);
            (*input.clearCommandsWrapper)(r, clearCommandsFacility);
            r.markStateSharing(clearCommandsFacility, logic, "outstanding_cmds");
        }
    }

    
}

#endif