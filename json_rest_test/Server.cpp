#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/SerializationHelperMacros.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>

#define REQ_FIELDS \
    ((std::vector<std::string>, x)) \
    ((double, y))
#define RESP_FIELDS \
    ((uint32_t, xCount)) \
    ((double, yTimesTwo)) \
    ((std::list<std::string>, xCopy))

TM_BASIC_CBOR_CAPABLE_STRUCT(Req, REQ_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(Req, REQ_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT(Resp, RESP_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(Resp, RESP_FIELDS);

#undef REQ_FIELDS
#undef RESP_FIELDS

using namespace dev::cd606::tm;

using Env = infra::Environment<
    infra::CheckTimeComponent<false>
    , infra::TrivialExitControlComponent
    , basic::TimeComponentEnhancedWithSpdLogging<
        basic::real_time_clock::ClockComponent
        , false
    >
    , transport::CrossGuidComponent
    , transport::AllNetworkTransportComponents
>;
using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;
using GL = infra::GenericLift<M>;

int main() {
    Env env;
    R r(&env);

    auto facility = GL::liftFacility([](Req &&r) -> Resp {
        return Resp {
            (uint32_t) r.x.size()
            , r.y*2.0
            , std::list<std::string> {r.x.begin(), r.x.end()}
        };
    });
    r.registerOnOrderFacility("facility", facility);

    transport::MultiTransportFacilityWrapper<R>::wrapWithProtocol<std::void_t,Req,Resp>(
        r 
        , facility 
        , "json_rest://:34567:::/test_facility"
        , "wrapper"
        , std::nullopt
        , false
    );

    r.finalize();
    infra::terminationController(infra::RunForever {});
}