#include "simple_demo_chain_version/main_program_logic/MainProgramStateFolder.hpp"

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/SharedChainCreator.hpp>

using namespace simple_demo_chain_version;

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: place_request value\n";
        return 1;
    }
    double value = std::stof(argv[1]);

    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<
            basic::real_time_clock::ClockComponent
        >,
        transport::CrossGuidComponent,
        transport::lock_free_in_memory_shared_chain::SharedMemoryChainComponent
    >;
    using M = infra::RealTimeApp<TheEnvironment>;

    TheEnvironment env;

    //setting up chain
    std::string today = infra::withtime_utils::localTimeString(std::chrono::system_clock::now()).substr(0,10);
    std::ostringstream chainLocatorOss;
    chainLocatorOss << "in_shared_memory://::::" << today << "-simple-demo-chain[size=" << (100*1024*1024) << "]";
    std::string chainLocatorStr = chainLocatorOss.str();

    //Please note that this object should not be allowed to go out of scope
    transport::SharedChainCreator<M> sharedChainCreator;

    int id = -1;
    if (sharedChainCreator.oneShotWrite<
        ChainData 
        , main_program_logic::MainProgramStateFolder
    >(
        &env 
        , chainLocatorStr
        , [&env,value,&id](main_program_logic::MainProgramState const &s) -> std::optional<std::tuple<std::string, ChainData>> {
            int64_t now = infra::withtime_utils::sinceEpoch<std::chrono::milliseconds>(env.now());
            id = s.max_id_sofar+1;
            PlaceRequest r {
                id
                , value
            };
            return std::tuple<std::string, ChainData> {
                ""
                , ChainData {now, r}
            };
        }
    )) {
        env.log(infra::LogLevel::Info, "Request placed with id "+std::to_string(id)+" and value "+std::to_string(value));
    }
    
    return 0;
} 