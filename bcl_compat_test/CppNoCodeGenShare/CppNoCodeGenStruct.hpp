#ifndef CPP_NO_PROTO_STRUCT_HPP_
#define CPP_NO_PROTO_STRUCT_HPP_

#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/transport/bcl_compat/BclStructs.hpp>

using namespace dev::cd606::tm;

namespace bcl_compat_test{
#ifdef _MSC_VER
    #define QUERY_FIELDS \
        ((transport::bcl_compat::BclGuid<Env>, id)) \
        ((transport::bcl_compat::BclDecimal, value)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(basic::SingleLayerWrapperWithID<6,std::string>), description)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(basic::SingleLayerWrapperWithID<5,std::vector<float>>), floatArr))

    #define RESULT_FIELDS \
        ((transport::bcl_compat::BclGuid<Env>, id)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(basic::SingleLayerWrapperWithID<3,transport::bcl_compat::BclDecimal>), value)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(basic::SingleLayerWrapperWithID<2,std::vector<std::string>>), messages))
    
    #define SMALL_QUERY_FIELDS \
        ((transport::bcl_compat::BclGuid<Env>, id)) \
        ((transport::bcl_compat::BclDecimal, value)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(basic::SingleLayerWrapperWithID<6,std::string>), description)) 

    #define SMALL_RESULT_FIELDS \
        ((transport::bcl_compat::BclGuid<Env>, id)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(basic::SingleLayerWrapperWithID<3,transport::bcl_compat::BclDecimal>), value)) 
#else
    #define QUERY_FIELDS \
        ((transport::bcl_compat::BclGuid<Env>, id)) \
        ((transport::bcl_compat::BclDecimal, value)) \
        (((basic::SingleLayerWrapperWithID<6,std::string>), description)) \
        (((basic::SingleLayerWrapperWithID<5,std::vector<float>>), floatArr))

    #define RESULT_FIELDS \
        ((transport::bcl_compat::BclGuid<Env>, id)) \
        (((basic::SingleLayerWrapperWithID<3,transport::bcl_compat::BclDecimal>), value)) \
        (((basic::SingleLayerWrapperWithID<2,std::vector<std::string>>), messages))

    #define SMALL_QUERY_FIELDS \
        ((transport::bcl_compat::BclGuid<Env>, id)) \
        ((transport::bcl_compat::BclDecimal, value)) \
        (((basic::SingleLayerWrapperWithID<6,std::string>), description)) 

    #define SMALL_RESULT_FIELDS \
        ((transport::bcl_compat::BclGuid<Env>, id)) \
        (((basic::SingleLayerWrapperWithID<3,transport::bcl_compat::BclDecimal>), value)) 
#endif

    TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT(((typename, Env)), QueryNoCodeGen, QUERY_FIELDS);
    TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT(((typename, Env)), ResultNoCodeGen, RESULT_FIELDS);
    TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT(((typename, Env)), SmallQueryNoCodeGen, SMALL_QUERY_FIELDS);
    TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT(((typename, Env)), SmallResultNoCodeGen, SMALL_RESULT_FIELDS);
}

TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT_SERIALIZE_NO_FIELD_NAMES(((typename, Env)), bcl_compat_test::QueryNoCodeGen, QUERY_FIELDS);
TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT_SERIALIZE_NO_FIELD_NAMES(((typename, Env)), bcl_compat_test::ResultNoCodeGen, RESULT_FIELDS);
TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT_SERIALIZE_NO_FIELD_NAMES(((typename, Env)), bcl_compat_test::SmallQueryNoCodeGen, SMALL_QUERY_FIELDS);
TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT_SERIALIZE_NO_FIELD_NAMES(((typename, Env)), bcl_compat_test::SmallResultNoCodeGen, SMALL_RESULT_FIELDS);


#undef QUERY_FIELDS
#undef RESULT_FIELDS 

#endif