#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/SynchronousRunner.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils_SynchronousRunner.hpp>

#include "RpcInterface.hpp"

using namespace dev::cd606::tm;

using Environment = infra::Environment<
    infra::CheckTimeComponent<true>,
    infra::TrivialExitControlComponent,
    basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
    transport::CrossGuidComponent,
    transport::AllNetworkTransportComponents
>;
using M = infra::RealTimeApp<Environment>;
using SR = infra::SynchronousRunner<M>;

int main() {
    Environment env;
    SR r(&env);

    std::vector<std::tuple<
        std::string, std::string, bool
    >> descriptors {
        /*
        {"redis://127.0.0.1:6379:::rpc_example_simple", "SIMPLE RPC", false}
        , {"redis://127.0.0.1:6379:::rpc_example_client_stream", "CLIENT STREAM RPC", true}
        , {"redis://127.0.0.1:6379:::rpc_example_server_stream", "SERVER STREAM RPC", false}
        , {"redis://127.0.0.1:6379:::rpc_example_both_stream", "BOTH STREAM RPC", true}
        */     
        {"nats://127.0.0.1:4222:::rpc_example_simple", "SIMPLE RPC", false}
        , {"nats://127.0.0.1:4222:::rpc_example_client_stream", "CLIENT STREAM RPC", true}
        , {"nats://127.0.0.1:4222:::rpc_example_server_stream", "SERVER STREAM RPC", false}
        , {"nats://127.0.0.1:4222:::rpc_example_both_stream", "BOTH STREAM RPC", true}        
    };

    for (auto const &desc : descriptors) {
        std::cout << "=============" << std::get<1>(desc) << "====================\n";
        auto streamer = r.facilityStreamer(
            transport::MultiTransportRemoteFacilityManagingUtils<SR>
                ::setupSimpleRemoteFacility<rpc_examples::Input, rpc_examples::Output>(
                std::get<0>(desc)
            )
        );
        streamer << rpc_examples::Input {5, "abc"};
        if (std::get<2>(desc)) {
            streamer << rpc_examples::Input {-1, "bcd"};
            streamer << rpc_examples::Input {-2, "cde"};
            streamer << rpc_examples::Input {-3, "def"};
            streamer << rpc_examples::Input {-4, "efg"}; //total 5 inputs for the ones 
                                                         //requiring client-side streaming
                                                         //(as indicated by the first input)
        }
        //The two ways of accessing the result below are equivalent in this case.
        //The first way would be more useful if more control (e.g. timeout, 
        //interleaving read/write) is desired, while the second way is more straightforward.
        /*
        while (!streamer->empty()) {
            std::cout << streamer->front().timedData.value.data << '\n';
            streamer->pop_front();
        }
        */
        for (auto const &d : *streamer) {
            std::cout << d.timedData.value.data << '\n';
        }
    }
}