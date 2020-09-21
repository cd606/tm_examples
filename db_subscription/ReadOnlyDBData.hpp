#ifndef READ_ONLY_DB_DATA_HPP_
#define READ_ONLY_DB_DATA_HPP_

#include <tm_kit/basic/SerializationHelperMacros.hpp>

#define DBDataFields \
    ((int32_t, value1)) \
    ((std::string, value2))

TM_BASIC_CBOR_CAPABLE_STRUCT(DBData, DBDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(DBData, DBDataFields);

#define DBQueryFields \
    ((std::string, name)) 

TM_BASIC_CBOR_CAPABLE_STRUCT(DBQuery, DBQueryFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(DBQuery, DBQueryFields);

using DBQueryResult = dev::cd606::tm::basic::CBOR<std::optional<DBData>>;

#endif