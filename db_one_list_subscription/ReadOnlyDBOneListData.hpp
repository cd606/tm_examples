#ifndef READ_ONLY_DB_ONE_LIST_DATA_HPP_
#define READ_ONLY_DB_ONE_LIST_DATA_HPP_

#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/basic/transaction/complex_key_value_store/VersionlessDataModel.hpp>

#define DBKeyFields \
    ((std::string, name))

#define DBDataFields \
    ((int32_t, amount)) \
    ((double, stat))

TM_BASIC_CBOR_CAPABLE_STRUCT(DBKey, DBKeyFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(DBKey, DBKeyFields);
TM_BASIC_CBOR_CAPABLE_STRUCT(DBData, DBDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(DBData, DBDataFields);

using DBQuery = dev::cd606::tm::basic::VoidStruct;
using DBQueryResult = dev::cd606::tm::basic::transaction::complex_key_value_store::FullDataResult<DBKey,DBData>;

#endif