#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/infra/WithTimeData.hpp>
#include <iostream>
#include <iomanip>

using namespace dev::cd606::tm::basic;
using namespace dev::cd606::tm::infra;

void printBytes(ByteData const &b) {
    std::cout << "[";
    for (size_t ii=0; ii<b.content.length(); ++ii) {
        if (ii > 0) {
            std::cout << ", ";
        }
        std::cout << "0x" << std::hex << std::setw(2) 
            << std::setfill('0') << static_cast<uint16_t>(static_cast<uint8_t>(b.content[ii]))
            << std::dec;
    }
    std::cout << "] (" << b.content.length() << " bytes)";
}

int main(int argc, char **argv) {
    using TestType = 
        std::tuple<
            int32_t
            , double
            , std::string
            , std::unique_ptr<ByteData>
            , ConstType<5>
            , std::variant<
                std::vector<uint16_t>
                , std::optional<SingleLayerWrapper<
                    std::array<float,5>
                >>
            >
            , GroupedVersionedData<std::string, int64_t, double>
            , bool
        >
    ;
    char buf[5] = {0x1, 0x2, 0x3, 0x4, 0x5};
    TestType t {
        -5
        , 2.3E7
        , "this is a test"
        , std::make_unique<ByteData>(
            ByteData {
                std::string {buf, buf+5}
            }
        )
        , ConstType<5> {}
        /*
        , std::vector<uint16_t> {
            10000, 11000, 12000
        }*/
        , std::optional<SingleLayerWrapper<
                    std::array<float,5>
                >> {
            SingleLayerWrapper<
                std::array<float,5>
            > {
                {1.2f, 2.3f, 3.4f, 4.5f, 5.6f}
            }
        }
        , GroupedVersionedData<std::string, int64_t, double> {
            "group1", 20, 1111.11
        }
        , true
    };
    auto encoded = bytedata_utils::RunSerializer<CBOR<TestType>>::apply({std::move(t)});
    //auto encoded = bytedata_utils::RunSerializer<TestType>::apply(t);
    printBytes(ByteData {encoded});
    std::cout << "\n";
    auto decoded = bytedata_utils::RunDeserializer<CBOR<TestType>>::apply(encoded);
    //auto decoded = bytedata_utils::RunDeserializer<TestType>::apply(encoded);
    if (decoded) {
        std::cout << "Decode success\n";
        TestType const &data = decoded->value;
        //TestType const &data = *decoded;
        std::cout << "TestType {\n";
        std::cout << "\t" << std::get<0>(data) << "\n"
            << "\t, " << std::get<1>(data) << "\n"
            << "\t, '" << std::get<2>(data) << "'\n";
        std::cout << "\t, ";
        printBytes(*(std::get<3>(data)));
        std::cout << "\n";
        std::cout << "\t, {}\n";
        std::cout << "\t, ";
        switch (std::get<5>(data).index()) {
        case 0:
            {
                std::cout << "[";
                auto v = std::get<0>(std::get<5>(data));
                for (auto item : v) {
                    std::cout << item << ' ';
                }
                std::cout << "]";
            }
            break;
        case 1:
            {
                auto v = std::get<1>(std::get<5>(data));
                if (v) {
                    std::cout << "[";
                    for (auto item : v->value) {
                        std::cout << item << ' ';
                    }
                    std::cout << "]";
                } else {
                    std::cout << "std::nullopt";
                }
            }
            
        }
        std::cout << "\n";
        std::cout << "\t, {" 
            << std::get<6>(data).groupID 
            << "," << std::get<6>(data).version 
            << "," << std::get<6>(data).data << "}\n";
        std::cout << "\t, " << (std::get<7>(data)?"true":"false") << "\n";
        std::cout << "}\n";
    } else {
        std::cout << "Decode failure\n";
    }
}