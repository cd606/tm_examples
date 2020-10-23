#ifndef MOCK_EXTERNAL_CALCULATOR_HPP_
#define MOCK_EXTERNAL_CALCULATOR_HPP_

#include "simple_demo_chain_version/external_logic/ExternalCalculator.hpp"
#include <tm_kit/infra/KleisliUtils.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

using namespace dev::cd606::tm;

namespace simple_demo_chain_version { namespace calculator_logic {
    template <class R, class ClockFacility>
    class MockExternalCalculator {
    private:
        using Env = typename R::EnvironmentType;
        using M = typename R::AppType;
    public:
        static auto connector(std::string const &prefix)
            -> typename R::template FacilitioidConnector<ExternalCalculatorInput,ExternalCalculatorOutput> 
        {
            return [prefix](
                R &r
                , typename R::template Source<typename M::template Key<ExternalCalculatorInput>> &&source
                , std::optional<typename R::template Sink<typename M::template KeyedData<ExternalCalculatorInput, ExternalCalculatorOutput>>> const &sink    
            ) {
                auto clockFacility = ClockFacility::template createClockCallback<ExternalCalculatorInput, bool>(
                    [](std::chrono::system_clock::time_point const &, std::size_t thisIdx, std::size_t totalCount) {
                        return (thisIdx+1 >= totalCount);
                    }
                );

                using ClockFacilityInput = typename ClockFacility::template FacilityInput<ExternalCalculatorInput>;
                
                auto createFacilityKey = 
                    M::template kleisli<typename M::template Key<ExternalCalculatorInput>>(
                        basic::CommonFlowUtilComponents<M>::template withKey<ExternalCalculatorInput>(
                            infra::KleisliUtils<M>::template liftPure<ExternalCalculatorInput>(
                                [](ExternalCalculatorInput &&input) -> ClockFacilityInput {
                                    return {
                                        std::move(input)
                                        , {
                                            std::chrono::seconds(0), std::chrono::seconds(2)
                                        }
                                    };
                                }
                            )
                        )
                    );

                if (sink) {
                    using ClockFacilityOutput = typename M::template KeyedData<ClockFacilityInput, bool>;       
                    auto facilityCallback = M::template liftPure<ClockFacilityOutput>(
                        [](ClockFacilityOutput &&data) -> typename M::template KeyedData<ExternalCalculatorInput,ExternalCalculatorOutput> {
                            ExternalCalculatorOutput res;
                            res.id = data.key.key().inputData.id;
                            if (data.data) {
                                res.output = -1.0;
                            } else {
                                res.output = data.key.key().inputData.input*2.0;
                            }
                            return { 
                                {data.key.id(), data.key.key().inputData}
                                , std::move(res)
                            };
                        }
                    );

                    r.placeOrderWithFacility(
                        r.execute(prefix+"/createFacilityKey", createFacilityKey, std::move(source))
                        , prefix+"/clockFacility", clockFacility
                        , r.actionAsSink(prefix+"/facilityCallback", facilityCallback)
                    );
                    r.connect(r.actionAsSource(facilityCallback), *sink);
                } else {
                    r.placeOrderWithFacilityAndForget(
                        r.execute(prefix+"/createFacilityKey", createFacilityKey, std::move(source))
                        , prefix+"/clockFacility", clockFacility
                    );
                }    
            };
        }
    };
} }

#endif