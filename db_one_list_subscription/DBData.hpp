#ifndef DB_DATA_HPP_
#define DB_DATA_HPP_

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/SerializationHelperMacros.hpp>

#define DBDataFields \
    ((int32_t, amount)) \
    ((double, stat))

namespace db_one_list_subscription {
    TM_BASIC_CBOR_CAPABLE_STRUCT(db_data, DBDataFields);
}

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(db_one_list_subscription::db_data, DBDataFields);

#endif
