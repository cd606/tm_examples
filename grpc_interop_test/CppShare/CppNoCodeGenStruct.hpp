#ifndef CPP_NO_CODE_GEN_STRUCT_HPP_
#define CPP_NO_CODE_GEN_STRUCT_HPP_

#include <tm_kit/basic/SerializationHelperMacros.hpp>

namespace grpc_interop_test {
    #define TEST_REQUEST_FIELDS \
        ((uint32_t, intParam)) \
        ((std::vector<double>, doubleListParam)) 

    #define TEST_RESPONSE_FIELDS \
        ((std::vector<std::string>, stringResp))

    #define SIMPLE_REQUEST_FIELDS \
        ((uint32_t, input)) 

    #define SIMPLE_RESPONSE_FIELDS \
        ((uint32_t, resp))

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