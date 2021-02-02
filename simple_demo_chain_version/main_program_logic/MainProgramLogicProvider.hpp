#ifndef MAIN_PROGRAM_LOGIC_PROVIDER_HPP_
#define MAIN_PROGRAM_LOGIC_PROVIDER_HPP_

#include "simple_demo_chain_version/main_program_logic/MainProgramStateFolder.hpp"
#include "simple_demo_chain_version/main_program_logic/MainProgramFacilityInputHandler.hpp"
#include "simple_demo_chain_version/main_program_logic/OperationLogic.hpp"
#include "simple_demo_chain_version/main_program_logic/ProgressReporter.hpp"
#include "simple_demo_chain_version/main_program_logic/MainProgramChainDataReader.hpp"
#include "simple_demo_chain_version/main_program_logic/MainProgramIDAndFinalFlagExtractor.hpp"
#include "defs.pb.h"

#include <tm_kit/basic/simple_shared_chain/ChainWriter.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>
#include <tm_kit/basic/simple_shared_chain/ChainBackedFacility.hpp>

#include <boost/hana/functional/curry.hpp>

#include <iostream>
#include <sstream>
#include <cmath>

namespace simple_demo_chain_version { namespace main_program_logic {

    template <class R, template <class M> class ChainCreator>
    std::tuple<
        typename R::template FacilitioidConnector<double, std::optional<ChainData>>
        , std::string
    >
    chainBasedRequestHandler(
        R &r 
        , ChainCreator<typename R::AppType> &chainCreator
        , std::string const &chainLocatorStr
        , std::string const &graphPrefix
    ) {
        auto res = basic::simple_shared_chain::createChainBackedFacility<
            R 
            , ChainData
            , MainProgramStateFolder
            , MainProgramFacilityInputHandler<typename R::EnvironmentType>
            , MainProgramIDAndFinalFlagExtractor<typename R::EnvironmentType>
        >(
            r 
            , chainCreator.template writerFactory<
                ChainData
                , MainProgramStateFolder
                , MainProgramFacilityInputHandler<typename R::EnvironmentType>
            >(
                r.environment()
                , chainLocatorStr
            )
            , chainCreator.template readerFactory<
                ChainData
                , TrivialChainDataFolder
            >(
                r.environment()
                , chainLocatorStr
            )
            , std::make_shared<MainProgramIDAndFinalFlagExtractor<typename R::EnvironmentType>>()
            , graphPrefix+"/facility_combo"
        );
        return {res.facility, res.registeredNameForFacilitioidConnector};
    }

    template <class R>
    void mainProgramLogicMain(
        R &r 
        , typename R::template FacilitioidConnector<
            double, std::optional<ChainData>
        > requestHandler
        , typename R::template ConvertibleToSourceoid<InputData> &&dataSource
        , std::optional<typename R::template Source<bool>> const &enabledSource
        , std::string const &graphPrefix
        , std::function<void(bool)> const &statusUpdater = [](bool x) {}
    ) {
        using M = typename R::AppType;
        auto *env = r.environment();

        auto operationLogicPtr = std::make_shared<OperationLogic>(
            [env](std::string const &s) {
                env->log(infra::LogLevel::Info, s);
            }
            , statusUpdater
        );
        r.preservePointer(operationLogicPtr);

        auto extractDouble = infra::KleisliUtils<M>
            ::template liftPure<InputData>(
            [](InputData &&d) -> double {
                return d.value();
            }
        );
        auto duplicator = basic::CommonFlowUtilComponents<M>
            ::template duplicateInput<double>();
        auto exponentialAverage = basic::CommonFlowUtilComponents<M>
            ::template wholeHistoryFold<double>(
            ExponentialAverage(std::log(0.5))
        );
        auto exponentialAverageWithInputAttached = basic::CommonFlowUtilComponents<M>
            ::template preserveLeft<double, double>(std::move(exponentialAverage));
        auto filterValue = basic::CommonFlowUtilComponents<M>
            ::template pureFilter<std::tuple<double, double>>(
            [](std::tuple<double,double> const &input) -> bool {
                return std::get<0>(input) >= 1.05*std::get<1>(input);
            }
        );
        auto assembleOperationLogicInput = basic::CommonFlowUtilComponents<M>
            ::template dropRight<double,double>();
        auto upToOperationLogicInputKleisli =
            infra::KleisliUtils<M>::template compose<InputData>(
                std::move(extractDouble)
                , infra::KleisliUtils<M>::template compose<double>(
                    std::move(duplicator)
                    , infra::KleisliUtils<M>::template compose<std::tuple<double,double>>(
                        std::move(exponentialAverageWithInputAttached)
                        , infra::KleisliUtils<M>::template compose<std::tuple<double,double>>(
                            std::move(filterValue)
                            , std::move(assembleOperationLogicInput)
                        )
                    )
                )
            );
        auto upToOperationLogicInput = M::template kleisli<InputData>(std::move(upToOperationLogicInputKleisli));
        auto logic = M::template liftMaybe<double>(
            boost::hana::curry<2>(std::mem_fn(&OperationLogic::runLogic))(operationLogicPtr.get())
            , infra::LiftParameters<std::chrono::system_clock::time_point>()
                .DelaySimulator(
                    [](int , std::chrono::system_clock::time_point const &) -> std::chrono::system_clock::duration {
                        return std::chrono::milliseconds(100);
                    }
                )
        );
        r.registerAction(graphPrefix+"/preprocess", upToOperationLogicInput);
        r.registerAction(graphPrefix+"/operation_logic", logic);

        auto keyify = infra::KleisliUtils<M>::action(
            basic::CommonFlowUtilComponents<M>::template keyify<double>()
        );
        auto extractIDAndData = infra::KleisliUtils<M>::action(
            basic::CommonFlowUtilComponents<M>::template extractIDStringAndDataFromKeyedData<double, std::optional<ChainData>>()
        );

        r.convertToSourceoid(std::move(dataSource))(r, r.actionAsSink(upToOperationLogicInput));
        requestHandler(
            r
            , r.execute(graphPrefix+"/keyify", keyify, 
                r.execute(logic, r.actionAsSource(upToOperationLogicInput))
            )
            , r.actionAsSink(graphPrefix+"/extractIDAndData", extractIDAndData)
        );

        if (enabledSource) {
            auto enabledSetter = M::template pureExporter<bool>(
                [operationLogicPtr](bool &&x) {
                    operationLogicPtr->setEnabled(x);
                }
            );
            r.registerExporter(graphPrefix+"/setEnabled", enabledSetter);
            r.exportItem(enabledSetter, enabledSource->clone());
        }

        auto progressReporter = M::template liftMulti<
            std::tuple<std::string, std::optional<ChainData>>
        >(&ProgressReporter::reportProgress);
        r.registerAction(graphPrefix+"/progressReporter", progressReporter);

        r.execute(progressReporter, r.actionAsSource(extractIDAndData));

        auto printExporter = M::template pureExporter<std::string>(
            [env](std::string &&s) {
                env->log(infra::LogLevel::Info, s);
            }
        );
        r.registerExporter(graphPrefix+"/printExporter", printExporter);
        r.exportItem(printExporter, r.actionAsSource(progressReporter));
    }

} }

#endif