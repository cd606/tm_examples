#ifndef CHAIN_DATA_HPP_
#define CHAIN_DATA_HPP_

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/SerializationHelperMacros.hpp>

namespace simple_demo_chain_version {

    enum class RequestCompletedFashion : uint8_t {
        Fulfilled = 0
        , TimeoutBeforeAcceptance = 1
        , AcceptedThenTimeoutBeforeResponse = 2
        , PartiallyHandledThenTimeout = 3
    };

    //If we define this enum with macros, then we can get the print for
    //free, however, the macros will cause the string to be stored in 
    //serialization, and that might not be what we want (if we put it
    //in a struct with non-trivially-copyable other fields, then the 
    //whole struct must be serialized instead of directly mapped onto
    //memory block for chain storage, and if the enum is serialized as
    //string, there will be waste of storage)

    inline std::ostream &operator<<(std::ostream &os, RequestCompletedFashion x) {
        switch (x) {
        case RequestCompletedFashion::Fulfilled:
            os << "Fulfilled(0)";
            break;
        case RequestCompletedFashion::TimeoutBeforeAcceptance:
            os << "TimeoutBeforeAcceptance(1)";
            break;
        case RequestCompletedFashion::AcceptedThenTimeoutBeforeResponse:
            os << "AcceptedThenTimeoutBeforeResponse(2)";
            break;
        case RequestCompletedFashion::PartiallyHandledThenTimeout:
            os << "PartiallyHandledThenTimeout(3)";
            break;
        default:
            os << "Unknown(" << static_cast<int>(x) << ")";
            break;
        }
        return os;
    } 

    #define PlaceRequestFields \
        ((int, id)) \
        ((double, value))
    #define ConfirmRequestReceiptFields \
        ((std::vector<int>, ids))
    #define RespondToRequestFields \
        ((int, id)) \
        ((double, response)) \
        ((bool, isFinalResponse))
    #define RequestCompletedFields \
        ((int, id)) \
        ((simple_demo_chain_version::RequestCompletedFashion, fashion))

    TM_BASIC_CBOR_CAPABLE_STRUCT(PlaceRequest, PlaceRequestFields);
    TM_BASIC_CBOR_CAPABLE_STRUCT(ConfirmRequestReceipt, ConfirmRequestReceiptFields);
    TM_BASIC_CBOR_CAPABLE_STRUCT(RespondToRequest, RespondToRequestFields);
    TM_BASIC_CBOR_CAPABLE_STRUCT(RequestCompleted, RequestCompletedFields);

    using UpdateContent = std::variant<
        PlaceRequest
        , ConfirmRequestReceipt
        , RespondToRequest
        , RequestCompleted
    >;

    #define ChainDataFields \
        ((int64_t, timestamp)) \
        ((simple_demo_chain_version::UpdateContent, update))

    TM_BASIC_CBOR_CAPABLE_STRUCT(ChainData, ChainDataFields);
}

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(simple_demo_chain_version::PlaceRequest, PlaceRequestFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(simple_demo_chain_version::ConfirmRequestReceipt, ConfirmRequestReceiptFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(simple_demo_chain_version::RespondToRequest, RespondToRequestFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(simple_demo_chain_version::RequestCompleted, RequestCompletedFields);

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(simple_demo_chain_version::ChainData, ChainDataFields);

#endif