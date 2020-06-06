#include "dbData.pb.h"

struct DBDataEq {
    bool operator()(db_subscription::db_data const &a, db_subscription::db_data const &b) const {
        return (a.value1() == b.value1() && a.value2() == b.value2());
    }
};