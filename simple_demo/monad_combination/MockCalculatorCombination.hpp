#ifndef MOCK_CALCULATOR_COMBINATION_HPP_
#define MOCK_CALCULATOR_COMBINATION_HPP_

#include "defs.pb.h"

template <class R, class ClockFacility>
class MockCalculatorCombination {
private:
    using M = typename R::MonadType;
    using ID = typename R::EnvironmentType::IDType;
    template <class T>
    using Key = typename M::template Key<T>;
    template <class A, class B>
    using KeyedData = typename M::template KeyedData<A,B>;

    using MockInput = Key<simple_demo::CalculateCommand>;
    using MockOutput = KeyedData<simple_demo::CalculateCommand,simple_demo::CalculateResult>;
public:
    static void service(
        R &r
        , typename R::template Source<MockInput> &&source
        , std::optional<typename R::template Sink<MockOutput>> const &sink) 
    {       
        auto clockFacility = ClockFacility::template createClockCallback<simple_demo::CalculateCommand, bool>(
            [](std::chrono::system_clock::time_point const &, std::size_t thisIdx, std::size_t totalCount) {
                return (thisIdx+1 >= totalCount);
            }
        );

        using ClockFacilityInput = typename ClockFacility::template FacilityInput<simple_demo::CalculateCommand>;
        
        auto createFacilityKey = M::template liftPure<MockInput>(
            [](MockInput &&key) -> Key<ClockFacilityInput> {
                return {
                    key.id()
                    , {
                        key.key()
                        , {
                            std::chrono::seconds(0), std::chrono::seconds(2)
                        }
                    }
                };
            }
        );

        if (sink) {
            using ClockFacilityOutput = KeyedData<ClockFacilityInput, bool>;       
            auto facilityCallback = M::template liftPure<ClockFacilityOutput>(
                [](ClockFacilityOutput &&data) -> MockOutput {
                    simple_demo::CalculateResult res;
                    res.set_id(data.key.key().inputData.id());
                    if (data.data) {
                        res.set_result(-1.0);
                    } else {
                        res.set_result(data.key.key().inputData.value()*2.0);
                    }
                    return { 
                        {data.key.id(), data.key.key().inputData}
                        , std::move(res)
                    };
                }
            );

            r.placeOrderWithFacility(
                r.execute("createFacilityKey", createFacilityKey, std::move(source))
                , "clockFacility", clockFacility
                , r.actionAsSink("facilityCallback", facilityCallback)
            );
            r.connect(r.actionAsSource(facilityCallback), *sink);
        } else {
            r.placeOrderWithFacilityAndForget(
                r.execute("createFacilityKey", createFacilityKey, std::move(source))
                , "clockFacility", clockFacility
            );
        }        
    }
};

#endif