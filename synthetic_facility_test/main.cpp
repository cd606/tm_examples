#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>
#include <tm_kit/infra/GraphStructureBasedResourceHolderComponent.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/NlohmannJsonInterop.hpp>
#include <tm_kit/basic/ProtoInterop.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>

#include <tm_kit/transport/SyntheticMultiTransportFacility.hpp>
#include <tm_kit/transport/CrossGuidComponent.hpp>

using namespace dev::cd606::tm;

using Env = infra::Environment<
    infra::CheckTimeComponent<false> 
    , infra::TrivialExitControlComponent 
    , infra::GraphStructureBasedResourceHolderComponent
    , basic::TimeComponentEnhancedWithSpdLogging<
        basic::real_time_clock::ClockComponent
    >
    , transport::CrossGuidComponent
    , transport::AllNetworkTransportComponents
>;
using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;

void client() {
    Env env;
    R r(&env);

    auto c = transport::SyntheticMultiTransportFacility<R>
        ::client<
            basic::nlohmann_json_interop::Json, basic::proto_interop::Proto 
            , int, double 
        >(
            r 
            , "clientFacility"
            , "multicast://224.0.0.1:34567:::test" 
            , "synthetic_test_input"
            , "zeromq://localhost:12345:::test"
            , "synthetic_test_output"
        );
    infra::DeclarativeGraph<R>("", {
        {"importer", [](Env *env) -> std::tuple<bool, M::Data<int>> {
            static int x = 0;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return {
                true 
                , M::InnerData<int> {
                    env 
                    , {
                        env->now() 
                        , (x++)
                        , false
                    }
                }
            };
        }}
        , {"keyify", basic::CommonFlowUtilComponents<M>::keyify<int>()}
        , {"exporter", [&env](M::KeyedData<int,double> &&x) {
            std::ostringstream oss;
            oss << "Result: " << x.data << " from input " << x.key.key() << '(' << *(env.currentNodeName()) << ')';
            env.log(infra::LogLevel::Info, oss.str());
        }}
        , {"importer", "keyify"}
        , {"keyify", c, "exporter"}
    })(r);
    r.finalize();

    infra::terminationController(infra::RunForever {});
}

void server() {
    Env env;
    R r(&env);

    using GL = infra::GenericLift<M>;
    auto f = GL::liftFacility([](int x) -> double {
        return x*2.0;
    });
    r.registerOnOrderFacility("facility", f);
    transport::SyntheticMultiTransportFacility<R>
        ::server<
            basic::nlohmann_json_interop::Json, basic::proto_interop::Proto 
            , int, double 
        >(
            r 
            , "serverFacility"
            , "multicast://224.0.0.1:34567:::test" 
            , "synthetic_test_input"
            , "zeromq://localhost:12345:::test"
            , "synthetic_test_output"
            , r.facilityConnector(f)
        );
    r.finalize();

    infra::terminationController(infra::RunForever {});
}

void multi_client() {
    Env env;
    R r(&env);

    auto c = transport::SyntheticMultiTransportFacility<R>
        ::client<
            basic::nlohmann_json_interop::Json, basic::proto_interop::Proto 
            , int, double 
            , true
        >(
            r 
            , "clientFacility"
            , "multicast://224.0.0.1:34567:::test" 
            , "synthetic_test_input"
            , "zeromq://localhost:12345:::test"
            , "synthetic_test_output"
        );
    infra::DeclarativeGraph<R>("", {
        {"importer", [](Env *env) -> std::tuple<bool, M::Data<int>> {
            static int x = 0;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return {
                true 
                , M::InnerData<int> {
                    env 
                    , {
                        env->now() 
                        , (x++)
                        , false
                    }
                }
            };
        }}
        , {"keyify", basic::CommonFlowUtilComponents<M>::keyify<int>()}
        , {"exporter", [&env](M::InnerData<M::KeyedData<int,double>> &&x) {
            std::ostringstream oss;
            oss << "Result: " << x.timedData.value.data << " from input " << x.timedData.value.key.key() << '(' << *(env.currentNodeName()) << ')';
            if (x.timedData.finalFlag) {
                oss << " [F]";
            }
            env.log(infra::LogLevel::Info, oss.str());
        }}
        , {"importer", "keyify"}
        , {"keyify", c, "exporter"}
    })(r);
    r.finalize();

    infra::terminationController(infra::RunForever {});
}

void multi_server() {
    Env env;
    R r(&env);

    class F : public M::AbstractOnOrderFacility<int, double> {
    public:
        void handle(M::InnerData<M::Key<int>> &&d) {
            std::thread th([this,d=std::move(d)]() {
                publish(d.environment, M::Key<double> {d.timedData.value.id(), d.timedData.value.key()*-2.0}, false);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                publish(d.environment, M::Key<double> {d.timedData.value.id(), d.timedData.value.key()*2.0}, true);
            });
            th.detach();
        }
    };
    auto f = M::fromAbstractOnOrderFacility(new F());
    r.registerOnOrderFacility("facility", f);
    transport::SyntheticMultiTransportFacility<R>
        ::server<
            basic::nlohmann_json_interop::Json, basic::proto_interop::Proto 
            , int, double 
            , true
        >(
            r 
            , "serverFacility"
            , "multicast://224.0.0.1:34567:::test" 
            , "synthetic_test_input"
            , "zeromq://localhost:12345:::test"
            , "synthetic_test_output"
            , r.facilityConnector(f)
        );
    r.finalize();

    infra::terminationController(infra::RunForever {});
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: synthetic_facility_test client|server|multi-client|multi-server\n";
        return -1;
    }
    if (std::string_view(argv[1]) == "client") {
        client();
    } else if (std::string_view(argv[1]) == "server") {
        server();
    } else if (std::string_view(argv[1]) == "multi-client") {
        multi_client();
    } else if (std::string_view(argv[1]) == "multi-server") {
        multi_server();
    } else {
        std::cerr << "Usage: synthetic_facility_test client|server\n";
        return -1;
    }
    return 0;
}
