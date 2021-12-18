#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/basic/StructFieldInfoWithMaskingFilter.hpp>
#include <tm_kit/basic/StructFieldInfoBasedCsvUtils.hpp>

#include <iostream>
#include <fstream>

using namespace dev::cd606::tm;

#define DATA_FIELDS \
    ((std::string, Name)) \
    ((int, Count)) \
    ((std::string, Description)) \
    ((bool, Check))

TM_BASIC_CBOR_CAPABLE_STRUCT(Data, DATA_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(Data, DATA_FIELDS);

inline constexpr bool mask(std::string_view const &s) {
    return (s[0] == 'C');
}
inline constexpr bool mask2(std::string_view const &s) {
    return (s != "Description");
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: maskTest FILE_NAME\n";
        return 1;
    }
    std::ifstream ifs(argv[1]);
    std::vector<Data> dataVec;
    basic::struct_field_info_utils::StructFieldInfoBasedSimpleCsvInput<Data>
        ::readInto(
            ifs 
            , std::back_inserter(dataVec)
            , basic::struct_field_info_utils::StructFieldInfoBasedCsvInputOption::UseHeaderAsDict
        );
    std::cout << dataVec.size() << '\n';
    for (auto const &item : dataVec) {
        std::cout << item << '\n';
    }

    std::cout << "=======================\n";
    basic::struct_field_info_masking::MaskedStructView<Data,mask> {dataVec[0]} 
        = basic::struct_field_info_masking::MaskedStructConstView<Data,mask2> {dataVec[1]};
    
    std::vector<basic::struct_field_info_masking::MaskedStruct<Data,mask2>> x;
    for (auto const &item : dataVec) {
        x.push_back({item});
    }

    basic::struct_field_info_utils::StructFieldInfoBasedSimpleCsvOutput<
        basic::struct_field_info_masking::MaskedStruct<Data,mask2>
    >::writeDataCollection(
        std::cout
        , x.begin()
        , x.end()
    );
}