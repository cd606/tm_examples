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

#define REQ2_FIELDS \
    ((int, iParam)) \
    ((std::string, sParam))

#define RESP2_FIELDS \
    ((std::string, retVal))

TM_BASIC_CBOR_CAPABLE_STRUCT(Req, REQ_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(Req, REQ_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT(Resp, RESP_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(Resp, RESP_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT(Req2, REQ2_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(Req2, REQ2_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT(Resp2, RESP2_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(Resp2, RESP2_FIELDS);

#undef REQ_FIELDS
#undef RESP_FIELDS

#endif