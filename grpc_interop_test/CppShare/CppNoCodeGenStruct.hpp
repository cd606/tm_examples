#ifndef CPP_NO_CODE_GEN_STRUCT_HPP_
#define CPP_NO_CODE_GEN_STRUCT_HPP_

#include <tm_kit/basic/SerializationHelperMacros.hpp>

namespace grpc_interop_test {
    #define TEST_REQUEST_FIELDS \
        ((uint32_t, intParam)) \
        ((std::vector<double>, doubleListParam)) 

    #define TEST_RESPONSE_FIELDS \
        ((std::vector<std::string>, stringResp))

    using ReqOneOf = std::variant<
        std::monostate
        , std::string
        , float
    >;
    #define SIMPLE_REQUEST_FIELDS \
        ((uint32_t, input)) \
        ((grpc_interop_test::ReqOneOf, reqOneOf)) \
        ((std::string, name2)) \
        ((std::vector<uint32_t>, anotherInput))

    using RespOneOf = std::variant<
        std::monostate
        , dev::cd606::tm::basic::SingleLayerWrapperWithID<5, std::string>
        , dev::cd606::tm::basic::SingleLayerWrapperWithID<3, float>
    >;
#ifdef _MSC_VER
    #define SIMPLE_RESPONSE_FIELDS \
        ((uint32_t, resp)) \
        ((grpc_interop_test::RespOneOf, respOneOf)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(dev::cd606::tm::basic::SingleLayerWrapperWithID<2,std::string>), name2Resp)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(dev::cd606::tm::basic::SingleLayerWrapperWithID<6,std::vector<uint32_t>>), anotherInputBack))
#else
    #define SIMPLE_RESPONSE_FIELDS \
        ((uint32_t, resp)) \
        ((grpc_interop_test::RespOneOf, respOneOf)) \
        (((dev::cd606::tm::basic::SingleLayerWrapperWithID<2,std::string>), name2Resp)) \
        (((dev::cd606::tm::basic::SingleLayerWrapperWithID<6,std::vector<uint32_t>>), anotherInputBack))
#endif

    TM_BASIC_CBOR_CAPABLE_STRUCT(TestRequest, TEST_REQUEST_FIELDS);
    TM_BASIC_CBOR_CAPABLE_STRUCT(TestResponse, TEST_RESPONSE_FIELDS);
    TM_BASIC_CBOR_CAPABLE_STRUCT(SimpleRequest, SIMPLE_REQUEST_FIELDS);
    TM_BASIC_CBOR_CAPABLE_STRUCT(SimpleResponse, SIMPLE_RESPONSE_FIELDS);
}

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(grpc_interop_test::TestRequest, TEST_REQUEST_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(grpc_interop_test::TestResponse, TEST_RESPONSE_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(grpc_interop_test::SimpleRequest, SIMPLE_REQUEST_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(grpc_interop_test::SimpleResponse, SIMPLE_RESPONSE_FIELDS);

#undef TEST_REQUEST_FIELDS
#undef TEST_RESPONSE_FIELDS
#undef SIMPLE_REQUEST_FIELDS
#undef SIMPLE_RESPONSE_FIELDS 

#endif