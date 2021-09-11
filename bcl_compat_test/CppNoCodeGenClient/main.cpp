#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>

#include "../CppNoCodeGenShare/CppNoCodeGenStruct.hpp"

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

    basic::proto_interop::Proto<bcl_compat_test::QueryNoCodeGen<Environment>> q;
    q->value = boost::multiprecision::cpp_dec_float_100("-0.00000000123");
    q->description = "cpp_client_test";
    q->floatArr = {1.0f, 2.0f};

    std::cout << q->id << '\n';
    std::cout << q->value << '\n';

    auto resF = transport::OneShotMultiTransportRemoteFacilityCall<Environment>
        ::call<
            basic::proto_interop::Proto<bcl_compat_test::QueryNoCodeGen<Environment>>
            , basic::proto_interop::Proto<bcl_compat_test::ResultNoCodeGen<Environment>>
        >(
            &env
            //, "redis://127.0.0.1:6379:::bcl_test_queue"
            , "rabbitmq://127.0.0.1::guest:guest:bcl_test_queue"
            , std::move(q)
        );
    resF.wait();
    auto res = resF.get();

    std::cout << res->id << '\n';
    std::cout << res->value << '\n';
}