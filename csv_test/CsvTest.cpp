#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/SynchronousRunner.hpp>
#include <tm_kit/infra/Environments.hpp>

#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/basic/StructFieldInfoBasedCsvUtils.hpp>
#include <tm_kit/basic/top_down_single_pass_iteration_clock/ClockComponent.hpp>

#ifdef _MSC_VER
    #define TestDataFields \
        ((std::string, name)) \
        ((int32_t, amount)) \
        ((double, stat)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::array<float, 5>), moreData)) \
        ((std::tm, theTime))
#else
    #define TestDataFields \
        ((std::string, name)) \
        ((int32_t, amount)) \
        ((double, stat)) \
        (((std::array<float, 5>), moreData)) \
        ((std::tm, theTime))
#endif

TM_BASIC_CBOR_CAPABLE_STRUCT(test_data, TestDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(test_data, TestDataFields);

using namespace dev::cd606::tm;

struct TrivialLoggingComponent {
    static inline void log(infra::LogLevel l, std::string const &s) {
        std::cout << l << ": " << s << std::endl;
    }
};

using BasicEnvironment = infra::Environment<
    infra::CheckTimeComponent<true>,
    infra::FlagExitControlComponent,
    TrivialLoggingComponent,
    basic::top_down_single_pass_iteration_clock::ClockComponent<std::chrono::system_clock::time_point>
>;
using M = infra::TopDownSinglePassIterationApp<BasicEnvironment>;
using SR = infra::SynchronousRunner<M>;

int main() {
    BasicEnvironment env;
    SR r(&env);

    test_data d;
    basic::struct_field_info_utils::StructFieldInfoBasedInitializer<test_data>::initialize(d);
    d.name = "abc\"\\,,t,est";
    d.amount = 1;
    d.stat = 2.3;
    d.moreData[2] = 0.5f;
    d.theTime.tm_year=102;

    std::stringstream ss;
    auto ex = basic::struct_field_info_utils::StructFieldInfoBasedCsvExporterFactory<M>
        ::createExporter<test_data>(ss);
    r.exportItem(ex, std::move(d));

    d.name="bcd\"   ,cd";
    d.amount = 2;
    d.stat = 3.4;
    d.moreData[1] = 0.2f;
    r.exportItem(ex, std::move(d));

    std::cout << ss.str() << "\n";

    auto im = basic::struct_field_info_utils::StructFieldInfoBasedCsvImporterFactory<M>
        ::createImporter<test_data>(
            ss
            , [](BasicEnvironment *e, test_data const &) {return e->now();}
            , basic::struct_field_info_utils::StructFieldInfoBasedCsvInputOption::UseHeaderAsDict
        );
    auto res = r.importItem(im);
    for (auto const &d1 : *res) {
        std::cout << d1.timedData.value << '\n';
    }
    /*
    std::vector<test_data> x;
    basic::struct_field_info_utils::StructFieldInfoBasedSimpleCsvInput<test_data>
        ::readInto(ss, std::back_inserter(x));
    for (auto const &d1 : x) {
        std::cout << d1 << '\n';
    }*/
}