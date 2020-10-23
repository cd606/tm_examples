#ifndef EXTERNAL_CALCULATOR_WRAPPED_AS_FACILITY_HPP_
#define EXTERNAL_CALCULATOR_WRAPPED_AS_FACILITY_HPP_

#include "simple_demo_chain_version/external_logic/ExternalCalculator.hpp"

#include <tm_kit/infra/RealTimeApp.hpp>
#include <unordered_map>

namespace simple_demo_chain_version { namespace calculator_logic {
    template <class Env>
    class ExternalCalculatorWrappedAsFacility : 
        public infra::RealTimeApp<Env>::IExternalComponent
        , public infra::RealTimeApp<Env>::template AbstractOnOrderFacility<ExternalCalculatorInput, ExternalCalculatorOutput>
        , public CalculateResultListener
    {
    private:
        Env *env_;
        ExternalCalculator calc_;
        std::unordered_map<int, typename Env::IDType> idLookup_;
        std::mutex mutex_;
    public:
        ExternalCalculatorWrappedAsFacility() : env_(nullptr), calc_() {}
        virtual ~ExternalCalculatorWrappedAsFacility() {}
        virtual void start(Env *env) {
            env_ = env;
            calc_.start(this);
        }
        virtual void handle(typename infra::RealTimeApp<Env>::template InnerData<
            typename infra::RealTimeApp<Env>::template Key<ExternalCalculatorInput>
        > &&input) override final {
            {
                std::lock_guard<std::mutex> _(mutex_);
                idLookup_.insert({input.timedData.value.key().id, input.timedData.value.id()});
            }
            calc_.request(input.timedData.value.key());
        }
        virtual void onCalculateResult(ExternalCalculatorOutput const &o) override final {
            typename Env::IDType id;
            auto isFinal = (o.output <= 0);
            {
                std::lock_guard<std::mutex> _(mutex_);
                auto iter = idLookup_.find(o.id);
                if (iter == idLookup_.end()) {
                    return;
                }
                id = iter->second;
                if (isFinal) {
                    idLookup_.erase(iter);
                }
            }
            this->publish(
                env_
                , typename infra::RealTimeApp<Env>::template Key<ExternalCalculatorOutput> {
                    id 
                    , o
                }
                , isFinal
            );
        }
    };
} }

#endif