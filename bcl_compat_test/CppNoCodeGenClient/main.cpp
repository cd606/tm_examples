#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/NlohmannJsonInterop.hpp>
#include <tm_kit/basic/StructFieldInfoBasedCsvUtils.hpp>
#include <tm_kit/basic/StructFieldInfoBasedCopy.hpp>

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
    q->value = "-0.00000000123";
    q->description.value = "cpp_client_test";
    q->floatArr.value = {1.0f, 2.0f};
    q->ts = std::chrono::milliseconds(12345);
    q->dt = std::chrono::system_clock::now();

    std::cout << q->id << '\n';
    std::cout << q->value << '\n';
    std::cout << '\n';

    bcl_compat_test::SmallQueryNoCodeGen<Environment> sq;
    basic::struct_field_info_utils::StructuralCopy::copy(sq, *q);
    basic::struct_field_info_utils::StructFieldInfoBasedSimpleCsvOutput<bcl_compat_test::SmallQueryNoCodeGen<Environment>>
        ::writeHeader(std::cout);
    basic::struct_field_info_utils::StructFieldInfoBasedSimpleCsvOutput<bcl_compat_test::SmallQueryNoCodeGen<Environment>>
        ::writeData(std::cout, sq);
    std::cout << '\n';

    basic::nlohmann_json_interop::Json<bcl_compat_test::QueryNoCodeGen<Environment> *> q1(&(*q));
    std::string q1Str;
    q1.writeToString(&q1Str);
    std::cout << q1Str;
    std::cout << "\n\n";

    std::cout << "-->Parse\n";
    bcl_compat_test::QueryNoCodeGen<Environment> q2;
    basic::nlohmann_json_interop::Json<bcl_compat_test::QueryNoCodeGen<Environment> *> q2_2(&q2);
    q2_2.fromString(q1Str);
    basic::PrintHelper<bcl_compat_test::QueryNoCodeGen<Environment>>::print(std::cout, q2);
    std::cout << "\n\n";
    
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
    std::cout << res->value.value << '\n';
    basic::PrintHelper<bcl_compat_test::ResultNoCodeGen<Environment>>::print(std::cout, *res);
    std::cout << '\n';

    bcl_compat_test::SmallResultNoCodeGen<Environment> sr;
    basic::struct_field_info_utils::StructuralCopy::copy(sr, *res);
    basic::struct_field_info_utils::StructFieldInfoBasedSimpleCsvOutput<bcl_compat_test::SmallResultNoCodeGen<Environment>>
        ::writeHeader(std::cout);
    basic::struct_field_info_utils::StructFieldInfoBasedSimpleCsvOutput<bcl_compat_test::SmallResultNoCodeGen<Environment>>
        ::writeData(std::cout, sr);
    std::cout << '\n';

    basic::nlohmann_json_interop::Json<bcl_compat_test::ResultNoCodeGen<Environment> *> res1(&(*res));
    res1.writeToStream(std::cout);
    std::cout << "\n\n";
}