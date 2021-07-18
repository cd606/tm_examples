#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SerializationHelperMacros.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>

#include <tclap/CmdLine.h>

using namespace dev::cd606::tm;

using Environment = infra::Environment<
    infra::CheckTimeComponent<true>,
    infra::TrivialExitControlComponent,
    basic::SpdLoggingComponent,
    basic::real_time_clock::ClockComponent,
    transport::CrossGuidComponent,
    transport::AllNetworkTransportComponents
>;
using M = infra::RealTimeApp<Environment>;
using R = infra::AppRunner<M>;

#define FACILITY_INPUT_FIELDS \
    ((int64_t, timestamp)) \
    ((int, data))
#define FACILITY_OUTPUT_FIELDS \
    ((int, data))

TM_BASIC_CBOR_CAPABLE_STRUCT(FacilityInput, FACILITY_INPUT_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT(FacilityOutput, FACILITY_OUTPUT_FIELDS);

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(FacilityInput, FACILITY_INPUT_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(FacilityOutput, FACILITY_OUTPUT_FIELDS);

void runServer(std::string const &serviceDescriptor) {
    Environment env;
    R r(&env);

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

void runClient(std::string const &serviceDescriptor, int repeatTimes) {
    Environment env;
    R r(&env);
    int64_t count = 0;
    int64_t recvCount = 0;

    infra::DeclarativeGraph<R>("", {
        {"importer", [repeatTimes](Environment *e) -> std::tuple<bool, M::Data<int>> {
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
        , {"facility", transport::MultiTransportRemoteFacilityManagingUtils<R>::setupSimpleRemoteFacility<FacilityInput,FacilityOutput>(r, serviceDescriptor)}
        , {"exporter", [&count,&recvCount,repeatTimes](M::InnerData<M::KeyedData<FacilityInput,FacilityOutput>> &&data) {
            auto now = infra::withtime_utils::sinceEpoch<std::chrono::microseconds>(data.timedData.timePoint);
            count += (now-data.timedData.value.key.key().timestamp);
            ++recvCount;
            if (data.timedData.value.key.key().data >= repeatTimes) {
                data.environment->log(infra::LogLevel::Info, std::string("average delay of ")+std::to_string(recvCount)+" calls is "+std::to_string(count*1.0/repeatTimes)+" microseconds");
                data.environment->exit();
            }
        }}
        , {"importer", "keyify"}
        , {"keyify", "facility", "exporter"}
    })(r);
    r.finalize();
    infra::terminationController(infra::RunForever {});
}

int main(int argc, char **argv) {
    TCLAP::CmdLine cmd("RPC Delay Measurer", ' ', "0.0.1");
    TCLAP::ValueArg<std::string> modeArg("m", "mode", "server or client", true, "", "string");
    TCLAP::ValueArg<std::string> serviceDescriptorArg("d", "serviceDescriptor", "service descriptor", true, "", "string");
    TCLAP::ValueArg<int> repeatTimesArg("r", "repeatTimes", "repeat times", false, 1000, "int");
    cmd.add(modeArg);
    cmd.add(serviceDescriptorArg);
    cmd.add(repeatTimesArg);    
    
    cmd.parse(argc, argv);

    if (modeArg.getValue() == "server") {
        runServer(serviceDescriptorArg.getValue());
        return 0;
    } else if (modeArg.getValue() == "client") {
        runClient(serviceDescriptorArg.getValue(), repeatTimesArg.getValue());
        return 0;
    } else {
        std::cerr << "Mode must be server or client\n";
        return 1;
    }
}