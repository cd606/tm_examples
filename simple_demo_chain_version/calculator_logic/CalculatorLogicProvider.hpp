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
        , ExternalCalculator *externalCalc 
        , Chain *chain
        , std::string const &graphPrefix
    ) {
        using TheEnvironment = typename R::EnvironmentType;
        using M = typename R::AppType;
        auto *env = r.environment();

        //we use a trigger importer to bring the external calculator's output 
        //into the graph
        
        class ExternalCallback : public CalculateResultListener {
        private:
            std::function<void(ExternalCalculatorOutput &&)> callback_;
        public:
            ExternalCallback(std::function<void(ExternalCalculatorOutput &&)> callback) : callback_(callback) {}
            virtual ~ExternalCallback() {}
            virtual void onCalculateResult(ExternalCalculatorOutput const &o) override final {
                ExternalCalculatorOutput copy = o;
                callback_(std::move(copy));
            }
        };

        auto calculateResultImporterPair = M::template triggerImporter<ExternalCalculatorOutput>();
        r.registerImporter(graphPrefix+"/calculateResultImporter", std::get<0>(calculateResultImporterPair));
        //Since the callback is created inside this function call, we need to 
        //preserve its pointer inside the runner
        auto cb = std::make_shared<ExternalCallback>(std::get<1>(calculateResultImporterPair));
        r.preservePointer(cb);

        externalCalc->start(cb.get());

        //Now we define the chain worker

        using Writer = basic::simple_shared_chain::ChainWriter<
            M, Chain
            , CalculatorStateFolder<TheEnvironment,Chain>
            , CalculatorFacilityInputHandler<TheEnvironment,Chain>
            , CalculatorIdleWorker<TheEnvironment,Chain>
        >;

        //inputs to the external calculator (requests) are coming from the chain
        //(through the importer part of the ChainWriter), so we feed it to the 
        //external calculator through an exporter

        using U = typename CalculatorIdleWorker<TheEnvironment,Chain>::OffChainUpdateType;

        auto sendCommandExporter = M::template pureExporter<U>(
            [externalCalc,env](U &&u) {
                std::visit([externalCalc,env,&u](auto &&update) {
                    using T = std::decay_t<decltype(update)>;
                    if constexpr (std::is_same_v<T, ConfirmRequestReceipt>) {
                        for (auto const &item : u.valueRef) {
                            ExternalCalculatorInput c {item.first, item.second};
                            externalCalc->request(c);
                            std::ostringstream oss;
                            oss << "Sent external request {id=" << c.id << ", input=" << c.input << "}";
                            env->log(infra::LogLevel::Info, oss.str());
                        }
                    }
                }, std::move(u.action.update));
            }
        );
        r.registerExporter(graphPrefix+"/sendCommandExporter", sendCommandExporter);

        //now we create the main chain worker and add the facility connectors

        auto facility = Writer::onOrderFacilityWithExternalEffects(chain);
        r.registerOnOrderFacilityWithExternalEffects(graphPrefix+"/chainFacility", facility);

        auto keyify = infra::KleisliUtils<M>::action(
            basic::CommonFlowUtilComponents<M>::template keyify<ExternalCalculatorOutput>()
        );
        r.registerAction(graphPrefix+"/keyify", keyify);

        auto extractFacilityOutput = infra::KleisliUtils<M>::action(
            basic::CommonFlowUtilComponents<M>::template extractDataFromKeyedData<ExternalCalculatorOutput,ChainData>()
        );
        r.registerAction(graphPrefix+"/extractFacilityOutput", extractFacilityOutput);

        r.placeOrderWithFacilityWithExternalEffects(
            r.actionAsSource(keyify)
            , facility
            , r.actionAsSink(extractFacilityOutput)
        );

        //now we connect the importers and exporters to the main chain worker

        r.execute(keyify, r.importItem(std::get<0>(calculateResultImporterPair)));
        r.exportItem(sendCommandExporter, r.facilityWithExternalEffectsAsSource(facility));

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
        r.execute(combiner, r.execute(takeChainDataForPrint, r.facilityWithExternalEffectsAsSource(facility)));
        r.execute(combiner, r.actionAsSource(extractFacilityOutput));

        //return the combiner output

        return {
            r.actionAsSource(combiner)
        };
    }

} }

#endif