#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/SynchronousRunner.hpp>
#include <tm_kit/infra/Environments.hpp>

#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/basic/StructFieldInfoBasedCsvUtils.hpp>
#include <tm_kit/basic/StructFieldInfoBasedFlatPackUtils.hpp>
#include <tm_kit/basic/StructFieldFlattenedInfo.hpp>
#include <tm_kit/basic/StructFieldInfoBasedCopy.hpp>
#include <tm_kit/basic/NlohmannJsonInterop.hpp>
#include <tm_kit/basic/top_down_single_pass_iteration_clock/ClockComponent.hpp>

#define InsideDataFields \
    ((std::string, s)) \
    ((double, stat)) \
    ((dev::cd606::tm::basic::ConstType<1>, emptyField)) \
    ((std::optional<int16_t>, i)) 

#define FlatPackInsideDataFields \
    ((double, stat)) \
    ((dev::cd606::tm::basic::ConstType<1>, emptyField)) \
    ((int16_t, i)) 

#ifdef _MSC_VER
    #define TestDataFields \
        ((std::string, name)) \
        ((int32_t, amount)) \
        ((std::optional<inside_data>, inside)) \
        ((inside_data, inside2)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::array<inside_data, 5>), moreData)) \
        ((std::tm, theTime)) \
        ((std::chrono::system_clock::time_point, tp))
    #define FlatPackTestDataFields \
        ((int32_t, amount)) \
        ((flatpack_inside_data, inside)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::array<flatpack_inside_data, 5>), moreData)) 
#else
    #define TestDataFields \
        ((std::string, name)) \
        ((int32_t, amount)) \
        ((std::optional<inside_data>, inside)) \
        ((inside_data, inside2)) \
        (((std::array<inside_data, 5>), moreData)) \
        ((std::tm, theTime)) \
        ((std::chrono::system_clock::time_point, tp))
    #define FlatPackTestDataFields \
        ((int32_t, amount)) \
        ((flatpack_inside_data, inside)) \
        (((std::array<flatpack_inside_data, 5>), moreData)) 
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
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::array<char,10>), inside2_s)) \
        ((std::optional<double>, inside2_stat)) \
        ((dev::cd606::tm::basic::ConstType<1>, inside2_emptyField)) \
        ((int16_t, inside2_i)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::array<inside_data, 5>), moreData)) \
        ((std::chrono::system_clock::time_point, theTime)) \
        ((std::tm, tp))
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
        (((std::array<char,10>), inside2_s)) \
        ((std::optional<double>, inside2_stat)) \
        ((dev::cd606::tm::basic::ConstType<1>, inside2_emptyField)) \
        ((int16_t, inside2_i)) \
        (((std::array<inside_data, 5>), moreData)) \
        ((std::chrono::system_clock::time_point, theTime)) \
        ((std::tm, tp))
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
TM_BASIC_CBOR_CAPABLE_STRUCT(flatpack_inside_data, FlatPackInsideDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(flatpack_inside_data, FlatPackInsideDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT(flatpack_test_data, FlatPackTestDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(flatpack_test_data, FlatPackTestDataFields);

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

    basic::struct_field_info_utils::FlatPack<flatpack_test_data> fd;
    basic::struct_field_info_utils::StructFieldInfoBasedInitializer<flatpack_test_data>::initialize(*fd);
    fd->amount = 1;
    fd->inside.i = -10;
    fd->moreData[2].stat = 0.5;
    std::cout << *fd << '\n';

    std::string flatStr;
    fd.writeToString(&flatStr);

    basic::bytedata_utils::printByteDataDetails(std::cout, basic::ByteDataView {flatStr});
    std::cout << '\n';

    basic::struct_field_info_utils::FlatPack<flatpack_test_data> fd1;
    if (fd1.fromString(flatStr)) {
        std::cout << *fd1 << '\n';
    }
   
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
    d.inside = inside_data {"abc", 3.4, {}, {1}};
    d.inside2 = inside_data {"in2", 2.3, {}, {3}};
    d.moreData[1].s = "test";
    d.moreData[1].i = 2;
    d.tp = std::chrono::system_clock::now();

    std::stringstream jsonSS;
    basic::nlohmann_json_interop::Json<test_data *>(&d).writeToStream(jsonSS);
    std::string jsonStr = jsonSS.str();
    std::cout << jsonStr << '\n';
    test_data dj;
    basic::nlohmann_json_interop::Json<test_data *>(&dj).fromString(jsonStr);
    std::cout << dj << '\n';

    small_test_data sd;
    basic::struct_field_info_utils::StructuralCopy::copy(sd, d);
    test_data_2 d2;
    basic::struct_field_info_utils::FlatCopy::copy(d2, d);
    std::cout << d << '\n';
    std::cout << sd << '\n';
    std::cout << d2 << '\n';

    r.exportItem(ex, std::move(d));

    std::cout << ss.str() << '\n';

    std::stringstream ss2;
    auto ex2 = basic::struct_field_info_utils::StructFieldInfoBasedCsvExporterFactory<M>
        ::createExporter<test_data_2>(ss2);
    r.exportItem(ex2, std::move(d2));
    std::cout << ss2.str() << '\n';
    std::vector<test_data_2> x2;
    basic::struct_field_info_utils::StructFieldInfoBasedSimpleCsvInput<test_data_2>
        ::readInto(ss2, std::back_inserter(x2));
    for (auto const &y2 : x2) {
        std::cout << y2 << '\n';
    }
    std::cout << '\n';

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
    for (auto const &y : x) {
        std::cout << y << '\n';
    }
    std::cout << '\n';
    basic::struct_field_info_utils::StructFieldInfoBasedSimpleCsvOutput<test_data>
        ::writeDataCollection(std::cout, x.begin(), x.end());
    */
}
