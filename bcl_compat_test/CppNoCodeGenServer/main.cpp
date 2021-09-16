#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/MultiTransportFacilityWrapper.hpp>

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
    R r(&env);

    auto facility = M::liftPureOnOrderFacility<
        basic::proto_interop::Proto<bcl_compat_test::QueryNoCodeGen<Environment>>
    >(
        [](basic::proto_interop::Proto<bcl_compat_test::QueryNoCodeGen<Environment>> &&q) -> basic::proto_interop::Proto<bcl_compat_test::ResultNoCodeGen<Environment>> {
            std::cout << q->id << '\n';
            auto value = *(q->value);
            std::cout << value << '\n';
            for (auto const &f : q->floatArr.value) {
                std::cout << '\t' << f << '\n';
            }
            
            value *= 2.0;
            
            basic::proto_interop::Proto<bcl_compat_test::ResultNoCodeGen<Environment>> r;
            r->id = q->id;
            r->value.value = value;
            r->messages.value.push_back(q->description.value);

            return r;
        }
    );
    r.registerOnOrderFacility("facility", facility);
    transport::MultiTransportFacilityWrapper<R>::wrap<
        basic::proto_interop::Proto<bcl_compat_test::QueryNoCodeGen<Environment>> 
        , basic::proto_interop::Proto<bcl_compat_test::ResultNoCodeGen<Environment>>
    >(
        //r, facility, "redis://127.0.0.1:6379:::bcl_test_queue", "wrapper/"
        r, facility, "rabbitmq://127.0.0.1::guest:guest:bcl_test_queue", "wrapper/"
    );
    
    r.finalize();
    infra::terminationController(infra::RunForever {});

    return 0;
}