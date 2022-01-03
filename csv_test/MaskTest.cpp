#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/basic/StructFieldInfoWithMaskingFilter.hpp>
#include <tm_kit/basic/StructFieldInfoBasedCsvUtils.hpp>
#include <tm_kit/basic/ConcatStructFieldInfo.hpp>
#include <tm_kit/basic/StructFieldInfoBasedDynamicView.hpp>

#include <iostream>
#include <fstream>

using namespace dev::cd606::tm;

#define DATA_FIELDS \
    ((std::string, Name)) \
    ((int, Count)) \
    ((std::string, Description)) \
    ((bool, Check))

#define DATA2_FIELDS \
    ((double, Data)) \
    ((double, AnotherData))

TM_BASIC_CBOR_CAPABLE_STRUCT(Data, DATA_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(Data, DATA_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT(Data2, DATA2_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(Data2, DATA2_FIELDS);

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

    std::cout << "=======================\n";
    using C = basic::struct_field_info_utils::ConcatStructFields<Data,Data2>;
    std::vector<C> cs;
    C c;
    c.Data::operator=(dataVec[0]);
    c.Data2::operator=(Data2 {1.2, 3.4});
    cs.push_back(std::move(c));
    c.Data::operator=(dataVec[1]);
    c.Data2::operator=(Data2 {5.6, 7.8});
    cs.push_back(std::move(c));

    basic::struct_field_info_utils::StructFieldInfoBasedSimpleCsvOutput<
        C
    >::writeDataCollection(
        std::cout
        , cs.begin()
        , cs.end()
    );

    std::cout << "=======================\n";
    std::cout << std::any_cast<double>(
        *(basic::struct_field_info_utils::DynamicConstView<C> {cs[0]})["Data"]
    ) << '\n';
    basic::struct_field_info_utils::DynamicView<C> dv {cs[0]};
    dv["Data"] = 1.23;
    std::cout << std::any_cast<double>(
        *dv["Data"]
    ) << '\n';
    dv[4] = 1.234;
    std::cout << std::any_cast<double>(
        *dv[4]
    ) << '\n';
    dv.set<double>("Data", 1.2345);
    std::cout << *(dv.get<double>(4)) << '\n';

    std::vector<std::tuple<std::string_view, std::any>> fields;
    (basic::struct_field_info_utils::DynamicConstView<C> {cs[0]}).copyNamesAndValuesTo(std::back_inserter(fields));
    std::cout << fields.size() << '\n';
    std::cout << std::get<0>(fields[5]) << ' ' << std::any_cast<double>(std::get<1>(fields[5])) << '\n';
}