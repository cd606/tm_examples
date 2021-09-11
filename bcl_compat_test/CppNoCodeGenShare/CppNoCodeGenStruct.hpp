#ifndef CPP_NO_PROTO_STRUCT_HPP_
#define CPP_NO_PROTO_STRUCT_HPP_

#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/transport/bcl_compat/BclStructs.hpp>

using namespace dev::cd606::tm;

namespace bcl_compat_test{
    #define QUERY_FIELDS \
        ((transport::bcl_compat::BclGuid<Env>, id)) \
        ((transport::bcl_compat::BclDecimal, value)) \
        ((std::string, description)) \
        ((std::vector<float>, floatArr))

    #define RESULT_FIELDS \
        ((transport::bcl_compat::BclGuid<Env>, id)) \
        ((transport::bcl_compat::BclDecimal, value)) \
        ((std::vector<std::string>, messages))

    TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT(((typename, Env)), QueryNoCodeGen, QUERY_FIELDS);
    TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT(((typename, Env)), ResultNoCodeGen, RESULT_FIELDS);
}

TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT_SERIALIZE_NO_FIELD_NAMES(((typename, Env)), bcl_compat_test::QueryNoCodeGen, QUERY_FIELDS);
TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT_SERIALIZE_NO_FIELD_NAMES(((typename, Env)), bcl_compat_test::ResultNoCodeGen, RESULT_FIELDS);

#undef QUERY_FIELDS
#undef RESULT_FIELDS 

#endif