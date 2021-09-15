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

#include "../CppShare/CppNoCodeGenStruct.hpp"

using namespace dev::cd606::tm;

using Req = basic::proto_interop::Proto<grpc_interop_test::TestRequest>;
using Resp = basic::proto_interop::Proto<grpc_interop_test::TestResponse>;
using SimpleReq = basic::proto_interop::Proto<grpc_interop_test::SimpleRequest>;
using SimpleResp = basic::proto_interop::Proto<grpc_interop_test::SimpleResponse>;

using Env = infra::Environment<
    infra::CheckTimeComponent<false>
    , infra::TrivialExitControlComponent
    , basic::TimeComponentEnhancedWithSpdLogging<
        basic::real_time_clock::ClockComponent
    >
    , transport::CrossGuidComponent
    , transport::AllNetworkTransportComponents
    , transport::HeartbeatAndAlertComponent
>;
using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;
using GL = infra::GenericLift<M>;

int main(int argc, char **argv) {
    Env env;
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
                uint32_t chunkSize = std::max(1u, reqData->intParam);
                uint32_t totalSize = reqData->doubleListParam.size();
                if (totalSize == 0) {
                    this->publish(req.environment, M::Key<Resp> {id, Resp{}}, true);
                } else {
                    for (uint32_t ii=0; ii<totalSize; ii+=chunkSize) {
                        Resp resp;
                        uint32_t jj = 0;
                        for (; jj<chunkSize && ii+jj<totalSize; ++jj) {
                            resp->stringResp.push_back(std::to_string(reqData->doubleListParam[ii+jj]));
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
        resp->resp = 2*(req->input);
        return resp;
    });

    r.registerOnOrderFacility("testFacility", testFacility);
    r.registerOnOrderFacility("simpleTestFacility", simpleTestFacility);

    transport::MultiTransportFacilityWrapper<R>::wrap<Req,Resp>
        (
            r
            , testFacility
            , "grpc_interop://127.0.0.1:34567:::grpc_interop_test/TestService/Test"
            , "testService"
        );
    transport::MultiTransportFacilityWrapper<R>::wrap<SimpleReq,SimpleResp>
        (
            r
            , simpleTestFacility
            , "grpc_interop://127.0.0.1:34567:::grpc_interop_test/TestService/SimpleTest"
            , "simpleTestService"
        );

    r.finalize();
    infra::terminationController(infra::RunForever {});
}