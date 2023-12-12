#include <tm_kit/basic/FixedPrecisionShortDecimal.hpp>
#include <tm_kit/basic/NlohmannJsonInterop.hpp>
#include <tm_kit/basic/ProtoInterop.hpp>

using namespace dev::cd606::tm;

using D = basic::FixedPrecisionShortDecimal<6>;

#define TEST \
    ((std::string, a)) \
    ((D, d)) \
    ((bool, x))

#define TEST_STR \
    ((std::string, a)) \
    ((std::string, d)) \
    ((bool, x))

#define TEST_DBL \
    ((std::string, a)) \
    ((double, d)) \
    ((bool, x))

TM_BASIC_CBOR_CAPABLE_STRUCT(Test, TEST);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(Test, TEST);
TM_BASIC_CBOR_CAPABLE_STRUCT(TestStr, TEST_STR);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(TestStr, TEST_STR);
TM_BASIC_CBOR_CAPABLE_STRUCT(TestDbl, TEST_DBL);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(TestDbl, TEST_DBL);

int main() {
    D x {"-12345678.55001045E5"};
    std::cout << x.value() << '\n';
    std::cout << x.asString() << '\n';
    std::cout << (double) x << '\n';
    Test t {"abc", x, true};
    std::cout << t << '\n';
    TestStr ts {"abc", "-12345678.55001045E5", true};
    TestDbl td {"abc", -12345678.55001045E5, true};
    std::cout << ts << '\n';
    std::cout << td << '\n';

    auto enc = basic::bytedata_utils::RunCBORSerializer<Test>::apply(t);
    std::cout << enc.length() << '\n';
    auto enc_s = basic::bytedata_utils::RunCBORSerializer<TestStr>::apply(ts);
    std::cout << enc_s.length() << '\n';
    auto enc_d = basic::bytedata_utils::RunCBORSerializer<TestDbl>::apply(td);
    std::cout << enc_d.length() << '\n';
    Test t1;
    if (basic::bytedata_utils::RunCBORDeserializer<Test>::applyInPlace(t1, enc, 0)) {
        std::cout << "GOOD 1:" << t1 << '\n';
    } else {
        std::cout << "BAD 1\n";
    }
    if (basic::bytedata_utils::RunCBORDeserializer<Test>::applyInPlace(t1, enc_s, 0)) {
        std::cout << "GOOD 2:" << t1 << '\n';
    } else {
        std::cout << "BAD 2\n";
    }
    if (basic::bytedata_utils::RunCBORDeserializer<Test>::applyInPlace(t1, enc_d, 0)) {
        std::cout << "GOOD 3:" << t1 << '\n';
    } else {
        std::cout << "BAD 3\n";
    }

    basic::nlohmann_json_interop::Json<Test>(t).writeToString(&enc);
    std::cout << enc << '\n';
    basic::nlohmann_json_interop::Json<TestStr>(ts).writeToString(&enc_s);
    std::cout << enc_s << '\n';
    basic::nlohmann_json_interop::Json<TestDbl>(td).writeToString(&enc_d);
    std::cout << enc_d << '\n';
    if (basic::nlohmann_json_interop::Json<Test *>(&t1).fromStringView(enc)) {
        std::cout << "JSON GOOD 1:" << t1 << '\n';
    } else {
        std::cout << "JSON BAD 1\n";
    }
    if (basic::nlohmann_json_interop::Json<Test *>(&t1).fromStringView(enc_s)) {
        std::cout << "JSON GOOD 2:" << t1 << '\n';
    } else {
        std::cout << "JSON BAD 2\n";
    }
    if (basic::nlohmann_json_interop::Json<Test *>(&t1).fromStringView(enc_d)) {
        std::cout << "JSON GOOD 3:" << t1 << '\n';
    } else {
        std::cout << "JSON BAD 3\n";
    }

    basic::proto_interop::Proto<Test>(t).SerializeToString(&enc);
    std::cout << enc.size() << '\n';
    basic::proto_interop::Proto<TestStr>(ts).SerializeToString(&enc_s);
    std::cout << enc_s.size() << '\n';
    basic::proto_interop::Proto<TestDbl>(td).SerializeToString(&enc_d);
    std::cout << enc_d.size() << '\n';
    if (basic::proto_interop::Proto<Test *>(&t1).ParseFromStringView(enc)) {
        std::cout << "PROTO GOOD 1: " << t1 << '\n';
    } else {
        std::cout << "PROTO BAD 1\n";
    }
    if (basic::proto_interop::Proto<Test *>(&t1).ParseFromStringView(enc_s)) {
        std::cout << "PROTO GOOD 2: " << t1 << '\n';
    } else {
        std::cout << "PROTO BAD 2\n";
    }
    if (basic::proto_interop::Proto<Test *>(&t1).ParseFromStringView(enc_d)) {
        std::cout << "PROTO GOOD 3: " << t1 << '\n';
    } else {
        std::cout << "PROTO BAD 3\n";
    }
}