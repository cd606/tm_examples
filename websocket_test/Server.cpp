#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/SerializationHelperMacros.hpp>

#include <tm_kit/transport/TLSConfigurationComponent.hpp>
#include <tm_kit/transport/MultiTransportBroadcastPublisherManagingUtils.hpp>
#include <tm_kit/transport/MultiTransportTouchups.hpp>

#define DATA_FIELDS \
    ((std::chrono::system_clock::time_point, now)) \
    ((std::string, textData)) \
    ((uint64_t, intData)) \
    ((std::vector<double>, doubleData)) \
    (((std::variant<int,float>), variantData)) \
    (((std::tuple<bool,int>), tupleData))

TM_BASIC_CBOR_CAPABLE_STRUCT(Data, DATA_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(Data, DATA_FIELDS);

#undef DATA_FIELDS

using namespace dev::cd606::tm;

using Env = infra::Environment<
    infra::CheckTimeComponent<false>
    , infra::TrivialExitControlComponent
    , basic::TimeComponentEnhancedWithSpdLogging<
        basic::real_time_clock::ClockComponent
        , false
    >
    , transport::TLSServerConfigurationComponent
    , transport::AllNetworkTransportComponents
>;
using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;

int main(int argc, char **argv) {
    Env env; 
    R r(&env);

    env.transport::TLSServerConfigurationComponent::setConfigurationItem(
        transport::TLSServerInfoKey {
            34567
        }
        , transport::TLSServerInfo {
            "../grpc_interop_test/DotNetServer/server.crt"
            , "../grpc_interop_test/DotNetServer/server.key"
        }
    );

    transport::multi_transport_touchups::PublisherTouchupWithProtocol<
        R, basic::nlohmann_json_interop::Json, Data
    >(
        r 
        , {
            "websocket://localhost:34567[ignoreTopic=true]"
        }
    );

    
    transport::multi_transport_touchups::PublisherTouchupWithProtocol<
        R, std::void_t, Data
    >(
        r 
        , {
            "websocket://localhost:45678"
            , std::nullopt 
            , false 
            , "cbor_websocket"
        }
    );


    auto timerInput = basic::real_time_clock::ClockImporter<Env>
        ::createRecurringClockImporter<basic::TypedDataWithTopic<Data>>
    (
        std::chrono::system_clock::now()
        , std::chrono::system_clock::now()+std::chrono::minutes(1)
        , std::chrono::seconds(2)
        , [](std::chrono::system_clock::time_point const &t) -> basic::TypedDataWithTopic<Data> {
            static int ii=0;
            ++ii;
            std::variant<int,float> v;
            if (ii%2==0) {
                v.emplace<0>(ii);
            } else {
                v.emplace<1>(ii+0.5f);
            }
            return {
                "test"
                , {
                    t 
                    , "abc"
                    , (uint64_t) (ii*2)
                    , {ii*3.0, ii*4.0, ii*5.0}
                    , v
                    , {(ii%2==0),ii}
                }
            };
        }
    );

    r.registerImporter("timerInput", timerInput);

    r.finalize();

    infra::terminationController(infra::TerminateAfterDuration {std::chrono::seconds(62)});

    return 0;
}
