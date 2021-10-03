#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/PrintHelper.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListenerManagingUtils.hpp>

#include <iostream>

#include "ReadOnlyDBOneListData.hpp"

using namespace dev::cd606::tm;

void graphBasedMain() {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::CrossGuidComponent,
        transport::rabbitmq::RabbitMQComponent/*,
        transport::ClientSideSimpleIdentityAttacherComponent<
            std::string
            , DBQuery
        >*/
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    R r(&env);

    auto heartbeatSource = 
        transport::MultiTransportBroadcastListenerManagingUtils<R>
        ::oneBroadcastListener<
            transport::HeartbeatMessage
        >(
            r 
            , "heartbeatListener"
            , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
            , "read_only_db_one_list_server.heartbeat"
        );
    auto remoteFacilityInfo = transport::MultiTransportRemoteFacilityManagingUtils<R>
        ::setupOneNonDistinguishedRemoteFacilityWithProtocol<basic::CBOR, DBQuery, DBQueryResult>(
            r 
            , heartbeatSource.clone()
            , std::regex("read_only_db_one_list_server")
            , "queryFacility"
        );
    auto facility = remoteFacilityInfo.facility;

    auto input = M::liftMaybe<std::size_t>(
        [](std::size_t &&s) -> std::optional<M::Key<DBQuery>> {
            if (s > 0) {
                return M::keyify(DBQuery {});
            } else {
                return std::nullopt;
            }
        }
        , infra::LiftParameters<std::chrono::system_clock::time_point>().FireOnceOnly(true)
    );
    r.registerAction("input", input);
    remoteFacilityInfo.feedUnderlyingCount(r, r.actionAsSink(input));

    auto handler = M::pureExporter<M::KeyedData<DBQuery,DBQueryResult>>(
        [&env](M::KeyedData<DBQuery,DBQueryResult> &&d) {
            std::ostringstream oss;
            oss << "Result is ";
            basic::PrintHelper<DBQueryResult>::print(oss, d.data);
            env.log(infra::LogLevel::Info, oss.str());
            env.exit();
        }
    );
    r.registerExporter("handler", handler);
    facility(r, r.actionAsSource(input), r.exporterAsSink(handler));

    r.finalize();
    infra::terminationController(infra::RunForever {});
}

void directCallBasedMain() {
    using TheEnvironment = infra::Environment<
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::CrossGuidComponent,
        transport::rabbitmq::RabbitMQComponent/*,
        transport::ClientSideSimpleIdentityAttacherComponent<
            std::string
            , DBQuery
        >*/
    >;

    TheEnvironment env;
    auto result = transport::OneShotMultiTransportRemoteFacilityCall<TheEnvironment>
        ::callWithProtocol<basic::CBOR, DBQuery, DBQueryResult>(
            &env 
            , "rabbitmq://127.0.0.1::guest:guest:test_db_read_only_one_list_queue"
            , DBQuery {}
        );

    std::ostringstream oss;
    oss << "Result is ";
    basic::PrintHelper<DBQueryResult>::print(oss, result.get());
    env.log(infra::LogLevel::Info, oss.str());
}

void heartbeatCallBasedMain() {
    using TheEnvironment = infra::Environment<
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::CrossGuidComponent,
        transport::rabbitmq::RabbitMQComponent/*,
        transport::ClientSideSimpleIdentityAttacherComponent<
            std::string
            , DBQuery
        >*/
    >;

    TheEnvironment env;
    auto result = transport::OneShotMultiTransportRemoteFacilityCall<TheEnvironment>
        ::callWithProtocolByHeartbeat<basic::CBOR, DBQuery, DBQueryResult>(
            &env 
            , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
            , "read_only_db_one_list_server.heartbeat"
            , std::regex("read_only_db_one_list_server")
            , "queryFacility"
            , DBQuery {}
        );
    std::ostringstream oss;
    oss << "Result is ";
    basic::PrintHelper<DBQueryResult>::print(oss, result.get());
    env.log(infra::LogLevel::Info, oss.str());
}

int main(int argc, char **argv) {
    //graphBasedMain();
    //directCallBasedMain();
    heartbeatCallBasedMain();
    return 0;
}
