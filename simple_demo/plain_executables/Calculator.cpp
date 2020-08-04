#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQOnOrderFacility.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>

#include <boost/program_options.hpp>

#include "defs.pb.h"
#include "simple_demo/external_logic/Calculator.hpp"

#include <iostream>
#include <sstream>

using namespace dev::cd606::tm;
using namespace simple_demo;

using TheEnvironment = infra::Environment<
    infra::CheckTimeComponent<false>,
    infra::TrivialExitControlComponent,
    basic::TrivialBoostLoggingComponent,
    basic::real_time_clock::ClockComponent,
    transport::BoostUUIDComponent,
    transport::ServerSideSimpleIdentityCheckerComponent<std::string,CalculateCommand>,
    transport::rabbitmq::RabbitMQComponent,
    transport::HeartbeatAndAlertComponent
>;
using M = infra::RealTimeApp<TheEnvironment>;

class CalculatorFacility final : public M::IExternalComponent, public M::AbstractOnOrderFacility<std::tuple<std::string, CalculateCommand>,CalculateResult>, public CalculateResultListener {
private:
    TheEnvironment *env_;
    Calculator calc_;
    std::unordered_map<int, TheEnvironment::IDType> idLookup_;
    int count_;
    std::mutex mutex_;
public:
    CalculatorFacility() : env_(nullptr), calc_(), idLookup_(), count_(0), mutex_() {}
    ~CalculatorFacility() {}
    virtual void start(TheEnvironment *env) override final {
        env_ = env;
        calc_.start(this);
        env->sendAlert("simple_demo.plain_executables.calculator.info", infra::LogLevel::Info, "Calculator started");
    }
    virtual void handle(M::InnerData<M::Key<std::tuple<std::string, CalculateCommand>>> &&data) override final {
        int countCopy, outstandingCountCopy;
        {
            std::lock_guard<std::mutex> _(mutex_);
            ++count_;
            idLookup_.insert({std::get<1>(data.timedData.value.key()).id(), data.timedData.value.id()});
            countCopy = count_;
            outstandingCountCopy = idLookup_.size();
        }
        auto const &req = std::get<1>(data.timedData.value.key());
        CalculatorInput input {
            req.id(), req.value()
        };
        calc_.request(input);
        std::ostringstream oss;
        oss << "Total " << countCopy << " requests, outstanding " << outstandingCountCopy << " requests";
        data.environment->setStatus(
            "calculator_body"
            , transport::HeartbeatMessage::Status::Good
            , oss.str()
        );
    }
    virtual void onCalculateResult(CalculatorOutput const &result) override final {
        TheEnvironment::IDType envID;
        CalculateResult res;
        res.set_id(result.id);
        res.set_result(result.output);
        bool isFinalResponse = (res.result() <= 0);
        {
            std::lock_guard<std::mutex> _(mutex_);
            auto iter = idLookup_.find(res.id());
            if (iter == idLookup_.end()) {
                return;
            }
            envID = iter->second;
            if (isFinalResponse) {
                idLookup_.erase(iter);
            }
        }
        publish(env_, M::Key<CalculateResult> {envID, res}, isFinalResponse);
    }
};

int main(int argc, char **argv) {
    TheEnvironment env;
    
    transport::HeartbeatAndAlertComponentInitializer<TheEnvironment,transport::rabbitmq::RabbitMQComponent>()
        (&env, "simple_demo plain Calculator", transport::ConnectionLocator::parse("127.0.0.1::guest:guest:amq.topic[durable=true]"));
    env.setStatus("program", transport::HeartbeatMessage::Status::Good);

    infra::AppRunner<M> r(&env);

    auto facility = M::fromAbstractOnOrderFacility(new CalculatorFacility());
    r.registerOnOrderFacility("facility", facility);
    transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::wrapOnOrderFacility(
        r, facility, transport::ConnectionLocator::parse("127.0.0.1::guest:guest:test_queue"), "wrapper_"
        , std::nullopt //hook
    );

    transport::attachHeartbeatAndAlertComponent(r, &env, "simple_demo.plain_executables.calculator.heartbeat", std::chrono::seconds(1));

    r.finalize();

    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });

    return 0;
}