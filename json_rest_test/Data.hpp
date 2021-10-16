#ifndef DATA_HPP_
#define DATA_HPP_

#include <tm_kit/basic/SerializationHelperMacros.hpp>

#ifdef _MSC_VER
#define REQ_FIELDS \
    ((std::vector<std::string>, x)) \
    ((double, y)) \
    ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::variant<int,float>), tChoice))
#define RESP_FIELDS \
    ((uint32_t, xCount)) \
    ((double, yTimesTwo)) \
    ((std::list<std::string>, xCopy)) \
    ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::tuple<int,float>), t))
#else
#define REQ_FIELDS \
    ((std::vector<std::string>, x)) \
    ((double, y)) \
    (((std::variant<int,float>), tChoice))
#define RESP_FIELDS \
    ((uint32_t, xCount)) \
    ((double, yTimesTwo)) \
    ((std::list<std::string>, xCopy)) \
    (((std::tuple<int,float>), t))
#endif

TM_BASIC_CBOR_CAPABLE_STRUCT(Req, REQ_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(Req, REQ_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT(Resp, RESP_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(Resp, RESP_FIELDS);

#undef REQ_FIELDS
#undef RESP_FIELDS

#endif