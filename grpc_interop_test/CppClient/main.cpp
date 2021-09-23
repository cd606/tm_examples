#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>
#include <tm_kit/infra/TerminationController.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/ProtoInterop.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/grpc_interop/GrpcClientFacility.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>
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
    , transport::TLSClientConfigurationComponent
    , transport::AllNetworkTransportComponents
>;
using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;
using CFU = basic::CommonFlowUtilComponents<M>;

int main(int argc, char **argv) {
    Env env;
    bool useSsl = (argc>=2 && std::string_view(argv[1]) == "ssl");
    if (useSsl) {
        env.transport::TLSClientConfigurationComponent::setConfigurationItem(
            transport::TLSClientInfoKey {
                "localhost"
                , 34567
            }
            , transport::TLSClientInfo {
                "../grpc_interop_test/DotNetServer/server.crt"
                , "../grpc_interop_test/DotNetClient/client.crt"
                , "../grpc_interop_test/DotNetClient/client.key"
            }
        );
    }
    R r(&env);

    //synchronous call first
    //Please note that we don't use the same OneShotCall methods
    //as in other transports.
    //The reason is that grpc interop will wait for
    //ALL results to come back in synchronous mode, while OneShotCall
    //was designed to only return one result.
    //Also, SynchronousRunner does not work well with grpc because
    //grpc does not return the last value marked as "final", so 
    //SynchronousRunner loop will not exit.
    //Therefore it is better to just use grpc-specific method for
    //synchronous calls
    Req req;
    req.intParam = 2;
    req.doubleListParam = std::vector<double> {1.0, 2.1, 3.2, 4.3, 5.4, 6.5, 7.6};
    auto result = transport::grpc_interop::GrpcClientFacilityFactory<M>
        ::runSyncClient<basic::proto_interop::Proto<Req>, basic::proto_interop::Proto<Resp>>(
            &env
            , transport::ConnectionLocator::parse("localhost:34567:::grpc_interop_test/TestService/Test")
            , {req}
        );
    for (auto const &resp : result) {
        std::cout << *resp << '\n';
    }
    SimpleReq req2;
    req2.input = 1;
    auto result2 = transport::OneShotMultiTransportRemoteFacilityCall<Env>
        ::call<SimpleReq, SimpleResp>(
            &env 
            , "grpc_interop://localhost:34567:::grpc_interop_test/TestService/SimpleTest"
            , std::move(req2)
        ).get();
    std::cout << result2 << '\n';

    //then asynchronous call

    //We specify the server location directly because we want
    //this to also work with the dotnet version server. If we
    //change it to use the heartbeat spec, that also works with
    //the cpp server, but since the dotnet server is not publishing
    //heartbeat, it will fail with the dotnet server.

    //for direct specification, it is also possible to use the
    //simpler setupSimpleRemoteFacility instead of setupSimpleRemoteFacilitioid
    //(the latter supports heartbeat spec).

    auto facility = transport::MultiTransportRemoteFacilityManagingUtils<R>::setupSimpleRemoteFacilitioid<
        Req, Resp
    >(
        r
        //, "grpc_interop://localhost:34567:::grpc_interop_test/TestService/Test"
        , transport::SimpleRemoteFacilitySpecByHeartbeat {
            "zeromq://localhost:12345"
            , "grpc_interop_test.heartbeat"
            , std::regex("grpc_interop_test_server")
            , "testFacility"
        }
        , "facility"
    );
    auto facility2 = transport::MultiTransportRemoteFacilityManagingUtils<R>::setupSimpleRemoteFacilitioid<
        SimpleReq, SimpleResp
    >(
        r
        //, "grpc_interop://localhost:34567:::grpc_interop_test/TestService/SimpleTest"
        , transport::SimpleRemoteFacilitySpecByHeartbeat {
            "zeromq://localhost:12345"
            , "grpc_interop_test.heartbeat"
            , std::regex("grpc_interop_test_server")
            , "simpleTestFacility"
        }
        , "facility2"
    );
    
    infra::DeclarativeGraph<R>("", {
        {"importer", infra::DelayImporter<basic::VoidStruct> {}, [](Env *env) -> std::tuple<bool, M::Data<Req>> {
            static int counter = 0;
            if (counter != 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            ++counter;
            Req req;
            req.intParam = counter;
            req.doubleListParam = std::vector<double> {1.0, 2.1, 3.2, 4.3, 5.4, 6.5, 7.6};
            return {
                (counter < 3)
                , M::InnerData<Req> {
                    env 
                    , {
                        env->now()
                        , req 
                        , (counter >= 3)
                    }
                }
            };
        }}
        , {"keyify", CFU::keyify<Req>()}
        , {"exporter", [&env](M::KeyedData<Req, Resp> &&data) {
            std::ostringstream oss;
            oss << data.data;
            env.log(infra::LogLevel::Info, oss.str());
        }}
        , {facility.facilityFirstReady.clone(), "importer"}
        , {"importer", "keyify"}
        , {"keyify", facility.facility, "exporter"}
        , {"importer2", infra::DelayImporter<basic::VoidStruct> {}, [](Env *env) -> std::tuple<bool, M::Data<SimpleReq>> {
            static int counter = 0;
            if (counter != 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            ++counter;
            SimpleReq req;
            req.input = counter;
            if (counter%2 == 0) {
                req.reqOneOf.emplace<1>(std::string("abc"));
            } else {
                req.reqOneOf.emplace<2>(0.1f*counter);
            }
            req.name2 = std::string(counter, 'x');
            req.anotherInput.push_back((uint32_t) counter-1);
            req.anotherInput.push_back((uint32_t) counter);
            req.anotherInput.push_back((uint32_t) counter+1);
            std::ostringstream oss;
            oss << "SimpleReq: " << req;
            env->log(infra::LogLevel::Info, oss.str());
            return {
                (counter < 3)
                , M::InnerData<SimpleReq> {
                    env 
                    , {
                        env->now()
                        , req 
                        , (counter >= 3)
                    }
                }
            };
        }}
        , {"keyify2", CFU::keyify<SimpleReq>()}
        , {"exporter2", [&env](M::KeyedData<SimpleReq, SimpleResp> &&data) {
            std::ostringstream oss;
            oss << data.data;
            env.log(infra::LogLevel::Info, oss.str());
        }}
        , {facility2.facilityFirstReady.clone(), "importer2"}
        , {"importer2", "keyify2"}
        , {"keyify2", facility2.facility, "exporter2"}
    })(r);
    r.finalize();
    infra::terminationController(infra::TerminateAfterDuration {std::chrono::seconds(15)});
}
