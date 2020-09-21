#ifndef READ_ONLY_DB_ONE_LIST_DATA_HPP_
#define READ_ONLY_DB_ONE_LIST_DATA_HPP_

#include <tm_kit/basic/SerializationHelperMacros.hpp>

#define DBDataFields \
    ((std::string, name)) \
    ((int32_t, amount)) \
    ((double, stat))

TM_BASIC_CBOR_CAPABLE_STRUCT(DBData, DBDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(DBData, DBDataFields);

TM_BASIC_CBOR_CAPABLE_EMPTY_STRUCT(DBQuery);
TM_BASIC_CBOR_CAPABLE_EMPTY_STRUCT_SERIALIZE(DBQuery);

using DBQueryResult = dev::cd606::tm::basic::CBOR<std::vector<DBData>>;

#endif