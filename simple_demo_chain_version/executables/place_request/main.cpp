#include "simple_demo_chain_version/main_program_logic/MainProgramStateFolder.hpp"
#include "simple_demo_chain_version/security_keys/VerifyingKeys.hpp"

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/SharedChainCreator.hpp>
#include <tm_kit/transport/security/SignatureAndVerifyHookFactoryComponents.hpp>

using namespace simple_demo_chain_version;

int main(int argc, char **argv) {
    const transport::security::SignatureHelper::PrivateKey placeRequestKey = {
        0x66,0x69,0x02,0xBB,0xB4,0x42,0xA2,0x8F,0x6D,0x05,0x41,0x56,0x87,0x6E,0xBA,0x8F,
        0x53,0x77,0xB8,0xAC,0xBE,0xA8,0xFD,0x5A,0x16,0x7B,0xFC,0x9A,0xB8,0xEB,0x54,0x2A,
        0xDE,0x1D,0xFB,0x0C,0x22,0x40,0x96,0xFD,0x90,0x3F,0x7D,0x36,0x10,0xA6,0xDA,0x75,
        0xE5,0x2A,0xE0,0x01,0xC3,0x5A,0x1D,0xE9,0x3D,0x72,0xF6,0xB8,0xA2,0x36,0xC1,0x9E
    };
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
        transport::lock_free_in_memory_shared_chain::SharedMemoryChainComponent,
        transport::security::SignatureWithNameHookFactoryComponent<ChainData>,
        transport::security::VerifyUsingNameTagHookFactoryComponent<ChainData>
    >;
    using M = infra::RealTimeApp<TheEnvironment>;

    TheEnvironment env;
    env.transport::security::SignatureWithNameHookFactoryComponent<ChainData>::operator=(
        transport::security::SignatureWithNameHookFactoryComponent<ChainData> {
            "place_request"
            , placeRequestKey
        }
    );
    env.transport::security::VerifyUsingNameTagHookFactoryComponent<ChainData>::operator=(
        transport::security::VerifyUsingNameTagHookFactoryComponent<ChainData> {
            verifyingKeys
        }
    );

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