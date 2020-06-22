#ifndef COMMAND_INTERFACE_HPP_
#define COMMAND_INTERFACE_HPP_

#include "DBData.hpp"
#include <tm_kit/basic/ConstType.hpp>
#include <tm_kit/basic/VoidStruct.hpp>

namespace test {
    using TransactionKey = dev::cd606::tm::basic::VoidStruct; //we have a fixed combination that we want to update
    using TransactionDataVersion = std::array<int64_t, 3>; //index 0 is account A, 1 is account B, and 2 is transfer list
    using TransactionVersionComparer = dev::cd606::tm::infra::ArrayComparerWithSkip<int64_t, 3>;
    using TransactionDataSummary = dev::cd606::tm::basic::VoidStruct; //no need of any summary
    struct TransactionDataCheckSummary {
        bool operator()(TransactionData const &, TransactionDataSummary const &) const {
            return true;
        }
    };
    using TransferRequest = std::tuple<
        dev::cd606::tm::basic::ConstType<1001>
        , TransferData
    >;
    using ProcessRequest = dev::cd606::tm::basic::ConstType<1002>;
    using InjectRequest = std::tuple<
        dev::cd606::tm::basic::ConstType<1003>
        , InjectData
    >;
    using TransactionDataDelta = std::variant<
        TransferRequest
        , ProcessRequest
        , InjectRequest
    >;

    inline std::ostream &operator<<(std::ostream &os, TransactionDataVersion const &v) {
        os << "[" << v[0] << ',' << v[1] << ',' << v[2] << "]";
        return os;
    }
}

#endif