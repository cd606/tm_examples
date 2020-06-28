#include "dbData.pb.h"

namespace db_subscription {
    inline bool operator==(db_data const &a, db_data const &b) {
        return (a.value1() == b.value1() && a.value2() == b.value2());
    }
}