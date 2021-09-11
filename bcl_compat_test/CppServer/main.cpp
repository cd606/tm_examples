#include "bcl_compat_test.pb.h"

#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>
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
    R r(&env);

    auto facility = M::liftPureOnOrderFacility<
        bcl_compat_test::Query 
    >(
        [](bcl_compat_test::Query &&q) -> bcl_compat_test::Result {
            auto id = transport::bcl_compat::GuidConverter<Environment>::read(q.id());
            std::cout << id << '\n';
            auto value = transport::bcl_compat::DecimalConverter::read(q.value());
            std::cout << value << '\n';
            for (auto ii=0; ii<q.floatarr_size(); ++ii) {
                std::cout << '\t' << q.floatarr(ii) << '\n';
            }
            
            value *= 2.0;
            
            bcl_compat_test::Result r;
            transport::bcl_compat::GuidConverter<Environment>::write(*r.mutable_id(), id);
            transport::bcl_compat::DecimalConverter::write(*r.mutable_value(), value);
            *(r.add_messages()) = q.description();

            return r;
        }
    );
    r.registerOnOrderFacility("facility", facility);
    transport::MultiTransportFacilityWrapper<R>::wrap<
        bcl_compat_test::Query 
        , bcl_compat_test::Result
    >(
        //r, facility, "redis://127.0.0.1:6379:::bcl_test_queue", "wrapper/"
        r, facility, "rabbitmq://127.0.0.1::guest:guest:bcl_test_queue", "wrapper/"
    );
    
    r.finalize();
    infra::terminationController(infra::RunForever {});

    return 0;
}