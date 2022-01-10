#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>

#include <tm_kit/transport/SyntheticMultiTransportFacility.hpp>
#include <tm_kit/transport/CrossGuidComponent.hpp>

using namespace dev::cd606::tm;

using Env = infra::Environment<
    infra::CheckTimeComponent<false> 
    , infra::TrivialExitControlComponent 
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
            basic::CBOR, basic::CBOR 
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
            oss << "Result: " << x.data << " from input " << x.key.key();
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
        ::serverWithFacility<
            basic::CBOR, basic::CBOR 
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

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: synthetic_facility_test client|server\n";
        return -1;
    }
    if (std::string_view(argv[1]) == "client") {
        client();
    } else if (std::string_view(argv[1]) == "server") {
        server();
    } else {
        std::cerr << "Usage: synthetic_facility_test client|server\n";
        return -1;
    }
    return 0;
}