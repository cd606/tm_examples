#ifndef READ_ONLY_DB_DATA_HPP_
#define READ_ONLY_DB_DATA_HPP_

#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <tm_kit/basic/transaction/complex_key_value_store/VersionlessDataModel.hpp>

#define DBDataFields \
    ((int32_t, value1)) \
    ((std::string, value2))

TM_BASIC_CBOR_CAPABLE_STRUCT(DBData, DBDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(DBData, DBDataFields);

#define DBKeyFields \
    ((std::string, name)) 

TM_BASIC_CBOR_CAPABLE_STRUCT(DBKey, DBKeyFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(DBKey, DBKeyFields);

using DBQuery = DBKey;
using DBQueryResult = dev::cd606::tm::basic::transaction::complex_key_value_store::KeyBasedQueryResult<DBData>;

#endif