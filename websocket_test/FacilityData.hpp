#ifndef DATA_HPP_
#define DATA_HPP_

#include <tm_kit/basic/SerializationHelperMacros.hpp>

#define QUERY_FIELDS \
    ((std::chrono::system_clock::time_point, now)) \
    ((std::string, textData)) \
    ((uint64_t, intData)) \
    ((std::vector<double>, doubleData)) 

#define RESPONSE_FIELDS \
    ((std::tm, inputTime)) \
    ((std::string, responseText)) \
    ((std::vector<double>, responseDoubleData))

TM_BASIC_CBOR_CAPABLE_STRUCT(Query, QUERY_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(Query, QUERY_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT(Response, RESPONSE_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(Response, RESPONSE_FIELDS);

#undef QUERY_FIELDS
#undef RESPONSE_FIELDS

#endif