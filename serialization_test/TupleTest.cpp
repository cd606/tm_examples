#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/PrintHelper.hpp>
#include <iostream>
#include <fstream>

using namespace dev::cd606::tm;

int main(int argc, char **argv) {
    using T = std::tuple<int, std::string, double, std::tuple<int, std::string, std::string>>;
    using T1 = std::tuple<int, std::string>;
    
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
    } else if (std::string_view(argv[1]) == "encode-names") {
        if (std::string_view(argv[2]) == "small") {
            T1 x {1, "abc"};
            auto s = basic::bytedata_utils::RunCBORSerializerWithNameList<T1, 2>::apply(x, {"f1", "f2"});
            std::ofstream ofs(argv[3]);
            ofs.write(s.data(), s.length());
            ofs.close();
        } else if (std::string_view(argv[2]) == "big") {
            T x {1, "abc", 0.5, {2, "bcd", "cde"}};
            auto s = basic::bytedata_utils::RunCBORSerializerWithNameList<T, 4>::apply(x, {"f1", "f2", "f3", "f4"});
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
    } else if (std::string_view(argv[1]) == "decode-names") {
        std::ifstream ifs(argv[3], std::ios::binary|std::ios::ate);
        auto len = (std::size_t) ifs.tellg();
        std::string s;
        s.resize(len);
        ifs.seekg(0, std::ios::beg);
        ifs.read(s.data(), len);
        ifs.close();
        if (std::string_view(argv[2]) == "small") {
            auto res = basic::bytedata_utils::RunCBORDeserializerWithNameList<T1, 2>::apply(s, 0, {"f1", "f2"});
            if (res) {
                basic::PrintHelper<T1>::print(std::cout, std::get<0>(*res));
                std::cout << '\n';
            } else {
                std::cout << "Failure\n";
            }
        } else if (std::string_view(argv[2]) == "big") {
            auto res = basic::bytedata_utils::RunCBORDeserializerWithNameList<T, 4>::apply(s, 0, {"f1", "f2", "f3", "f4"});
            if (res) {
                basic::PrintHelper<T>::print(std::cout, std::get<0>(*res));
                std::cout << '\n';
            } else {
                std::cout << "Failure\n";
            }
        }
    } else if (std::string_view(argv[1]) == "decode-inplace-names") {
        std::ifstream ifs(argv[3], std::ios::binary|std::ios::ate);
        auto len = (std::size_t) ifs.tellg();
        std::string s;
        s.resize(len);
        ifs.seekg(0, std::ios::beg);
        ifs.read(s.data(), len);
        ifs.close();
        if (std::string_view(argv[2]) == "small") {
            T1 y;
            auto res = basic::bytedata_utils::RunCBORDeserializerWithNameList<T1, 2>::applyInPlace(y, s, 0, {"f1", "f2"});
            if (res) {
                basic::PrintHelper<T1>::print(std::cout, y);
                std::cout << '\n';
            } else {
                std::cout << "Failure\n";
            }
        } else if (std::string_view(argv[2]) == "big") {
            T y;
            auto res = basic::bytedata_utils::RunCBORDeserializerWithNameList<T, 4>::applyInPlace(y, s, 0, {"f1", "f2", "f3", "f4"});
            if (res) {
                basic::PrintHelper<T>::print(std::cout, y);
                std::cout << '\n';
            } else {
                std::cout << "Failure\n";
            }
        }
    }
}