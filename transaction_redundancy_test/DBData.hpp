#ifndef DB_DATA_HPP_
#define DB_DATA_HPP_

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <unordered_map>
#include <map>

/**
 * The setup:
 *
 * There are many accounts, with name, amount and pending amount.
 *
 * Each pending transfer is marked with from, to and amount, and put
 * in a global pending transfer list.
 *
 * There is an overall statistics record, for now, it contains the 
 * sum of all amounts in all accounts.
 *
 * The system will try to maintain the data integrity as much as possible
 * , but it will not check outside changes to make sure the data is still
 * consistent. For example, if the overall statistics record is modified from
 * outside, then the system does not try to make sure it still reflects the
 * sum of all amounts in all accounts.
 *
 * This file simply defines some data structures. The integrity check logic
 * will be in TransactionInterface.hpp/cpp.
 */

//First, the individual data parts

#define AccountDataFields \
    ((uint32_t, amount)) \
    ((int32_t, pending_amount))

#define TransferDataFields \
    ((std::string, from)) \
    ((std::string, to)) \
    ((uint32_t, amount))

#define TransferListFields \
    ((std::vector<test::TransferData>, items))

#define OverallStatFields \
    ((uint64_t, totalSum))

namespace test {
    TM_BASIC_CBOR_CAPABLE_STRUCT(AccountData, AccountDataFields);
    TM_BASIC_CBOR_CAPABLE_STRUCT(TransferData, TransferDataFields);
    TM_BASIC_CBOR_CAPABLE_STRUCT_DEF(TransferList, TransferListFields);
    TM_BASIC_CBOR_CAPABLE_STRUCT_EQ(TransferList, TransferListFields);
    TM_BASIC_CBOR_CAPABLE_STRUCT(OverallStat, OverallStatFields);
}

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(test::AccountData, AccountDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(test::TransferData, TransferDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(test::TransferList, TransferListFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(test::OverallStat, OverallStatFields);

//Now, the "shape" of the complete data

#define ShapedTemplateParams \
    ((typename, StatPartType)) \
    ((typename, AccountPartType)) \
    ((typename, PendingPartType))

#ifdef _MSC_VER 
    #define ShapedFields \
        ((StatPartType, overallStat)) \
        ((TM_BASIC_CBOR_CAPABLE_STRUCT_PROTECT_TYPE(std::map<std::string, AccountPartType), accounts)) \
        ((PendingPartType, pendingTransfers))
#else
    #define ShapedFields \
        ((StatPartType, overallStat)) \
        (((std::map<std::string, AccountPartType>), accounts)) \
        ((PendingPartType, pendingTransfers))
#endif

namespace test {
    TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT(ShapedTemplateParams, Shaped, ShapedFields);

    template <class StatPartType, class AccountPartType, class PendingPartType>
    using ShapedOptionals = Shaped<
        std::optional<StatPartType>
        , std::optional<AccountPartType>
        , std::optional<PendingPartType>
    >;
}

TM_BASIC_CBOR_CAPABLE_TEMPLATE_STRUCT_SERIALIZE(ShapedTemplateParams, test::Shaped, ShapedFields);

//These are the two other commands (than transfer data) that the system supports.

#define InjectDataFields \
    ((std::string, to)) \
    ((uint32_t, amount))

#define CloseAccountFields \
    ((std::string, which)) 

namespace test {
    //ProcessPendingTransfers has no field so we only need to pick an empty
    //type that is different from other types used in this program. So we 
    //pick ConstType<1>
    using ProcessPendingTransfers = dev::cd606::tm::basic::ConstType<1>; 

    TM_BASIC_CBOR_CAPABLE_STRUCT(InjectData, InjectDataFields);
    TM_BASIC_CBOR_CAPABLE_STRUCT(CloseAccount, CloseAccountFields);
}

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(test::InjectData, InjectDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(test::CloseAccount, CloseAccountFields);

#endif