#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/SynchronousRunner.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/top_down_single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/empty_clock/ClockComponent.hpp>
#include <tm_kit/basic/IntIDComponent.hpp>

#include "RpcInterface.hpp"
#include "RpcServer.hpp"

using namespace dev::cd606::tm;

template <class M>
void run() {
    using SR = infra::SynchronousRunner<M>;
    typename M::EnvironmentType env;
    SR r(&env);

    {
        std::cout << "=============SIMPLE RPC====================\n";
        auto simpleCallRes = r.placeOrderWithFacility(
            rpc_examples::Input {5, "abc"}
            , rpc_examples::simpleFacility<SR>()
        )->front();
        std::cout << simpleCallRes << '\n';
    }

    {
        std::cout << "=============CLIENT STREAM RPC====================\n";
        auto streamer = r.facilityStreamer(
            rpc_examples::clientStreamFacility<SR>()
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
            , rpc_examples::serverStreamFacility<SR>()
        );
        for (auto const &r : *serverStreamCallRes) {
            std::cout << r << '\n';
        }
    }

    {
        std::cout << "=============BOTH STREAM RPC====================\n";
        auto streamer = r.facilityStreamer(
            rpc_examples::bothStreamFacility<SR>()
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

template <class ClockComponent>
using Environment = infra::Environment<
    infra::CheckTimeComponent<true>,
    infra::TrivialExitControlComponent,
    basic::TimeComponentEnhancedWithSpdLogging<ClockComponent,false>,
    basic::IntIDComponent<int>
>;

int main(int argc, char **argv) {
    if (argc == 1 || std::string_view(argv[1]) == "basic") {
        run<infra::BasicWithTimeApp<
            Environment<basic::empty_clock::ClockComponent<>>
        >>();
    /*
    } else if (std::string_view(argv[1]) == "sp") {
        run<infra::SinglePassIterationApp<
            Environment<basic::single_pass_iteration_clock::ClockComponent<int>>
        >>();
    */
    } else if (std::string_view(argv[1]) == "tsp") {
        run<infra::TopDownSinglePassIterationApp<
            Environment<basic::top_down_single_pass_iteration_clock::ClockComponent<int>>
        >>();
    }
    return 0;
}