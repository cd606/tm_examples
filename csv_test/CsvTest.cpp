#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/basic/StructFieldInfoBasedCsvUtils.hpp>

#define TestDataFields \
    ((std::string, name)) \
    ((int32_t, amount)) \
    ((double, stat)) \
    (((std::array<float, 5>), moreData)) \
    ((std::tm, theTime))

TM_BASIC_CBOR_CAPABLE_STRUCT(test_data, TestDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(test_data, TestDataFields);

using namespace dev::cd606::tm;

int main() {
    using O = basic::struct_field_info_utils::StructFieldInfoBasedSimpleCsvOutput<test_data>;
    std::stringstream ss;
    O::writeHeader(ss);
    test_data d;
    basic::struct_field_info_utils::StructFieldInfoBasedInitializer<test_data>::initialize(d);
    d.name = "abc\"\\test";
    d.amount = 1;
    d.stat = 2.3;
    d.moreData[2] = 0.5f;
    d.theTime.tm_year=102;
    O::writeData(ss, d);
    std::cout << ss.str() << "\n";
    using I = basic::struct_field_info_utils::StructFieldInfoBasedSimpleCsvInput<test_data>;
    auto x = I::readHeader(ss);
    std::cout << x.size() << '\n';
    for (auto const &y : x) {
        std::cout << y.first << ": " << y.second << '\n';
    }
    test_data d1;
    std::cout << I::readOneWithHeaderDict(ss, d1, x) << '\n';
    std::cout << d1 << '\n';
    return 0;
}