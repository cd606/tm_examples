
#ifndef DATA_HPP_
#define DATA_HPP_

#include <tm_kit/basic/SerializationHelperMacros.hpp>

#ifdef _MSC_VER
#define DATA_FIELDS \
    ((std::chrono::system_clock::time_point, now)) \
    ((std::string, textData)) \
    ((uint64_t, intData)) \
    ((std::vector<double>, doubleData)) \
    ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::variant<int,float>), variantData)) \
    ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::tuple<bool,int>), tupleData))
#else
#define DATA_FIELDS \
    ((std::chrono::system_clock::time_point, now)) \
    ((std::string, textData)) \
    ((uint64_t, intData)) \
    ((std::vector<double>, doubleData)) \
    (((std::variant<int,float>), variantData)) \
    (((std::tuple<bool,int>), tupleData))
#endif

TM_BASIC_CBOR_CAPABLE_STRUCT(Data, DATA_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(Data, DATA_FIELDS);

#undef DATA_FIELDS

#endif