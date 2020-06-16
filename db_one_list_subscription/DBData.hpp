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

    inline bool operator==(db_key const &k1, db_key const &k2) {
        return k1.name == k2.name;
    }
    inline bool operator==(db_data const &d1, db_data const &d2) {
        return d1.amount == d2.amount && std::fabs(d1.stat-d2.stat) < 1.0E-10;
    }
}

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(db_one_list_subscription::db_key, DBKeyFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(db_one_list_subscription::db_data, DBDataFields);

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

//The reason we use "_DEF" here is that with std::vector, we cannot 
//simply feed it to ostream, so we don't generate the print part
namespace db_one_list_subscription {
    TM_BASIC_CBOR_CAPABLE_STRUCT_DEF(db_delta_delete, DBDeltaDeleteFields);
    TM_BASIC_CBOR_CAPABLE_STRUCT_DEF(db_delta_insert_update, DBDeltaInsertUpdateFields);
    TM_BASIC_CBOR_CAPABLE_STRUCT_DEF(db_delta, DBDeltaFields);
}

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(db_one_list_subscription::db_delta_delete, DBDeltaDeleteFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(db_one_list_subscription::db_delta_insert_update, DBDeltaInsertUpdateFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(db_one_list_subscription::db_delta, DBDeltaFields);

#endif