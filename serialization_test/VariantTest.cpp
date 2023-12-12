#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/PrintHelper.hpp>
#include <iostream>
#include <fstream>

using namespace dev::cd606::tm;

int main(int argc, char **argv) {
    using T = std::variant<int, std::string, double, std::tuple<int, std::string, std::string>>;
    using Arr = std::array<T,4>;
    Arr x;
    x[0] = (int) 1;
    x[1] = (std::string) "abc";
    x[2] = 0.5;
    x[3] = std::tuple<int, std::string, std::string> {2, "bcd", "cde"};
    
    if (std::string_view(argv[1]) == "encode") {
        auto s = basic::bytedata_utils::RunCBORSerializer<Arr>::apply(x);
        std::ofstream ofs(argv[2]);
        ofs.write(s.data(), s.length());
        ofs.close();
    } else if (std::string_view(argv[1]) == "decode") {
        std::ifstream ifs(argv[2], std::ios::binary|std::ios::ate);
        auto len = (std::size_t) ifs.tellg();
        std::string s;
        s.resize(len);
        ifs.seekg(0, std::ios::beg);
        ifs.read(s.data(), len);
        ifs.close();
        std::cout << len << '\n';
        Arr a;
        auto res = basic::bytedata_utils::RunCBORDeserializer<Arr>::applyInPlace(a, s, 0);
        if (res) {
            for (auto const &y : a) {
                basic::PrintHelper<T>::print(std::cout, y);
                std::cout << '\n';
            }
        } else {
            std::cout << "Failure\n";
        }
    }
}