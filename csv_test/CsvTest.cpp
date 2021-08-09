#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/SynchronousRunner.hpp>
#include <tm_kit/infra/Environments.hpp>

#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/basic/StructFieldInfoBasedCsvUtils.hpp>
#include <tm_kit/basic/StructFieldFlattenedInfo.hpp>
#include <tm_kit/basic/StructFieldInfoBasedCopy.hpp>
#include <tm_kit/basic/top_down_single_pass_iteration_clock/ClockComponent.hpp>

#define InsideDataFields \
    ((std::string, s)) \
    ((double, stat)) \
    ((std::optional<int16_t>, i)) 

#ifdef _MSC_VER
    #define TestDataFields \
        ((std::string, name)) \
        ((int32_t, amount)) \
        ((std::optional<inside_data>, inside)) \
        ((inside_data, inside2)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::array<inside_data, 5>), moreData)) \
        ((std::tm, theTime)) \
        ((std::chrono::system_clock::time_point, tp))
#else
    #define TestDataFields \
        ((std::string, name)) \
        ((int32_t, amount)) \
        ((std::optional<inside_data>, inside)) \
        ((inside_data, inside2)) \
        (((std::array<inside_data, 5>), moreData)) \
        ((std::tm, theTime)) \
        ((std::chrono::system_clock::time_point, tp))
#endif

#define SmallInsideDataFields \
    ((double, i)) \
    ((std::string, s))
#ifdef _MSC_VER
    #define SmallTestDataFields \
        ((small_inside_data, inside)) \
        ((std::string, name)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::array<std::optional<small_inside_data>,3>), moreData)) \
        ((std::tm, tp))
    #define TestData2Fields \
        ((std::string, name)) \
        ((int32_t, amount)) \
        ((std::optional<inside_data>, inside)) \
        ((std::string, inside2_s)) \
        ((double, inside2_stat)) \
        ((std::optional<int16_t>, inside2_i)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::array<inside_data, 5>), moreData)) \
        ((std::tm, theTime)) \
        ((std::chrono::system_clock::time_point, tp))
#else
    #define SmallTestDataFields \
        ((small_inside_data, inside)) \
        ((std::string, name)) \
        (((std::array<std::optional<small_inside_data>,3>), moreData)) \
        ((std::tm, tp))
    #define TestData2Fields \
        ((std::string, name)) \
        ((int32_t, amount)) \
        ((std::optional<inside_data>, inside)) \
        ((std::string, inside2_s)) \
        ((double, inside2_stat)) \
        ((std::optional<int16_t>, inside2_i)) \
        (((std::array<inside_data, 5>), moreData)) \
        ((std::tm, theTime)) \
        ((std::chrono::system_clock::time_point, tp))
#endif

TM_BASIC_CBOR_CAPABLE_STRUCT(inside_data, InsideDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(inside_data, InsideDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT(test_data, TestDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(test_data, TestDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT(small_inside_data, SmallInsideDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(small_inside_data, SmallInsideDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT(small_test_data, SmallTestDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(small_test_data, SmallTestDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT(test_data_2, TestData2Fields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(test_data_2, TestData2Fields);

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
    d.moreData[2].stat = 0.5;
    d.theTime.tm_year=102;
    d.tp = std::chrono::system_clock::now();

    std::stringstream ss;
    auto ex = basic::struct_field_info_utils::StructFieldInfoBasedCsvExporterFactory<M>
        ::createExporter<test_data>(ss);
    r.exportItem(ex, std::move(d));

    d.name="bcd\"   ,cd";
    d.amount = 2;
    d.inside = inside_data {"abc", 3.4, {1}};
    d.inside2 = inside_data {"in2", 2.3, {3}};
    d.moreData[1].s = "test";
    d.moreData[1].i = 2;
    d.tp = std::chrono::system_clock::now();

    small_test_data sd;
    basic::struct_field_info_utils::StructuralCopy::copy(sd, d);
    test_data_2 d2;
    basic::struct_field_info_utils::FlatCopy::copy(d2, d);
    std::cout << d << '\n';
    std::cout << sd << '\n';
    std::cout << d2 << '\n';

    r.exportItem(ex, std::move(d));

    std::cout << ss.str() << '\n';


    /*
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
    */
    std::vector<test_data> x;
    basic::struct_field_info_utils::StructFieldInfoBasedSimpleCsvInput<test_data>
        ::readInto(ss, std::back_inserter(x));
    for (auto const &y : x) {
        std::cout << y << '\n';
    }
    std::cout << '\n';
    basic::struct_field_info_utils::StructFieldInfoBasedSimpleCsvOutput<test_data>
        ::writeDataCollection(std::cout, x.begin(), x.end());
}
