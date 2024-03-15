#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/PrintHelper.hpp>
#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/basic/MetaInformation.hpp>
#include <tm_kit/basic/NlohmannJsonInterop.hpp>
#include <iostream>
#include <fstream>

using namespace dev::cd606::tm;

using InnerTuple = std::tuple<int, std::string, std::string>;
#define T_FIELDS \
    ((int, f1)) \
    ((std::string, f2)) \
    ((double, f3)) \
    ((InnerTuple, f4))
#define T1_FIELDS \
    ((int, f1)) \
    ((std::string, f2))

TM_BASIC_CBOR_CAPABLE_STRUCT(T, T_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT(T1, T1_FIELDS);
//TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(T, T_FIELDS);
//TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(T1, T1_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(T, T_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(T1, T1_FIELDS);

int main(int argc, char **argv) {   
    if (std::string_view(argv[1]) == "encode") {
        if (std::string_view(argv[2]) == "small") {
            T1 x {1, "abc"};
            auto s = basic::bytedata_utils::RunCBORSerializer<T1>::apply(x);
            std::ofstream ofs(argv[3]);
            ofs.write(s.data(), s.length());
            ofs.close();
        } else if (std::string_view(argv[2]) == "big") {
            T x {1, "abc", 0.5, {2, "bcd", "cde"}};
            auto s = basic::bytedata_utils::RunCBORSerializer<T>::apply(x);
            std::ofstream ofs(argv[3]);
            ofs.write(s.data(), s.length());
            ofs.close();
        }
    } else if (std::string_view(argv[1]) == "decode") {
        std::ifstream ifs(argv[3], std::ios::binary|std::ios::ate);
        auto len = (std::size_t) ifs.tellg();
        std::string s;
        s.resize(len);
        ifs.seekg(0, std::ios::beg);
        ifs.read(s.data(), len);
        ifs.close();
        if (std::string_view(argv[2]) == "small") {
            auto res = basic::bytedata_utils::RunCBORDeserializer<T1>::apply(s, 0);
            if (res) {
                basic::PrintHelper<T1>::print(std::cout, std::get<0>(*res));
                std::cout << '\n';
            } else {
                std::cout << "Failure\n";
            }
        } else if (std::string_view(argv[2]) == "big") {
            auto res = basic::bytedata_utils::RunCBORDeserializer<T>::apply(s, 0);
            if (res) {
                basic::PrintHelper<T>::print(std::cout, std::get<0>(*res));
                std::cout << '\n';
            } else {
                std::cout << "Failure\n";
            }
        }
    } else if (std::string_view(argv[1]) == "decode-inplace") {
        std::ifstream ifs(argv[3], std::ios::binary|std::ios::ate);
        auto len = (std::size_t) ifs.tellg();
        std::string s;
        s.resize(len);
        ifs.seekg(0, std::ios::beg);
        ifs.read(s.data(), len);
        ifs.close();
        if (std::string_view(argv[2]) == "small") {
            T1 y;
            auto res = basic::bytedata_utils::RunCBORDeserializer<T1>::applyInPlace(y, s, 0);
            if (res) {
                basic::PrintHelper<T1>::print(std::cout, y);
                std::cout << '\n';
            } else {
                std::cout << "Failure\n";
            }
        } else if (std::string_view(argv[2]) == "big") {
            T y;
            auto res = basic::bytedata_utils::RunCBORDeserializer<T>::applyInPlace(y, s, 0);
            if (res) {
                basic::PrintHelper<T>::print(std::cout, y);
                std::cout << '\n';
            } else {
                std::cout << "Failure\n";
            }
        }
    } else if (std::string_view(argv[1]) == "meta") {
        basic::PrintHelper<basic::MetaInformation>::print(std::cout, basic::MetaInformationGenerator<T>::generate());
        std::cout << '\n';
        basic::nlohmann_json_interop::Json<basic::MetaInformation>(basic::MetaInformationGenerator<T>::generate()).writeToStream(std::cout);
        std::cout << '\n';
    }
}