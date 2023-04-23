#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/basic/StructFieldInfoBasedHdf5Utils.hpp>

#define TestDataFields \
    ((double, x)) \
    ((uint32_t, y))

TM_BASIC_CBOR_CAPABLE_STRUCT(test_data, TestDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(test_data, TestDataFields);

using namespace dev::cd606::tm;

int main() {
    std::vector<test_data> d;

    H5Eset_auto2(H5E_DEFAULT, NULL, NULL);

    for (int ii=0; ii<10; ++ii) {
        d.push_back({ii*1.0, (uint32_t) ii});
    }
    basic::struct_field_info_utils::StructFieldInfoBasedHdf5Utils<test_data>
        ::append(d, "test.h5", "test1/data1");
    for (int ii=0; ii<10; ++ii) {
        d.push_back({ii*2.0, (uint32_t) ii*2});
    }
    basic::struct_field_info_utils::StructFieldInfoBasedHdf5Utils<test_data>
        ::append(d, "test.h5", "test1/data1");

    basic::struct_field_info_utils::StructFieldInfoBasedHdf5Utils<test_data>
        ::append({100.0, 200}, "test.h5", "test1/data1");

    std::vector<test_data> d1;
    basic::struct_field_info_utils::StructFieldInfoBasedHdf5Utils<test_data>
        ::read(d1, "test.h5", "test1/data1");
    for (auto const &x : d1) {
        std::cout << x << '\n';
    }
}
