#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>
#include <tm_kit/infra/SynchronousRunner.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SerializationHelperMacros.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils_SynchronousRunner.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>

#include <tclap/CmdLine.h>

using namespace dev::cd606::tm;

using Environment = infra::Environment<
    infra::CheckTimeComponent<true>,
    infra::TrivialExitControlComponent,
    basic::SpdLoggingComponent,
    basic::real_time_clock::ClockComponent,
    transport::CrossGuidComponent,
    transport::AllNetworkTransportComponents,
    transport::HeartbeatAndAlertComponent
>;
using M = infra::RealTimeApp<Environment>;
using R = infra::AppRunner<M>;
using SR = infra::SynchronousRunner<M>;

#define FACILITY_INPUT_FIELDS \
    ((int64_t, timestamp)) \
    ((int, data))
#define FACILITY_OUTPUT_FIELDS \
    ((int, data))

TM_BASIC_CBOR_CAPABLE_STRUCT(FacilityInput, FACILITY_INPUT_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT(FacilityOutput, FACILITY_OUTPUT_FIELDS);

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(FacilityInput, FACILITY_INPUT_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(FacilityOutput, FACILITY_OUTPUT_FIELDS);

void runServer(std::string const &serviceDescriptor, transport::HeartbeatAndAlertComponentTouchupParameters const &heartbeatParam) {
    Environment env;
    R r(&env);

    transport::HeartbeatAndAlertComponentTouchup<R> {
        r 
        , heartbeatParam
    };

    infra::GenericComponentLiftAndRegistration<R> _(r);
    auto facility = _("facility", infra::LiftAsFacility{}, [](FacilityInput &&x) -> FacilityOutput {
        return {x.data*2};
    });
    transport::MultiTransportFacilityWrapper<R>::wrap<FacilityInput,FacilityOutput>(
        r 
        , facility 
        , serviceDescriptor
        , "wrapper"
    );

    r.finalize();
    infra::terminationController(infra::RunForever {});
}

void runClient(transport::SimpleRemoteFacilitySpec const &spec, int repeatTimes) {
    Environment env;
    R r(&env);
    int64_t firstTimeStamp = 0;
    int64_t count = 0;
    int64_t recvCount = 0;

    auto facilitioidAndTrigger = transport::MultiTransportRemoteFacilityManagingUtils<R>
        ::setupSimpleRemoteFacilitioid<FacilityInput, FacilityOutput>
        (
            r 
            , spec
            , "remoteConnection"
        );

    infra::DeclarativeGraph<R>("", {
        {"data", infra::DelayImporter<basic::VoidStruct>{}, [repeatTimes](Environment *e) -> std::tuple<bool, M::Data<int>> {
            static int count = 0;
            ++count;
            return std::tuple<bool, M::Data<int>> {
                (count<repeatTimes)
                , M::InnerData<int> {
                    e 
                    , {
                        e->resolveTime()
                        , count 
                        , (count >= repeatTimes)
                    }
                }
            };
        }}
        , {"keyify", [](M::InnerData<int> &&x) -> M::Data<M::Key<FacilityInput>> {
            return M::InnerData<M::Key<FacilityInput>> {
                x.environment
                , {
                    x.timedData.timePoint
                    , M::keyify(FacilityInput {
                        infra::withtime_utils::sinceEpoch<std::chrono::microseconds>(x.timedData.timePoint)
                        , x.timedData.value
                    })
                    , x.timedData.finalFlag
                }
            };
        }}
        , {"exporter", [&firstTimeStamp,&count,&recvCount,repeatTimes](M::InnerData<M::KeyedData<FacilityInput,FacilityOutput>> &&data) {
            auto now = infra::withtime_utils::sinceEpoch<std::chrono::microseconds>(data.timedData.timePoint);
            count += (now-data.timedData.value.key.key().timestamp);
            if (data.timedData.value.key.key().data == 1) {
                firstTimeStamp = data.timedData.value.key.key().timestamp;
            }
            ++recvCount;
            if (data.timedData.value.key.key().data >= repeatTimes) {
                data.environment->log(infra::LogLevel::Info, std::string("average delay of ")+std::to_string(recvCount)+" calls is "+std::to_string(count*1.0/repeatTimes)+" microseconds");
                data.environment->log(infra::LogLevel::Info, std::string("total time for ")+std::to_string(recvCount)+" calls is "+std::to_string(now-firstTimeStamp)+" microseconds");
                data.environment->exit();
            }
        }}
        , {"data", "keyify"}
        , {"keyify", facilitioidAndTrigger.facility, "exporter"}
        , {facilitioidAndTrigger.facilityFirstReady.clone(), "data"}
    })(r);
    r.finalize();
    infra::terminationController(infra::RunForever {});
}

void runClientSynchronousMode(transport::SimpleRemoteFacilitySpec const &spec, int repeatTimes) {
    Environment env;
    SR r(&env);
    int64_t firstTimeStamp = 0;
    int64_t count = 0;
    int64_t recvCount = 0;

    auto facility =  transport::MultiTransportRemoteFacilityManagingUtils<SR>
        ::setupSimpleRemoteFacility<FacilityInput, FacilityOutput>
        (
            r 
            , spec
        );
    for (int ii=0; ii<repeatTimes; ++ii) {
        auto data = r.placeOrderWithFacility(
            FacilityInput {infra::withtime_utils::sinceEpoch<std::chrono::microseconds>(env.now()), ii+1}
            , facility
        )->front();
        auto now = infra::withtime_utils::sinceEpoch<std::chrono::microseconds>(data.timedData.timePoint);
        count += (now-data.timedData.value.key.key().timestamp);
        if (data.timedData.value.key.key().data == 1) {
            firstTimeStamp = data.timedData.value.key.key().timestamp;
        }
        ++recvCount;
        if (data.timedData.value.key.key().data >= repeatTimes) {
            env.log(infra::LogLevel::Info, std::string("average delay of ")+std::to_string(recvCount)+" calls is "+std::to_string(count*1.0/repeatTimes)+" microseconds");
            env.log(infra::LogLevel::Info, std::string("total time for ")+std::to_string(recvCount)+" calls is "+std::to_string(now-firstTimeStamp)+" microseconds");
        }
    }
    env.log(infra::LogLevel::Info, "done");
}

int main(int argc, char **argv) {
    TCLAP::CmdLine cmd("RPC Delay Measurer", ' ', "0.0.1");
    TCLAP::ValueArg<std::string> modeArg("m", "mode", "server, client or client-sync", true, "", "string");
    TCLAP::ValueArg<std::string> serviceDescriptorArg("d", "serviceDescriptor", "service descriptor", false, "", "string");
    TCLAP::ValueArg<std::string> heartbeatDescriptorArg("H", "heartbeatDescriptor", "heartbeat descriptor", false, "", "string");
    TCLAP::ValueArg<std::string> heartbeatTopicArg("T", "heartbeatTopic", "heartbeat topic", false, "tm.examples.heartbeats", "string");
    TCLAP::ValueArg<std::string> heartbeatIdentityArg("I", "heartbeatIdentity", "heartbeat identity", false, "rpc_delay_measurer", "string");
    TCLAP::ValueArg<int> repeatTimesArg("r", "repeatTimes", "repeat times", false, 1000, "int");
    cmd.add(modeArg);
    cmd.add(serviceDescriptorArg);
    cmd.add(heartbeatDescriptorArg);
    cmd.add(heartbeatTopicArg);
    cmd.add(heartbeatIdentityArg);
    cmd.add(repeatTimesArg);    
    
    cmd.parse(argc, argv);

    if (modeArg.getValue() == "server") {
        if (!serviceDescriptorArg.isSet()) {
            std::cerr << "Server mode requires descriptor\n";
            return 1;
        }
        transport::HeartbeatAndAlertComponentTouchupParameters heartbeatParam {
            heartbeatDescriptorArg.getValue()
            , heartbeatTopicArg.getValue()
            , heartbeatIdentityArg.getValue()
            , std::chrono::seconds(2)
        };
        runServer(serviceDescriptorArg.getValue(), heartbeatParam);
        return 0;
    } else if (modeArg.getValue() == "client") {
        if (serviceDescriptorArg.isSet()) {
            runClient(serviceDescriptorArg.getValue(), repeatTimesArg.getValue());
            return 0;
        } else if (heartbeatDescriptorArg.isSet()) {
            runClient(transport::SimpleRemoteFacilitySpecByHeartbeat {
                heartbeatDescriptorArg.getValue()
                , heartbeatTopicArg.getValue()
                , std::regex {heartbeatIdentityArg.getValue()}
                , "facility"
            }, repeatTimesArg.getValue());
            return 0;
        } else {
            std::cerr << "Client mode requires either service descriptor or heartbeat descriptor\n";
            return 1;
        }
    } else if (modeArg.getValue() == "client-sync") {
        if (serviceDescriptorArg.isSet()) {
            runClientSynchronousMode(serviceDescriptorArg.getValue(), repeatTimesArg.getValue());
            return 0;
        } else if (heartbeatDescriptorArg.isSet()) {
            runClientSynchronousMode(transport::SimpleRemoteFacilitySpecByHeartbeat {
                heartbeatDescriptorArg.getValue()
                , heartbeatTopicArg.getValue()
                , std::regex {heartbeatIdentityArg.getValue()}
                , "facility"
            }, repeatTimesArg.getValue());
            return 0;
        } else {
            std::cerr << "Client-sync mode requires either service descriptor or heartbeat descriptor\n";
            return 1;
        }
    } else {
        std::cerr << "Mode must be server, client or client-sync\n";
        return 1;
    }
}