#ifndef MAIN_LOGIC_COMBINATION_HPP_
#define MAIN_LOGIC_COMBINATION_HPP_

#include <simple_demo/program_logic/MainLogic.hpp>
#include <boost/hana/functional/curry.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>
#include <tm_kit/infra/KleisliSequence.hpp>
#include "defs.pb.h"

template <class R>
struct MainLogicInput {
    using M = typename R::AppType;
    typename R::template FacilitioidConnector<simple_demo::CalculateCommand, simple_demo::CalculateResult> commandConnector;
    typename R::template FacilityWrapper<std::tuple<std::string, simple_demo::ConfigureCommand>, simple_demo::ConfigureResult> configWrapper;
    typename R::template FacilityWrapper<simple_demo::OutstandingCommandsQuery, simple_demo::OutstandingCommandsResult> queryWrapper;
    typename R::template FacilityWrapper<std::tuple<std::string, simple_demo::ClearCommands>, simple_demo::ClearCommandsResult> clearCommandsWrapper;
};

template <class R>
struct MainLogicOutput {
    using M = typename R::AppType;
    typename R::template Sink<simple_demo::InputData> dataSink;
};

enum class LogicChoice {
    One, Two
};

template <class R>
inline MainLogicOutput<R> MainLogicCombination(R &r, typename R::EnvironmentType &env, MainLogicInput<R> &&input, LogicChoice logicChoice, std::string const &alertTopic="") {
    using M = typename R::AppType;
    using namespace std::placeholders;
    using namespace dev::cd606::tm;

    auto statusUpdaterUsingHeartbeatAndAlert = [&env,alertTopic](bool enabled) {
        if constexpr (std::is_convertible_v<typename R::EnvironmentType *, transport::HeartbeatAndAlertComponent *>) {
            if (enabled) {
                env.setStatus("calculation_status", transport::HeartbeatMessage::Status::Good, "enabled");
                if (alertTopic != "") {
                    env.sendAlert(alertTopic, infra::LogLevel::Info, "main logic calculation enabled");
                }
            } else {
                env.setStatus("calculation_status", transport::HeartbeatMessage::Status::Warning, "disabled");
                if (alertTopic != "") {
                    env.sendAlert(alertTopic, infra::LogLevel::Warning, "main logic calculation disabled");
                }
            }
        }
    };

    if (logicChoice == LogicChoice::One) {
        auto mainLogicPtr = std::make_shared<MainLogic>(
            [&env](std::string const &s) {
                env.log(dev::cd606::tm::infra::LogLevel::Info, s);
            }
            , statusUpdaterUsingHeartbeatAndAlert
        );
        r.preservePointer(mainLogicPtr);

        auto logic = M::template enhancedMaybe2<
                simple_demo::InputData
                , simple_demo::CalculateResult
            >(
                boost::hana::curry<2>(std::mem_fn(&MainLogic::runLogic))(mainLogicPtr.get())
                , dev::cd606::tm::infra::LiftParameters<std::chrono::system_clock::time_point>()
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
        auto cmd = r.actionAsSource("logic", logic);

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

        return MainLogicOutput<R> {
            r.actionAsSink_2_0(logic)
        };

    } else if (logicChoice == LogicChoice::Two) {
        auto mainLogicPtr = std::make_shared<MainLogic2>(
            [&env](std::string const &s) {
                env.log(dev::cd606::tm::infra::LogLevel::Info, s);
            }
            , statusUpdaterUsingHeartbeatAndAlert
        );
        r.preservePointer(mainLogicPtr);

        auto duplicator = dev::cd606::tm::basic::CommonFlowUtilComponents<M>
            ::template duplicateInput<double>();
        auto exponentialAverage = dev::cd606::tm::basic::CommonFlowUtilComponents<M>
            ::template wholeHistoryFold<double>(
            ExponentialAverage(std::log(0.5))
        );
        auto exponentialAverageWithInputAttached = dev::cd606::tm::basic::CommonFlowUtilComponents<M>
            ::template preserveLeft<double, double>(std::move(exponentialAverage));
        auto filterValue = dev::cd606::tm::basic::CommonFlowUtilComponents<M>
            ::template pureFilter<std::tuple<double, double>>(
            [](std::tuple<double,double> const &input) -> bool {
                return std::get<0>(input) >= 1.05*std::get<1>(input);
            }
        );
        auto assembleMainLogicInput = dev::cd606::tm::basic::CommonFlowUtilComponents<M>
            ::template dropRight<double,double>();

        auto upToMainLogicInputKleisli = dev::cd606::tm::infra::KleisliSequence<M>
            ::seq(
                [](simple_demo::InputData &&d) -> double {
                    return d.value();
                }
                , duplicator 
                , exponentialAverageWithInputAttached
                , filterValue
                , assembleMainLogicInput
            );

        /*
        auto extractDouble = dev::cd606::tm::infra::KleisliUtils<M>
            ::template liftPure<simple_demo::InputData>(
            [](simple_demo::InputData &&d) -> double {
                return d.value();
            }
        );
        
        auto upToMainLogicInputKleisli =
            dev::cd606::tm::infra::KleisliUtils<M>::template compose<simple_demo::InputData>(
                std::move(extractDouble)
                , dev::cd606::tm::infra::KleisliUtils<M>::template compose<double>(
                    std::move(duplicator)
                    , dev::cd606::tm::infra::KleisliUtils<M>::template compose<std::tuple<double,double>>(
                        std::move(exponentialAverageWithInputAttached)
                        , dev::cd606::tm::infra::KleisliUtils<M>::template compose<std::tuple<double,double>>(
                            std::move(filterValue)
                            , std::move(assembleMainLogicInput)
                        )
                    )
                )
            );
        */
        using GL = dev::cd606::tm::infra::GenericLift<M>;
        auto upToMainLogicInput = GL::lift(std::move(upToMainLogicInputKleisli));
        //GenericLift does not work with boost::hana::curry because 
        //the generated operator() in boost::hana::curry objects are
        //overloaded
        auto logic = M::template liftMaybe2<
                double
                , simple_demo::CalculateResult
            >(
                boost::hana::curry<2>(std::mem_fn(&MainLogic2::runLogic))(mainLogicPtr.get())
                , dev::cd606::tm::infra::LiftParameters<std::chrono::system_clock::time_point>()
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
        auto cmd = r.execute("logic", logic, r.actionAsSource("upToMainLogicInput", upToMainLogicInput));

        auto extractResult = GL::lift(
            [](typename M::template KeyedData<
                simple_demo::CalculateCommand
                , simple_demo::CalculateResult
            > &&result) -> simple_demo::CalculateResult {
                return std::move(result.data);
            }
        );
        r.execute(logic, r.actionAsSource("extractResult", extractResult));
        auto keyify = GL::lift(dev::cd606::tm::infra::withtime_utils::keyify<simple_demo::CalculateCommand, typename M::EnvironmentType>);
        input.commandConnector(
            r,
            r.execute("keyify", keyify, std::move(cmd)),
            r.actionAsSink(extractResult)
        );
        //The following lifts also cannot be done with GL::lift
        //because overloaded operator() in boost::hana::curry is involved
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
        
        return MainLogicOutput<R> {
            r.actionAsSink(upToMainLogicInput)
        };
    } else {
        throw "Invalid logic choice";
    }
    
}

#endif