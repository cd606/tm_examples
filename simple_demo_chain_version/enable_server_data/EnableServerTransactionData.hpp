#ifndef ENABLE_SERVER_TRANSACTION_DATA_HPP_
#define ENABLE_SERVER_TRANSACTION_DATA_HPP_

#include <tm_kit/basic/transaction/v2/TransactionLogicCombination.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/transport/CrossGuidComponent.hpp>

namespace simple_demo_chain_version { namespace enable_server {

    using Key = dev::cd606::tm::basic::VoidStruct;
    using Data = bool;
    using DataSummary = dev::cd606::tm::basic::VoidStruct;
    using DataDelta = Data;

    struct CheckSummary {
        bool operator()(Data const &d, DataSummary const &s) const {
            return true;
        }
    };

    using DI = dev::cd606::tm::basic::transaction::v2::DataStreamInterface<
        int64_t
        , Key
        , int64_t
        , Data
        , int64_t
        , DataDelta
    >;
    using TI = dev::cd606::tm::basic::transaction::v2::TransactionInterface<
        int64_t
        , Key
        , int64_t
        , Data
        , DataSummary
        , int64_t
        , DataDelta
    >;
    using GS = dev::cd606::tm::basic::transaction::v2::GeneralSubscriberTypes<
        dev::cd606::tm::transport::CrossGuidComponent::IDType, DI
    >;

} }

#endif