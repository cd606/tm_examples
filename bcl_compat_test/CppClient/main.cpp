#include "bcl_compat_test.pb.h"

#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>
#include <tm_kit/transport/bcl_compat/Decimal.hpp>
#include <tm_kit/transport/bcl_compat/Guid.hpp>

using namespace dev::cd606::tm;

int main(int argc, char **argv) {
    using Environment = infra::Environment<
        infra::CheckTimeComponent<false>
        , infra::TrivialExitControlComponent
        , basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>
        , transport::CrossGuidComponent
        , transport::AllNetworkTransportComponents
    >;
    using M = infra::RealTimeApp<Environment>;
    using R = infra::AppRunner<M>;

    Environment env;

    bcl_compat_test::Query q;
    auto id = Environment::new_id();
    transport::bcl_compat::GuidConverter<Environment>::write(*q.mutable_id(), id);
    boost::multiprecision::cpp_dec_float_100 value("-0.00000000123");
    transport::bcl_compat::DecimalConverter::write(*q.mutable_value(), value);
    q.set_description("cpp_client_test");

    auto resF = transport::OneShotMultiTransportRemoteFacilityCall<Environment>
        ::call<
            bcl_compat_test::Query 
            , bcl_compat_test::Result
        >(
            &env
            , "redis://127.0.0.1:6379:::bcl_test_queue"
            , std::move(q)
        );
    resF.wait();
    auto res = resF.get();

    std::cout << id << '\n';
    std::cout << value << '\n';
    std::cout << transport::bcl_compat::GuidConverter<Environment>::read(res.id()) << '\n';
    std::cout << transport::bcl_compat::DecimalConverter::read(res.value()) << '\n';
}