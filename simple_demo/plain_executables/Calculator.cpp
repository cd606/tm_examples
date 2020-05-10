#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeMonad.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQOnOrderFacility.hpp>

#include <boost/program_options.hpp>

#include "defs.pb.h"
#include "simple_demo/external_logic/Calculator.hpp"

#include <iostream>
#include <sstream>

using namespace dev::cd606::tm;
using namespace simple_demo;

using TheEnvironment = infra::Environment<
    infra::CheckTimeComponent<true>,
    basic::TrivialBoostLoggingComponent,
    basic::real_time_clock::ClockComponent,
    transport::BoostUUIDComponent,
    transport::ServerSideSimpleIdentityCheckerComponent<std::string,CalculateCommand>,
    transport::rabbitmq::RabbitMQComponent
>;
using M = infra::RealTimeMonad<TheEnvironment>;

class CalculatorFacility final : public M::IExternalComponent, public M::AbstractOnOrderFacility<std::tuple<std::string, CalculateCommand>,CalculateResult>, public CalculateResultListener {
private:
    TheEnvironment *env_;
    Calculator calc_;
    std::unordered_map<int, TheEnvironment::IDType> idLookup_;
    std::mutex mutex_;
public:
    CalculatorFacility() : env_(nullptr), calc_(), mutex_() {}
    ~CalculatorFacility() {}
    virtual void start(TheEnvironment *env) override final {
        env_ = env;
        calc_.start(this);
    }
    virtual void handle(M::InnerData<M::Key<std::tuple<std::string, CalculateCommand>>> &&data) override final {
        {
            std::lock_guard<std::mutex> _(mutex_);
            idLookup_.insert({std::get<1>(data.timedData.value.key()).id(), data.timedData.value.id()});
        }
        calc_.request(std::get<1>(data.timedData.value.key()));
    }
    virtual void onCalculateResult(CalculateResult const &result) override final {
        TheEnvironment::IDType envID;
        bool isFinalResponse = (result.result() <= 0);
        {
            std::lock_guard<std::mutex> _(mutex_);
            auto iter = idLookup_.find(result.id());
            if (iter == idLookup_.end()) {
                return;
            }
            envID = iter->second;
            if (isFinalResponse) {
                idLookup_.erase(iter);
            }
        }
        publish(env_, M::Key<CalculateResult> {envID, result}, isFinalResponse);
    }
};

int main(int argc, char **argv) {
    TheEnvironment env;
    infra::MonadRunner<M> r(&env);

    auto facility = M::fromAbstractOnOrderFacility(new CalculatorFacility());
    r.registerOnOrderFacility("facility", facility);
    transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::wrapOnOrderFacility(
        r, facility, transport::ConnectionLocator::parse("localhost::guest:guest:test_queue"), "wrapper_"
        , std::nullopt //hook
        , true //encode final flag
    );

    r.finalize();

    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });

    return 0;
}