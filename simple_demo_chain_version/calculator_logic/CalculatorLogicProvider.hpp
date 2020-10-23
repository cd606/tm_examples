#ifndef CALCULATOR_LOGIC_PROVIDER_HPP_
#define CALCULATOR_LOGIC_PROVIDER_HPP_

#include "simple_demo_chain_version/calculator_logic/CalculatorStateFolder.hpp"
#include "simple_demo_chain_version/calculator_logic/CalculatorIdleWorker.hpp"
#include "simple_demo_chain_version/calculator_logic/CalculatorFacilityInputHandler.hpp"

#include <tm_kit/basic/simple_shared_chain/ChainWriter.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

#include <iostream>
#include <sstream>

namespace simple_demo_chain_version { namespace calculator_logic {

    template <class R>
    struct CalculatorLogicProviderResult {
        typename R::template Source<ChainData> chainDataGeneratedFromCalculator;
    };

    template <class R, class Chain>
    CalculatorLogicProviderResult<R> calculatorLogicMain(
        R &r 
        , Chain *chain
        , typename R:: template FacilitioidConnector<ExternalCalculatorInput,ExternalCalculatorOutput> wrappedExternalCalculator
        , std::string const &graphPrefix
    ) {
        using TheEnvironment = typename R::EnvironmentType;
        using M = typename R::AppType;
        auto *env = r.environment();

        //Now we define the chain worker

        using Writer = basic::simple_shared_chain::ChainWriter<
            M, Chain
            , CalculatorStateFolder<TheEnvironment,Chain>
            , CalculatorFacilityInputHandler<TheEnvironment,Chain>
            , CalculatorIdleWorker<TheEnvironment,Chain>
        >;

        //inputs to the external calculator (requests) are coming from the chain
        //(through the importer part of the ChainWriter), so we feed it to the 
        //external calculator through an action

        using U = typename CalculatorIdleWorker<TheEnvironment,Chain>::OffChainUpdateType;

        auto sendCommandAction = M::template liftMulti<U>(
            [env](U &&u) -> std::vector<ExternalCalculatorInput> {
                std::vector<ExternalCalculatorInput> ret;
                std::visit([env,&u,&ret](auto &&update) {
                    using T = std::decay_t<decltype(update)>;
                    if constexpr (std::is_same_v<T, ConfirmRequestReceipt>) {
                        for (auto const &item : u.valueRef) {
                            ExternalCalculatorInput c {item.first, item.second};
                            std::ostringstream oss;
                            oss << "Sent external request {id=" << c.id << ", input=" << c.input << "}";
                            env->log(infra::LogLevel::Info, oss.str());
                            ret.push_back(std::move(c));
                        }
                    }
                }, std::move(u.action.update));
                return ret;
            }
        );
        r.registerAction(graphPrefix+"/sendCommand", sendCommandAction);

        auto keyifyForExternalCalcFacility = infra::KleisliUtils<M>::action(
            basic::CommonFlowUtilComponents<M>::template keyify<ExternalCalculatorInput>()
        );
        r.registerAction(graphPrefix+"/keyifyForExternalCalcFacility", keyifyForExternalCalcFacility);

        auto extractFacilityOutputFromExternalCalcFacility = infra::KleisliUtils<M>::action(
            basic::CommonFlowUtilComponents<M>::template extractDataFromKeyedData<ExternalCalculatorInput,ExternalCalculatorOutput>()
        );
        r.registerAction(graphPrefix+"/extractFacilityOutputFromExternalCalcFacility", extractFacilityOutputFromExternalCalcFacility);

        wrappedExternalCalculator(
            r
            , r.execute(keyifyForExternalCalcFacility, r.actionAsSource(sendCommandAction))
            , r.actionAsSink(extractFacilityOutputFromExternalCalcFacility)
        );

        //now we create the main chain worker and add the facility connectors

        //If very high throughput is required, then we need to use the busy-loop no-yield polling 
        //policy which will occupy full CPU (in real-time mode). If default polling policy is used
        //, then there will be a sleep of at least 1 millisecond (and most likely longer) between 
        //the polling, so the throughput will be degraded. In single-pass mode, the polling policy
        //is ignored since it is single-threaded and always uses busy polling.
        //auto chainFacility = Writer::onOrderFacilityWithExternalEffects(chain, basic::simple_shared_chain::ChainPollingPolicy().BusyLoop(true).NoYield(true));
        auto chainFacility = Writer::onOrderFacilityWithExternalEffects(chain);
        r.registerOnOrderFacilityWithExternalEffects(graphPrefix+"/chainFacility", chainFacility);

        auto keyifyForChainFacility = infra::KleisliUtils<M>::action(
            basic::CommonFlowUtilComponents<M>::template keyify<ExternalCalculatorOutput>()
        );
        r.registerAction(graphPrefix+"/keyifyForChainFacility", keyifyForChainFacility);

        auto extractFacilityOutputFromChainFacility = infra::KleisliUtils<M>::action(
            basic::CommonFlowUtilComponents<M>::template extractDataFromKeyedData<ExternalCalculatorOutput,ChainData>()
        );
        r.registerAction(graphPrefix+"/extractFacilityOutputFromChainFacility", extractFacilityOutputFromChainFacility);

        r.placeOrderWithFacilityWithExternalEffects(
            r.actionAsSource(keyifyForChainFacility)
            , chainFacility
            , r.actionAsSink(extractFacilityOutputFromChainFacility)
        );

        //now we connect the facilities together

        r.execute(keyifyForChainFacility, r.actionAsSource(extractFacilityOutputFromExternalCalcFacility));
        r.execute(sendCommandAction, r.facilityWithExternalEffectsAsSource(chainFacility));

        //now we create a combiner to combine the two ChainData outputs
        auto combiner = infra::KleisliUtils<M>::action(
            basic::CommonFlowUtilComponents<M>::template idFunc<ChainData>()
        );
        r.registerAction(graphPrefix+"/combiner", combiner);
        auto takeChainDataForPrint = M::template liftPure<U>(
            [](U &&u) -> ChainData {
                return std::move(u.action);
            }
        );
        r.registerAction(graphPrefix+"/takeChainDataForPrint", takeChainDataForPrint);
        r.execute(combiner, r.actionAsSource(extractFacilityOutputFromChainFacility));
        r.execute(combiner, r.execute(takeChainDataForPrint, r.facilityWithExternalEffectsAsSource(chainFacility)));

        //return the combiner output

        return {
            r.actionAsSource(combiner)
        };
    }

} }

#endif