#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

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
#include <tm_kit/transport/security/SignatureBasedIdentityCheckerComponent.hpp>
#include <tm_kit/transport/security/SignatureAndVerifyHookFactoryComponents.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>

#include <boost/program_options.hpp>
#include <boost/hana/functional/curry.hpp>

#include "defs.pb.h"
#include "simple_demo/external_logic/Calculator.hpp"
#include "simple_demo/security_logic/SignatureAndEncBasedIdentityCheckerComponent.hpp"
#include "simple_demo/security_logic/DHHelper.hpp"
#include "simple_demo/security_logic/EncAndSignHookFactory.hpp"

#include <iostream>
#include <sstream>

using namespace dev::cd606::tm;
using namespace simple_demo;

using TheEnvironment = infra::Environment<
    infra::CheckTimeComponent<true>,
    infra::TrivialExitControlComponent,
    basic::TrivialBoostLoggingComponent,
    basic::real_time_clock::ClockComponent,
    transport::BoostUUIDComponent,
    ServerSideSignatureAndEncBasedIdentityCheckerComponent<CalculateCommand>,
    transport::security::ServerSideSignatureBasedIdentityCheckerComponent<DHHelperCommand>,
    transport::security::SignatureHookFactoryComponent<DHHelperReply>,
    transport::rabbitmq::RabbitMQComponent,
    transport::redis::RedisComponent,
    transport::HeartbeatAndAlertComponent,
    EncAndSignHookFactoryComponent<transport::HeartbeatMessage>
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
    env.ServerSideSignatureAndEncBasedIdentityCheckerComponent<CalculateCommand>::add_identity_and_key(
        "main_logic_identity"
        , main_logic_pub_key
    );
    env.transport::security::ServerSideSignatureBasedIdentityCheckerComponent<DHHelperCommand>::add_identity_and_key(
        "main_logic_identity"
        , main_logic_pub_key
    );
    env.transport::security::SignatureHookFactoryComponent<DHHelperReply>::operator=(
        transport::security::SignatureHookFactoryComponent<DHHelperReply>(
            my_prv_key
        )
    );
    env.EncAndSignHookFactoryComponent<transport::HeartbeatMessage>::operator=(
        EncAndSignHookFactoryComponent<transport::HeartbeatMessage> {
            "testkey",
            my_prv_key
        }
    );

    transport::initializeHeartbeatAndAlertComponent(
        &env
        , "simple_demo secure Calculator"
        , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
    );

    using R = infra::AppRunner<M>;
    R r(&env);

    auto facility = M::fromAbstractOnOrderFacility(new CalculatorFacility());
    r.registerOnOrderFacility("facility", facility);

    transport::MultiTransportFacilityWrapper<R>::wrap<CalculateCommand,CalculateResult>(
        r, facility, "redis://localhost:6379:::test_queue", "wrapper/"
    );

    auto dh = std::make_shared<DHServerHelper>(&env);
    r.preservePointer(dh);

    auto dhFacility = M::template liftPureOnOrderFacility<std::tuple<std::string, DHHelperCommand>>(
        boost::hana::curry<2>(std::mem_fn(&DHServerHelper::process))(dh.get())
    );
    r.registerOnOrderFacility("dh_server_facility", dhFacility);

    transport::MultiTransportFacilityWrapper<R>::wrap<DHHelperCommand, DHHelperReply>(
        r, dhFacility, "redis://localhost:6379:::test_dh_queue", "dh_wrapper/"
    );

    transport::attachHeartbeatAndAlertComponent(r, &env, "simple_demo.secure_executables.calculator.heartbeat", std::chrono::seconds(1));
    env.setStatus("program", transport::HeartbeatMessage::Status::Good);

    r.finalize();

    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });

    return 0;
}