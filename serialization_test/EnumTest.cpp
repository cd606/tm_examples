#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/basic/NlohmannJsonInterop.hpp>
#include <iostream>
#include <fstream>

using namespace dev::cd606::tm;

#define TEST_VALUES \
    ((a, "First", 10)) ((b, "Second", 20)) ((c, "Third", 30))

TM_BASIC_CBOR_CAPABLE_ENUM_AS_STRING_WITH_ALTERNATES_AND_VALUES(Test, TEST_VALUES);
TM_BASIC_CBOR_CAPABLE_ENUM_AS_STRING_WITH_ALTERNATES_AND_VALUES_SERIALIZE(Test, TEST_VALUES);

#define S_FIELDS \
    ((Test, x)) \
    ((std::string, y))

TM_BASIC_CBOR_CAPABLE_STRUCT(S, S_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(S, S_FIELDS);

int main(int argc, char **argv) {
    S s {
        .x = Test::b 
        , .y = "abc"
    };
    std::string output;
    basic::nlohmann_json_interop::Json<S>(s).writeToString(&output);
    std::cout << output << '\n';
    std::string input = "{\"x\":\"Third\",\"y\":\"xyz\"}";
    basic::nlohmann_json_interop::Json<S> j;
    j.fromString(input);
    std::cout << j.value() << '\n';
    input = "{\"x\":10,\"y\":\"xyz\"}";
    j.fromString(input);
    std::cout << j.value() << '\n';
    return 0;
}