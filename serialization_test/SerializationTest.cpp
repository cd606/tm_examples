#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/PrintHelper.hpp>
#include <iostream>
#include <iomanip>

#include <tm_kit/basic/SerializationHelperMacros.hpp>

using namespace dev::cd606::tm::basic;
using namespace dev::cd606::tm::infra;

/*
TM_BASIC_CBOR_CAPABLE_ENUM_AS_STRING(TestEnum, (Item1) (Item2) (Item3) (Item4) (Item5));
TM_BASIC_CBOR_CAPABLE_ENUM_AS_STRING_SERIALIZE(TestEnum, (Item1) (Item2) (Item3) (Item4) (Item5));
*/
TM_BASIC_CBOR_CAPABLE_ENUM_AS_STRING_WITH_ALTERNATES(TestEnum, ((Item1, "first item")) ((Item2, "second item")) ((Item3, "third item")) ((Item4, "fourth item")) ((Item5, "fifth item")));
TM_BASIC_CBOR_CAPABLE_ENUM_AS_STRING_WITH_ALTERNATES_SERIALIZE(TestEnum, ((Item1, "first item")) ((Item2, "second item")) ((Item3, "third item")) ((Item4, "fourth item")) ((Item5, "fifth item")));

int main(int argc, char **argv) {
    //std::cout << bytedata_utils::DirectlySerializableChecker<TriviallySerializable<std::array<double, 10>>>::IsDirectlySerializable() << '\n';
    using TestType = 
        std::tuple<
            TestEnum //int32_t
            , double
            , std::string
            , std::unique_ptr<ByteDataWithTopic>
            , VoidStruct 
            , std::variant<
                std::vector<uint16_t>
                , std::optional<SingleLayerWrapper<
                    std::array<char,5>
                >>
            >
            , GroupedVersionedData<std::string, int64_t, double>
            , char
            , std::map<std::string, int32_t>
            , std::unordered_map<int32_t, double>
            , ConstType<5>
            , std::list<int16_t>
            , std::tuple<std::tuple<bool>>
            , std::tuple<>
            , std::tuple<float, double>
            , TriviallySerializable<std::array<float,5>>
        >
    ;
    char buf[10] = {0x1, 0x2, 0x3, 0x4, 0x5, (char) 0xff, (char) 0xfe, (char) 0xfd, (char) 0xfc, (char) 0xfb};
    TestType t {
        TestEnum::Item3 //-5
        , 2.3E7
        , "this is a test"
        , std::make_unique<ByteDataWithTopic>(
            ByteDataWithTopic {
                "test.topic"
                , std::string {buf, buf+10}
            }
        )
        , VoidStruct {}
        /*
        , std::vector<uint16_t> {
            10000, 11000, 12000
        }*/
        , std::optional<SingleLayerWrapper<
                    std::array<char,5>
                >> {
            SingleLayerWrapper<
                std::array<char,5>
            > {
                {'a','b','c','d','e'}
            }
        }
        , GroupedVersionedData<std::string, int64_t, double> {
            "group1", 20, 1111.11
        }
        , 'x'
        , std::map<std::string, int32_t> {
            {"a", 5}
            , {"b", 6}
        }
        , std::unordered_map<int32_t, double> {
            {10, 123.456}
            , {20, 234.567}
        }
        , ConstType<5> {}
        , std::list<int16_t> {321, 654, 987}
        , std::tuple<std::tuple<bool>> {{true}}
        , std::tuple<> {}
        , std::tuple<float, double> {1.2f, 3.4}
        , TriviallySerializable<std::array<float,5>> {
            std::array<float, 5> {
                1.0f, 2.0f, 3.0f, 4.0f, 5.0f
            }
        }
    };
    char buf1[1024];
    auto encodedV = bytedata_utils::RunCBORSerializerWithNameList<TestType, 16>::apply(
        t
        , {"f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15", "f16"}
        , buf1
    );
    auto encoded = std::string_view(buf1, encodedV);
    /*
    auto encoded = bytedata_utils::RunCBORSerializerWithNameList<TestType, 15>::apply(
        t
        , {"f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15"}
    );*/
    //auto encoded = bytedata_utils::RunSerializer<CBORWithMaxSizeHint<TestType>>::apply({std::move(t), 1024});
    //auto encoded = bytedata_utils::RunSerializer<TestType>::apply(t);
    bytedata_utils::printByteDataDetails(std::cout, ByteDataView {encoded});
    std::cout << "\n";
    auto decoded = bytedata_utils::RunCBORDeserializerWithNameList<TestType, 16>::apply(
        encoded, 0
        , {"f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15", "f16"}
    );
    //auto decoded = bytedata_utils::RunDeserializer<CBORWithMaxSizeHint<TestType>>::apply(encoded);
    //auto decoded = bytedata_utils::RunDeserializer<TestType>::apply(encoded);
    if (decoded) {
        std::cout << "Decode success\n";
        TestType const &data = std::get<0>(*decoded);
        //TestType const &data = decoded->value;
        //TestType const &data = *decoded;
        PrintHelper<TestType>::print(std::cout, data);
	    std::cout << '\n';
        std::cout << "TestType {\n";
        std::cout << "\t" << std::get<0>(data) << "\n"
            << "\t, " << std::get<1>(data) << "\n"
            << "\t, '" << std::get<2>(data) << "'\n";
        std::cout << "\t, {'" << std::get<3>(data)->topic << "',";
        bytedata_utils::printByteDataDetails(std::cout, ByteData {std::get<3>(data)->content});
        std::cout << "}";
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
        std::cout << "\t, {";
        for (auto const &item : std::get<8>(data)) {
            std::cout << "{'" << item.first << "'," << item.second << "} ";
        }
        std::cout << "}\n";
        std::cout << "\t, {";
        for (auto const &item : std::get<9>(data)) {
            std::cout << "{'" << item.first << "'," << item.second << "} ";
        }
        std::cout << "}\n";
        std::cout << "\t, {}\n";
        std::cout << "\t, [";
        for (auto const &item : std::get<11>(data)) {
            std::cout << item << ' ';
        }
        std::cout << "]\n";
        std::cout << "\t, {{" << std::get<0>(std::get<0>(std::get<12>(data))) << "}}\n";
        std::cout << "\t, {}\n";
        std::cout << "\t, {" << std::get<0>(std::get<14>(data)) << "," << std::get<1>(std::get<14>(data)) << "}\n";
        std::cout << "\t, {[";
        for (auto const &item : std::get<15>(data).value) {
            std::cout << item << ' ';
        }
        std::cout << "]}\n";
        std::cout << "}\n";
    } else {
        std::cout << "Decode failure\n";
    }
}
