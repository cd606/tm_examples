#include "TransactionInterface.hpp"

using namespace dev::cd606::tm;

namespace test {
    bool CompareVersion::operator()(Version const &a, Version const &b) const {
        //the reason we don't do ANY real version comparison here is that
        //we assume that GlobalVersion takes care of data consistency
        //, and we can simply treat any inner data as a new version
        return true;
    }
    bool CheckVersion::operator()(Version const &a, Version const &b) const {
        if (a.overallStat != 0 && b.overallStat != 0) {
            if (a.overallStat != b.overallStat) {
                return false;
            }
        }

        for (auto const &item : b.accounts) {
            auto aIter = a.accounts.find(item.first);
            if (aIter == a.accounts.end() && item.second != 0) {
                return false;
            }
            if (aIter->second == 0 || item.second == 0) {
                continue;
            }
            if (aIter->second != item.second) {
                return false;
            }
        }

        if (a.pendingTransfers != 0 && b.pendingTransfers != 0) {
            if (a.pendingTransfers != b.pendingTransfers) {
                return false;
            }
        }

        return true;
    }
    bool CheckVersionSlice::operator()(Version const &a, VersionSlice const &b) const {
        if (b.overallStat && *(b.overallStat) != 0 && a.overallStat != 0) {
            if (a.overallStat != *(b.overallStat)) {
                return false;
            }
        }

        for (auto const &item : b.accounts) {
            auto aIter = a.accounts.find(item.first);
            if (aIter == a.accounts.end() && item.second && *(item.second) != 0) {
                return false;
            }
            if (aIter->second == 0 || !(item.second) || *(item.second) == 0) {
                continue;
            }
            if (aIter->second != *(item.second)) {
                return false;
            }
        }

        if (b.pendingTransfers && *(b.pendingTransfers) != 0 && a.pendingTransfers != 0) {
            if (a.pendingTransfers != *(b.pendingTransfers)) {
                return false;
            }
        }

        return true;
    }

    bool CheckSummary::operator()(Data const &d, DataSummary const &summary) const {
        if (summary.overallStat && !(d.overallStat == *(summary.overallStat))) {
            return false;
        }
        for (auto const &item : summary.accounts) {
            if (item.second) {
                auto iter = d.accounts.find(item.first);
                if (iter == d.accounts.end()) {
                    return false;
                }
                if (!(iter->second == *(item.second))) {
                    return false;
                }
            }
        }
        if (summary.pendingTransfers && d.pendingTransfers.items != summary.pendingTransfers->items) {
            return false;
        }
        return true;
    }

    class ValidateCommandImpl {
    public:
        std::function<void(std::string const &)> errorLogger_;
        static constexpr size_t PENDING_TRANSFER_SIZE_LIMIT = 10;
        ValidateCommandImpl() : errorLogger_([](std::string const &) {}) {}
        ValidateCommandImpl(std::function<void(std::string const &)> errorLogger) : errorLogger_(errorLogger) {}
        bool check(Data const &data, Command const &cmd) const {
            return std::visit([this,&data](auto const &update) -> bool {
                using T = std::decay_t<decltype(update)>;
                if constexpr (std::is_same_v<T, TransferData>) {
                    if (data.pendingTransfers.items.size() >= PENDING_TRANSFER_SIZE_LIMIT) {
                        std::ostringstream oss;
                        oss << "Too many pending transfers";
                        errorLogger_(oss.str());
                        return false;
                    }
                    auto iter = data.accounts.find(update.from);
                    if (iter == data.accounts.end()) {
                        std::ostringstream oss;
                        oss << "No such account '" << update.from << "'";
                        errorLogger_(oss.str());
                        return false;
                    }
                    int32_t pendingNow = iter->second.pending_amount;
                    int64_t willBePending = static_cast<int64_t>(pendingNow)-static_cast<int64_t>(update.amount);
                    if (willBePending < std::numeric_limits<int32_t>::min()) {
                        std::ostringstream oss;
                        oss << "Will pending " << willBePending << " for '" << update.from << "' too low";
                        errorLogger_(oss.str());
                        return false;
                    }
                    if (static_cast<int32_t>(iter->second.amount)+static_cast<int32_t>(willBePending) < 0) {
                        std::ostringstream oss;
                        oss << "Will pending " << willBePending << " makes available amount in '" << update.from << "' less than zero";
                        errorLogger_(oss.str());
                        return false;
                    }
                    iter = data.accounts.find(update.to);
                    if (iter == data.accounts.end()) {
                        std::ostringstream oss;
                        oss << "No such account '" << update.to << "'";
                        errorLogger_(oss.str());
                        return false;
                    }
                    pendingNow = iter->second.pending_amount;
                    willBePending = static_cast<int64_t>(pendingNow)+static_cast<int64_t>(update.amount);
                    if (willBePending > std::numeric_limits<int32_t>::max()) {
                        std::ostringstream oss;
                        oss << "Will pending " << willBePending << "for '" << update.to << "' too hight";
                        errorLogger_(oss.str());
                        return false;
                    }
                    return true;
                } else if constexpr (std::is_same_v<T, ProcessPendingTransfers>) {
                    return true;
                } else if constexpr (std::is_same_v<T, InjectData>) {
                    return true;
                } else if constexpr (std::is_same_v<T, CloseAccount>) {
                    return true;
                } else {
                    return false;
                }
            }, cmd);
        }
    };

    ValidateCommand::ValidateCommand() : impl_(std::make_unique<ValidateCommandImpl>()) {}
    ValidateCommand::ValidateCommand(std::function<void(std::string const &)> errorLogger) :
        impl_(std::make_unique<ValidateCommandImpl>(errorLogger)) {
    }
    ValidateCommand::ValidateCommand(ValidateCommand &&) = default;
    ValidateCommand &ValidateCommand::operator=(ValidateCommand &&) = default;
    ValidateCommand::~ValidateCommand() {}
    bool ValidateCommand::operator()(Data const &data, Command const &cmd) const {
        return impl_->check(data, cmd);
    }

    class ProcessCommandOnLocalDataImpl {
    private:
        ValidateCommand validate_;
    public:
        ProcessCommandOnLocalDataImpl() : validate_() {}
        ProcessCommandOnLocalDataImpl(std::function<void(std::string const &)> errorLogger)
            : validate_(errorLogger) {}
        std::optional<DataSlice> process(Data const &data, Command const &cmd) const {
            if (!validate_(data, cmd)) {
                return std::nullopt;
            }
            return std::visit([&data](auto const &update) -> DataSlice {
                using T = std::decay_t<decltype(update)>;
                if constexpr (std::is_same_v<T, TransferData>) {
                    DataSlice outData;
                    auto iterFrom = data.accounts.find(update.from);
                    if (iterFrom == data.accounts.end()) {
                        return outData;
                    }
                    auto iterTo = data.accounts.find(update.to);
                    if (iterTo == data.accounts.end()) {
                        return outData;
                    }

                    auto outIterFrom = outData.accounts.insert(std::make_pair(
                        iterFrom->first, iterFrom->second
                    )).first;
                    outIterFrom->second->pending_amount -= update.amount;
                    auto outIterTo = outData.accounts.insert(std::make_pair(
                        iterTo->first, iterTo->second
                    )).first;
                    outIterTo->second->pending_amount += update.amount;
                    outData.pendingTransfers = data.pendingTransfers;
                    outData.pendingTransfers->items.push_back(update);

                    return outData;
                } else if constexpr (std::is_same_v<T, ProcessPendingTransfers>) {
                    DataSlice outData;

                    std::unordered_set<std::string> affected;
                    for (auto const &item : data.pendingTransfers.items) {
                        affected.insert(item.from);
                        affected.insert(item.to);
                    }
                    
                    for (auto const &x : affected) {
                        auto iter = data.accounts.find(x);
                        if (iter != data.accounts.end()) {
                            AccountData d = iter->second;
                            if (d.pending_amount < 0) {
                                if (d.amount < static_cast<uint32_t>(-d.pending_amount)) {
                                    d.amount = 0;
                                    d.pending_amount = 0;
                                } else {
                                    d.amount -= static_cast<uint32_t>(-d.pending_amount);
                                    d.pending_amount = 0;
                                }
                            } else {
                                uint64_t n = d.amount+static_cast<uint32_t>(d.pending_amount);
                                if (n > std::numeric_limits<uint32_t>::max()) {
                                    n = std::numeric_limits<uint32_t>::max();
                                }
                                d.amount = n;
                                d.pending_amount = 0;
                            }
                            outData.accounts.insert(std::make_pair(x, d));
                        }
                    }

                    outData.pendingTransfers = TransferList {};
                    return outData;
                } else if constexpr (std::is_same_v<T, InjectData>) {
                    DataSlice outData;

                    outData.overallStat = data.overallStat;
                
                    auto iter = data.accounts.find(update.to);
                    if (iter == data.accounts.end()) {
                        outData.accounts.insert(std::make_pair(
                            update.to
                            , AccountData {update.amount, 0}
                        ));
                        outData.overallStat->totalSum += update.amount;
                    } else {
                        AccountData d = iter->second;
                        uint64_t n = d.amount+update.amount;
                        if (n > std::numeric_limits<uint32_t>::max()) {
                            n = std::numeric_limits<uint32_t>::max();
                        }
                        outData.overallStat->totalSum += n-d.amount;
                        d.amount = n;
                        outData.accounts.insert(std::make_pair(
                            update.to
                            , d
                        ));
                    }
                    return outData;
                } else if constexpr (std::is_same_v<T, CloseAccount>) {
                    DataSlice outData;

                    auto iter = data.accounts.find(update.which);
                    if (iter == data.accounts.end()) {
                        return outData;
                    }

                    outData.overallStat = data.overallStat;
                    
                    outData.accounts.insert(std::make_pair(
                        update.which
                        , std::nullopt
                    ));

                    int32_t oldAmount = static_cast<int32_t>(iter->second.amount)+iter->second.pending_amount;
                    if (oldAmount > 0 && outData.overallStat->totalSum < static_cast<uint64_t>(oldAmount)) {
                        outData.overallStat->totalSum = 0;
                    } else {
                        outData.overallStat->totalSum += -oldAmount;
                    }
                    return outData;
                } else {
                    return DataSlice {};
                }
            }, cmd);
        }
    };

    ProcessCommandOnLocalData::ProcessCommandOnLocalData() : impl_(std::make_unique<ProcessCommandOnLocalDataImpl>()) {}
    ProcessCommandOnLocalData::ProcessCommandOnLocalData(std::function<void(std::string const &)> errorLogger) : impl_(std::make_unique<ProcessCommandOnLocalDataImpl>(errorLogger)) {}
    ProcessCommandOnLocalData::ProcessCommandOnLocalData(ProcessCommandOnLocalData &&) = default;
    ProcessCommandOnLocalData &ProcessCommandOnLocalData::operator=(ProcessCommandOnLocalData &&) = default;
    ProcessCommandOnLocalData::~ProcessCommandOnLocalData() {}
    std::optional<DataSlice> ProcessCommandOnLocalData::operator()(Data const &data, Command const &cmd) const {
        return impl_->process(data, cmd);
    }

    std::ostream &operator<<(std::ostream &os, Version const &v) {
        os << "{overallStat=" << v.overallStat;
        os << ",accounts={";
        int ii = 0;
        for (auto const &item : v.accounts) {
            if (ii > 0) {
                os << ',';
            }
            os << item.first << '=' << item.second;
            ++ii;
        }
        os << "},pendingTransfers=" << v.pendingTransfers;
        os << '}';
        return os;
    }
    std::ostream &operator<<(std::ostream &os, Data const &v) {
        os << "{overallStat=" << v.overallStat;
        os << ",accounts={";
        int ii = 0;
        for (auto const &item : v.accounts) {
            if (ii > 0) {
                os << ',';
            }
            os << item.first << '=' << item.second;
            ++ii;
        }
        os << "},pendingTransfers={items=[";
        ii = 0;
        for (auto const &item : v.pendingTransfers.items) {
            if (ii > 0) {
                os << ',';
            }
            os << item;
            ++ii;
        }
        os << "]}}";
        return os;
    }

}
