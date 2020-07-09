#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeMonad.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
/*
#include <tm_kit/transport/rabbitmq/RabbitMQImporterExporter.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQOnOrderFacility.hpp>
*/
#include <tm_kit/transport/redis/RedisComponent.hpp>
#include <tm_kit/transport/redis/RedisImporterExporter.hpp>
#include <tm_kit/transport/redis/RedisOnOrderFacility.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>

#include <boost/program_options.hpp>
#include <boost/hana/functional/curry.hpp>

#include "defs.pb.h"
#include "simple_demo/external_logic/Calculator.hpp"
#include "simple_demo/security_logic/SignatureBasedIdentityCheckerComponent.hpp"
#include "simple_demo/security_logic/SignatureAndAESBasedIdentityCheckerComponent.hpp"
#include "simple_demo/security_logic/DHServerSecurityCombination.hpp"

#include <iostream>
#include <sstream>

using namespace dev::cd606::tm;
using namespace simple_demo;

using TheEnvironment = infra::Environment<
    infra::CheckTimeComponent<true>,
    basic::TrivialBoostLoggingComponent,
    basic::real_time_clock::ClockComponent,
    transport::BoostUUIDComponent,
    ServerSideSignatureAndAESBasedIdentityCheckerComponent<CalculateCommand>,
    ServerSideSignatureBasedIdentityCheckerComponent<DHHelperCommand>,
    transport::rabbitmq::RabbitMQComponent,
    transport::redis::RedisComponent,
    transport::HeartbeatAndAlertComponent
>;
using M = infra::RealTimeMonad<TheEnvironment>;

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
        env->sendAlert("simple_demo.secure_executables.calculator.info", infra::LogLevel::Info, "Calculator started");
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
    std::array<unsigned char, 64> my_prv_key { 
        0x5E,0xD3,0x8F,0xE8,0x0A,0x67,0xA0,0xA4,0x24,0x0C,0x2D,0x0C,0xFE,0xB2,0xF4,0x78,
        0x69,0x46,0x01,0x95,0xF8,0xE4,0xD1,0xBB,0xC1,0xBC,0x22,0xCC,0x2F,0xB2,0x60,0xB0,
        0x69,0x61,0xB9,0xCF,0xBA,0x37,0xD0,0xE2,0x70,0x32,0x84,0xF9,0x41,0x02,0x17,0x22,
        0xFA,0x89,0x0F,0xE4,0xBA,0xAC,0xC8,0x73,0xB9,0x00,0x99,0x24,0x38,0x42,0xC2,0x9A 
    }; 
    std::array<unsigned char, 32> main_logic_pub_key { 
        0x69,0x61,0xB9,0xCF,0xBA,0x37,0xD0,0xE2,0x70,0x32,0x84,0xF9,0x41,0x02,0x17,0x22,
        0xFA,0x89,0x0F,0xE4,0xBA,0xAC,0xC8,0x73,0xB9,0x00,0x99,0x24,0x38,0x42,0xC2,0x9A 
    };

    TheEnvironment env;
    env.ServerSideSignatureAndAESBasedIdentityCheckerComponent<CalculateCommand>::add_identity_and_key(
        "main_logic_identity"
        , main_logic_pub_key
    );
    env.ServerSideSignatureBasedIdentityCheckerComponent<DHHelperCommand>::add_identity_and_key(
        "main_logic_identity"
        , main_logic_pub_key
    );

    infra::MonadRunner<M> r(&env);

    auto facility = M::fromAbstractOnOrderFacility(new CalculatorFacility());
    r.registerOnOrderFacility("facility", facility);
    transport::redis::RedisOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::wrapOnOrderFacility(
        r, facility, transport::ConnectionLocator::parse("localhost:6379:::test_queue"), "wrapper_"
        , std::nullopt //hook
    );

    DHServerSideCombination<
        infra::MonadRunner<M>
        , CalculateCommand
        , transport::redis::RedisOnOrderFacility<TheEnvironment>
        , transport::rabbitmq::RabbitMQComponent
    >(
        r
        , my_prv_key
        , transport::ConnectionLocator::parse("localhost:6379:::test_dh_queue") //facility locator
        , "simple_demo secure Calculator" //server name for heartbeat
        , transport::ConnectionLocator::parse("127.0.0.1::guest:guest:amq.topic[durable=true]") //heartbeat locator
        , "testkey" //encrypt heartbeat with this key
    );

    transport::attachHeartbeatAndAlertComponent(r, &env, "simple_demo.secure_executables.calculator.heartbeat", std::chrono::seconds(1));
    env.setStatus("program", transport::HeartbeatMessage::Status::Good);

    r.finalize();

    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });

    return 0;
}