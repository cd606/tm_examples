#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/SynchronousRunner.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/top_down_single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/IntIDComponent.hpp>

#include "RpcInterface.hpp"
#include "RpcServer.hpp"

using namespace dev::cd606::tm;

using Environment = infra::Environment<
    infra::CheckTimeComponent<true>,
    infra::TrivialExitControlComponent,
    basic::TimeComponentEnhancedWithSpdLogging<basic::top_down_single_pass_iteration_clock::ClockComponent<int>,false>,
    basic::IntIDComponent<int>
>;
using M = infra::TopDownSinglePassIterationApp<Environment>;
using SR = infra::SynchronousRunner<M>;

int main() {
    Environment env;
    SR r(&env);

    using FacilityCreator = SR::OnOrderFacilityPtr<rpc_examples::Input,rpc_examples::Output> (*)();
    std::vector<std::tuple<
        FacilityCreator, std::string, bool
    >> specs {
        {&rpc_examples::simpleFacility<SR>, "SIMPLE RPC", false}
        , {&rpc_examples::clientStreamFacility<SR>, "CLIENT STREAM RPC", true}
        , {&rpc_examples::serverStreamFacility<SR>, "SERVER STREAM RPC", false}
        , {&rpc_examples::bothStreamFacility<SR>, "BOTH STREAM RPC", true}
    };

    for (auto const &spec : specs) {
        std::cout << "=============" << std::get<1>(spec) << "====================\n";
        auto streamer = r.facilityStreamer(
            std::get<0>(spec)()
        );
        streamer << rpc_examples::Input {5, "abc"};
        if (std::get<2>(spec)) {
            streamer << rpc_examples::Input {-1, "bcd"};
            streamer << rpc_examples::Input {-2, "cde"};
            streamer << rpc_examples::Input {-3, "def"};
            streamer << rpc_examples::Input {-4, "efg"}; 
        }
        for (auto const &d : *streamer) {
            std::cout << d.timedData.value.data << '\n';
        }
    }
}
