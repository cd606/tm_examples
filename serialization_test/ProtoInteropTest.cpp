#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/PrintHelper.hpp>
#include <iostream>
#include <iomanip>
#include <valarray>

#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/basic/ProtoInterop.hpp>
#include <tm_kit/basic/StructFieldInfoBasedCopy.hpp>
#include <tm_kit/basic/StructFieldInfoBasedCsvUtils.hpp>

using namespace dev::cd606::tm::basic;
using namespace dev::cd606::tm::infra;

#define INNER_TEST_STRUCT_FIELDS \
    (((SingleLayerWrapperWithTypeMark<proto_interop::ZigZag,int32_t>), a)) \
    (((SingleLayerWrapperWithTypeMark<proto_interop::Fixed,int64_t>), a1)) \
    ((int32_t, a2)) \
    ((double, b)) \
    (((SingleLayerWrapperWithID<1001,std::vector<std::string>>), c)) \
    ((std::string, d)) 

#define SIMPLE_INNER_TEST_STRUCT_FIELDS \
    ((double, b)) \
    ((int32_t, a)) \
    ((int64_t, a1)) \
    ((int32_t, a2)) \
    ((std::vector<std::string>, c)) \
    ((std::string, d)) 

#define OUTER_TEST_STRUCT_FIELDS \
    ((std::valarray<float>, f)) \
    ((InnerTestStruct, g)) \
    ((bool, h))

#define SIMPLE_OUTER_TEST_STRUCT_FIELDS \
    ((std::valarray<float>, f)) \
    ((SimpleInnerTestStruct, g)) \
    ((bool, h))

TM_BASIC_CBOR_CAPABLE_STRUCT(InnerTestStruct, INNER_TEST_STRUCT_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(InnerTestStruct, INNER_TEST_STRUCT_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT(OuterTestStruct, OUTER_TEST_STRUCT_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(OuterTestStruct, OUTER_TEST_STRUCT_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT(SimpleInnerTestStruct, SIMPLE_INNER_TEST_STRUCT_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(SimpleInnerTestStruct, SIMPLE_INNER_TEST_STRUCT_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT(SimpleOuterTestStruct, SIMPLE_OUTER_TEST_STRUCT_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(SimpleOuterTestStruct, SIMPLE_OUTER_TEST_STRUCT_FIELDS);

int main(int argc, char **argv) {
    OuterTestStruct s {
        {1.0f, 2.0f, 3.0f}
        , InnerTestStruct {
            {-37}, {-50}, {-70}, 10.2525
            , {{"abcde", "bcd", "cde"}}
            , "xyz"
        }
        , false
    };
    std::cout << s << '\n';
    proto_interop::Proto<OuterTestStruct> p(std::move(s));
    std::string encoded;
    p.SerializeToString(&encoded);
    bytedata_utils::printByteDataDetails(std::cout, ByteDataView {encoded});
    std::cout << '\n';
    proto_interop::Proto<OuterTestStruct> p1;
    if (p1.ParseFromString(encoded)) {
        std::cout << p1.value() << '\n';
        SimpleOuterTestStruct aCopy;
        struct_field_info_utils::StructuralCopy::copy(aCopy, p1.value());
        std::cout << aCopy << '\n';
    } else {
        std::cout << "fail\n";
    }
}
