#ifndef DB_DATA_HPP_
#define DB_DATA_HPP_

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <iostream>
#include <cmath>

#define AccountDataFields \
    ((uint32_t, amount))

//positive is A to B, negative is B to A
#define TransferDataFields \
    ((int32_t, amount))

#define TransferListFields \
    ((std::vector<test::TransferData>, items))

#define TransactionDataFields \
    ((test::AccountData, accountA)) \
    ((test::AccountData, accountB)) \
    ((test::TransferList, pendingTransfers))

#define InjectDataFields \
    ((uint32_t, accountAIncrement)) \
    ((uint32_t, accountBIncrement)) 

namespace test {
    TM_BASIC_CBOR_CAPABLE_STRUCT(AccountData, AccountDataFields);
    TM_BASIC_CBOR_CAPABLE_STRUCT(TransferData, TransferDataFields);
    TM_BASIC_CBOR_CAPABLE_STRUCT_DEF(TransferList, TransferListFields);
    TM_BASIC_CBOR_CAPABLE_STRUCT_DEF(TransactionData, TransactionDataFields);
    TM_BASIC_CBOR_CAPABLE_STRUCT(InjectData, InjectDataFields);
}

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(test::AccountData, AccountDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(test::TransferData, TransferDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(test::TransferList, TransferListFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(test::TransactionData, TransactionDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(test::InjectData, InjectDataFields);

#endif