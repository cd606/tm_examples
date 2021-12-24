#include <tm_kit/transport/bcl_compat/BclStructs.hpp>
#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/basic/NlohmannJsonInterop.hpp>

#include <iostream>

using namespace dev::cd606::tm;

#define DATA_FIELDS \
    ((transport::bcl_compat::BclDecimal, x)) \
    ((std::string, y)) \
    ((float, z))

#define DATA2_FIELDS \
    ((transport::bcl_compat::BclDecimal, x)) \
    ((std::string, y)) \
    ((transport::bcl_compat::BclDecimal, z))

TM_BASIC_CBOR_CAPABLE_STRUCT(Data, DATA_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(Data, DATA_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT(Data2, DATA2_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(Data2, DATA2_FIELDS);

int main() {
    Data d;
    d.x = transport::bcl_compat::BclDecimal {"1.023"};
    d.y = "abc";
    d.z = 1.2;
    std::cout << d << '\n';

    basic::nlohmann_json_interop::Json<Data *> j(&d);
    std::string s;
    j.writeToString(&s);

    std::cout << s << '\n';

    basic::nlohmann_json_interop::Json<Data2> d2;
    d2.fromString(s);
    std::cout << *d2 << '\n';
}