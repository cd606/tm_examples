#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/SerializationHelperMacros.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/TLSConfigurationComponent.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>

#ifdef _MSC_VER
#define REQ_FIELDS \
    ((std::vector<std::string>, x)) \
    ((double, y)) \
    ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::variant<int,float>), tChoice))
#define RESP_FIELDS \
    ((uint32_t, xCount)) \
    ((double, yTimesTwo)) \
    ((std::list<std::string>, xCopy)) \
    ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::tuple<int,float>), t))
#else
#define REQ_FIELDS \
    ((std::vector<std::string>, x)) \
    ((double, y)) \
    (((std::variant<int,float>), tChoice))
#define RESP_FIELDS \
    ((uint32_t, xCount)) \
    ((double, yTimesTwo)) \
    ((std::list<std::string>, xCopy)) \
    (((std::tuple<int,float>), t))
#endif

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
    , transport::TLSServerConfigurationComponent
    , transport::AllNetworkTransportComponents
    , transport::ServerSideSimpleIdentityCheckerComponent<
        std::string, Req
    >
>;
using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;
using GL = infra::GenericLift<M>;

int main(int argc, char **argv) {
    Env env;
    R r(&env);

    bool useSsl = (argc>=2 && std::string_view(argv[1]) == "ssl");
    if (useSsl) {
        env.transport::TLSServerConfigurationComponent::setConfigurationItem(
            transport::TLSServerInfoKey {
                34567
            }
            , transport::TLSServerInfo {
                "../grpc_interop_test/DotNetServer/server.crt"
                , "../grpc_interop_test/DotNetServer/server.key"
            }
        );
    }
    env.transport::json_rest::JsonRESTComponent::addBasicAuthentication(34567, "user1", std::nullopt);
    env.transport::json_rest::JsonRESTComponent::addBasicAuthentication(34567, "user2", "abcde");
    env.transport::json_rest::JsonRESTComponent::setDocRoot(34567, "../json_rest_test");

    auto facility = GL::liftFacility([&env](std::tuple<std::string,Req> &&reqWithIdentity) -> Resp {
        env.log(infra::LogLevel::Info, "Got request from '"+std::get<0>(reqWithIdentity)+"'");
        auto &r = std::get<1>(reqWithIdentity);
        /*
        std::variant<int, float> tChoice;
        if (std::get<0>(r.t) > std::get<1>(r.t)) {
            tChoice.emplace<0>(std::get<0>(r.t));
        } else {
            tChoice.emplace<1>(std::get<1>(r.t));
        }
        */
        std::tuple<int, float> t;
        if (r.tChoice.index() == 0) {
            t = {std::get<0>(r.tChoice), -1.0f};
        } else {
            t = {-1, std::get<1>(r.tChoice)};
        }
        return Resp {
            (uint32_t) r.x.size()
            , r.y*2.0
            , std::list<std::string> {r.x.begin(), r.x.end()}
            , std::move(t) //, std::move(tChoice)
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