#include "DBData.hpp"

namespace db_one_list_subscription {
    using Key = dev::cd606::tm::basic::VoidStruct; //since there is only one key, it can simply be VoidStruct
    struct DBKeyCmp {
        bool operator()(db_key const &a, db_key const &b) const {
            return a.name < b.name;
        }
    };
    using Data = std::map<db_key, db_data, DBKeyCmp>;
    using DataSummary = size_t; //summary is just the current count of items
    using DataDelta = db_delta;

    struct CheckSummary {
        bool operator()(Data const &d, DataSummary const &s) const {
            return (d.size() == s);
        }
    };
    struct ApplyDelta {
        DataDelta operator()(Data &data, DataDelta const &delta) const {
            for (auto const &key : delta.deletes.keys) {
                data.erase(key);
            }
            for (auto const &item : delta.inserts_updates.items) {
                data[item.key] = item.data;
            }
            return delta;
        }
    };
}