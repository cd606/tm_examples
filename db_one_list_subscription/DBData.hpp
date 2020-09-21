#ifndef DB_DATA_HPP_
#define DB_DATA_HPP_

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/SerializationHelperMacros.hpp>
#include <iostream>
#include <cmath>

#define DBKeyFields \
    ((std::string, name))

#define DBDataFields \
    ((int32_t, amount)) \
    ((double, stat))

namespace db_one_list_subscription {
    TM_BASIC_CBOR_CAPABLE_STRUCT(db_key, DBKeyFields);
    TM_BASIC_CBOR_CAPABLE_STRUCT(db_data, DBDataFields);
}

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(db_one_list_subscription::db_key, DBKeyFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(db_one_list_subscription::db_data, DBDataFields);

#define DBItemFields \
    ((db_one_list_subscription::db_key, key)) \
    ((db_one_list_subscription::db_data, data))

namespace db_one_list_subscription {
    TM_BASIC_CBOR_CAPABLE_STRUCT(db_item, DBItemFields);
}

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(db_one_list_subscription::db_item, DBItemFields);

#define DBDeltaDeleteFields \
    ((std::vector<db_one_list_subscription::db_key>, keys)) 

#define DBDeltaInsertUpdateFields \
    ((std::vector<db_one_list_subscription::db_item>, items)) 

#define DBDeltaFields \
    ((db_one_list_subscription::db_delta_delete, deletes)) \
    ((db_one_list_subscription::db_delta_insert_update, inserts_updates)) 

namespace db_one_list_subscription {
    TM_BASIC_CBOR_CAPABLE_STRUCT(db_delta_delete, DBDeltaDeleteFields);
    TM_BASIC_CBOR_CAPABLE_STRUCT(db_delta_insert_update, DBDeltaInsertUpdateFields);
    TM_BASIC_CBOR_CAPABLE_STRUCT(db_delta, DBDeltaFields);
}

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(db_one_list_subscription::db_delta_delete, DBDeltaDeleteFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(db_one_list_subscription::db_delta_insert_update, DBDeltaInsertUpdateFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(db_one_list_subscription::db_delta, DBDeltaFields);

#endif
