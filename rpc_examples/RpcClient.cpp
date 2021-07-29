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

    {
        std::cout << "=============SIMPLE RPC====================\n";
        auto simpleCallRes = r.placeOrderWithFacility(
            rpc_examples::Input {5, "abc"}
            , transport::MultiTransportRemoteFacilityManagingUtils<SR>
                ::setupSimpleRemoteFacility<rpc_examples::Input, rpc_examples::Output>(
                "redis://127.0.0.1:6379:::rpc_example_simple"
            )
        )->front();
        std::cout << simpleCallRes << '\n';
    }

    {
        std::cout << "=============CLIENT STREAM RPC====================\n";
        auto streamer = r.facilityStreamer(
            transport::MultiTransportRemoteFacilityManagingUtils<SR>
                ::setupSimpleRemoteFacility<rpc_examples::Input, rpc_examples::Output>(
                "redis://127.0.0.1:6379:::rpc_example_client_stream"
            )
        );
        streamer << rpc_examples::Input {5, "abc"};
        streamer << rpc_examples::Input {-1, "bcd"};
        streamer << rpc_examples::Input {-2, "cde"};
        streamer << rpc_examples::Input {-3, "def"};
        streamer << rpc_examples::Input {-4, "efg"}; //total 5 inputs (as indicated by the first input)
        std::cout << streamer->front() << '\n';
    }

    {
        std::cout << "=============SERVER STREAM RPC====================\n";
        auto serverStreamCallRes = r.placeOrderWithFacility(
            rpc_examples::Input {5, "abc"}
            , transport::MultiTransportRemoteFacilityManagingUtils<SR>
                ::setupSimpleRemoteFacility<rpc_examples::Input, rpc_examples::Output>(
                "redis://127.0.0.1:6379:::rpc_example_server_stream"
            )
        );
        for (auto const &r : *serverStreamCallRes) {
            std::cout << r << '\n';
        }
    }

    {
        std::cout << "=============BOTH STREAM RPC====================\n";
        auto streamer = r.facilityStreamer(
            transport::MultiTransportRemoteFacilityManagingUtils<SR>
                ::setupSimpleRemoteFacility<rpc_examples::Input, rpc_examples::Output>(
                "redis://127.0.0.1:6379:::rpc_example_both_stream"
            )
        );
        streamer << rpc_examples::Input {5, "abc"};
        streamer << rpc_examples::Input {-1, "bcd"};
        streamer << rpc_examples::Input {-2, "cde"};
        streamer << rpc_examples::Input {-3, "def"};
        streamer << rpc_examples::Input {-4, "efg"}; //total 5 inputs (as indicated by the first input)
        /*
        while (!streamer->empty()) {
            std::cout << streamer->front() << '\n';
            streamer->pop_front();
        }
        */
        for (auto const &r : *streamer) {
            std::cout << r << '\n';
        }
    }
}