#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>
#include <tm_kit/infra/GenericLift.hpp>
#include <tm_kit/infra/TerminationController.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/ProtoInterop.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/HeartbeatAndAlertComponent.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>
#include <tm_kit/transport/TLSConfigurationComponent.hpp>

#include "../CppShare/CppNoCodeGenStruct.hpp"

using namespace dev::cd606::tm;

using Req = grpc_interop_test::TestRequest;
using Resp = grpc_interop_test::TestResponse;
using SimpleReq = grpc_interop_test::SimpleRequest;
using SimpleResp = grpc_interop_test::SimpleResponse;

using Env = infra::Environment<
    infra::CheckTimeComponent<false>
    , infra::TrivialExitControlComponent
    , basic::TimeComponentEnhancedWithSpdLogging<
        basic::real_time_clock::ClockComponent
    >
    , transport::CrossGuidComponent
    , transport::TLSServerConfigurationComponent
    , transport::AllNetworkTransportComponents
    , transport::HeartbeatAndAlertComponent
>;
using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;
using GL = infra::GenericLift<M>;

int main(int argc, char **argv) {
    Env env;
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
    R r(&env);

    transport::HeartbeatAndAlertComponentTouchup<R>(
        r
        , {
            "zeromq://127.0.0.1:12345"
            , "grpc_interop_test.heartbeat"
            , "grpc_interop_test_server"
            , std::chrono::seconds(1)
        }
    );

    class TestFacility : public M::AbstractOnOrderFacility<Req, Resp> {
    public:
        virtual void handle(M::InnerData<M::Key<Req>> &&req) override final {
            std::thread th([this,req=std::move(req)]() mutable {
                auto id = req.timedData.value.id();
                auto const &reqData = req.timedData.value.key();
                uint32_t chunkSize = std::max(1u, reqData.intParam);
                uint32_t totalSize = reqData.doubleListParam.size();
                if (totalSize == 0) {
                    this->publish(req.environment, M::Key<Resp> {id, Resp{}}, true);
                } else {
                    for (uint32_t ii=0; ii<totalSize; ii+=chunkSize) {
                        Resp resp;
                        uint32_t jj = 0;
                        for (; jj<chunkSize && ii+jj<totalSize; ++jj) {
                            resp.stringResp.push_back(std::to_string(reqData.doubleListParam[ii+jj]));
                            resp.stringResp.push_back("");
                        }
                        this->publish(req.environment, M::Key<Resp> {id, std::move(resp)}, (ii+jj)>=totalSize);
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }
            });
            th.detach();
        }
    };
    auto testFacility = M::fromAbstractOnOrderFacility(new TestFacility());
    
    auto simpleTestFacility = GL::lift(infra::LiftAsFacility {}, [](SimpleReq &&req) {
        SimpleResp resp;
        resp.resp = 2*(req.input);
        std::visit([&resp](auto const &x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, std::string>) {
                resp.respOneOf.emplace<1>(x+":resp");
            } else if constexpr (std::is_same_v<T, float>) {
                resp.respOneOf.emplace<2>(x*2.0f);
            }
        }, req.reqOneOf);
        resp.name2Resp = req.name2+":resp";
        std::copy(req.anotherInput.begin(), req.anotherInput.end(), std::back_inserter(resp.anotherInputBack.value));
        for (auto const &item : req.mapInput) {
            resp.mapOutput.value[item.first] = item.second;
        }
        std::cerr << "Got " << req << ", returning " << resp << '\n';
        return resp;
    });

    r.registerOnOrderFacility("testFacility", testFacility);
    r.registerOnOrderFacility("simpleTestFacility", simpleTestFacility);

    //By default a facility can only have one outgoing connection.
    //Since we want to wrap it twice with two different protocols,
    //we need to increase this limit.
    r.setMaxOutputConnectivity(testFacility, 2);
    r.setMaxOutputConnectivity(simpleTestFacility, 2);

    //It is ok to use basic::CBOR as protocol here too,
    //because adding basic::CBOR on top of CBOR-supporting structure
    //will not affect the encoding. However, for simplification,
    //std::void_t would be preferred
    transport::MultiTransportFacilityWrapper<R>::wrapWithProtocol<std::void_t,Req,Resp>
        (
            r
            , testFacility
            , "redis://localhost:6379:::testServiceQueue"
            , "testService-redis"
            , std::nullopt
            , true
        );
    //The second wrap will not be able to write into heartbeat message
    //since the facility is already registered with redis in the heartbeat
    //message. If the registration order needs to be reversed, but the
    //actually registered wrapper needs to the redis one still,
    //add std::nullopt (for hooks) and true (for no registration in 
    //heartbeat message) to the first wrap call to avoid registration
    transport::MultiTransportFacilityWrapper<R>::wrapWithProtocol<basic::proto_interop::Proto,Req,Resp>
        (
            r
            , testFacility
            , "grpc_interop://localhost:34567:::grpc_interop_test/TestService/Test"
            , "testService-grpc"
        );
    transport::MultiTransportFacilityWrapper<R>::wrapWithProtocol<std::void_t,SimpleReq,SimpleResp>
        (
            r
            , simpleTestFacility
            , "redis://localhost:6379:::simpleTestServiceQueue"
            , "simpleTestService-redis"
        );
    transport::MultiTransportFacilityWrapper<R>::wrapWithProtocol<basic::proto_interop::Proto,SimpleReq,SimpleResp>
        (
            r
            , simpleTestFacility
            , "grpc_interop://localhost:34567:::grpc_interop_test/TestService/SimpleTest"
            , "simpleTestService-grpc"
        );

    r.finalize();
    infra::terminationController(infra::RunForever {});
}