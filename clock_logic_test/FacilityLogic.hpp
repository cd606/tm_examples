#ifndef CLOCK_LOGIC_TEST_FACILITY_LOGIC_HPP_
#define CLOCK_LOGIC_TEST_FACILITY_LOGIC_HPP_

#include <memory>
#include <string>
#include <list>
#include <tm_kit/infra/WithTimeData.hpp>

namespace dev { namespace cd606 { namespace tm { namespace clock_logic_test_app {

    class FacilityLogicImpl;

    class FacilityLogic {
    private:
        std::unique_ptr<FacilityLogicImpl> impl_;
    public:
        FacilityLogic();
        ~FacilityLogic();
        std::string result(std::string const &queryKey, std::string const &dataInput);
    };

    template <class M>
    class Facility final : public M::template AbstractIntegratedLocalOnOrderFacility<std::string,std::string,std::string> {
    private:
        using TheEnvironment = typename M::EnvironmentType;
        FacilityLogic logic_;
        std::list<typename M::template Key<std::string>> queryInput_;
        std::string dataInput_;
        void doRespond(TheEnvironment *env, typename M::TimePoint tp) {
            if (dataInput_ != "") {
                for (auto const &item : queryInput_) {
                    this->publish(
                        {
                            env,
                            typename M::template TimedDataType<typename M::template Key<std::string>> {
                                tp
                                , {item.id(), logic_.result(item.key(), dataInput_)}
                                , true
                            }
                        }
                    );
                }        
                queryInput_.clear();
            }
        }
    public:
        virtual void start(TheEnvironment *) override final {}
        virtual void handle(typename M::template InnerData<typename M::template Key<std::string>> &&input) override final {
            queryInput_.push_back(std::move(input.timedData.value));
            doRespond(input.environment, input.timedData.timePoint);
        }
        virtual void handle(typename M::template InnerData<std::string> &&input) override final {
            dataInput_ = std::move(input.timedData.value);
            doRespond(input.environment, input.timedData.timePoint);
        } 
    };

} } } }

#endif