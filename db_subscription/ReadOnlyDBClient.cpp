#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/PrintHelper.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQOnOrderFacility.hpp>

#include <iostream>

#include "ReadOnlyDBData.hpp"

using namespace dev::cd606::tm;

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: read_only_db_client name_of_record\n";
        return 1;
    }

    using TheEnvironment = infra::Environment<
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::CrossGuidComponent,
        transport::rabbitmq::RabbitMQComponent
    >;

    TheEnvironment env;

    auto result = transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>
        ::typedOneShotRemoteCall<DBQuery, DBQueryResult>(
            &env 
            , transport::ConnectionLocator::parse("127.0.0.1::guest:guest:test_db_read_only_queue")
            , DBQuery {std::string(argv[1])}
        );

    std::ostringstream oss;
    oss << "Result is ";
    basic::PrintHelper<DBQueryResult>::print(oss, result.get());
    env.log(infra::LogLevel::Info, oss.str());

    return 0;
}
