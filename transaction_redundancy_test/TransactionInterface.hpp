#ifndef TRANSACTION_INTERFACE_HPP_
#define TRANSACTION_INTERFACE_HPP_

#include "DBData.hpp"
#include <tm_kit/infra/VersionedData.hpp>
#include <tm_kit/basic/ConstType.hpp>
#include <tm_kit/basic/VoidStruct.hpp>

#include <tm_kit/basic/transaction/TransactionServer.hpp>
#include <tm_kit/transport/BoostUUIDComponent.hpp>

namespace test {
    using GlobalVersion = int64_t;
    using Key = dev::cd606::tm::basic::VoidStruct; //we have a fixed combination that we want to update
    using Version = Shaped<int64_t, int64_t, int64_t>;
    using Data = Shaped<OverallStat, AccountData, TransferList>;
    using VersionSlice = ShapedOptionals<int64_t, int64_t, int64_t>;
    using DataSlice = ShapedOptionals<OverallStat, AccountData, TransferList>;
    using DataSummary = DataSlice;
    using Command = std::variant<
        TransferData
        , ProcessPendingTransfers
        , InjectData
        , CloseAccount
    >;

    struct CompareVersion {
        bool operator()(Version const &a, Version const &b) const;
    };
    struct CheckVersion {
        bool operator()(Version const &a, Version const &b) const;
    };
    struct CheckVersionSlice {
        bool operator()(Version const &a, VersionSlice const &b) const;
    };
    struct CheckSummary {
        bool operator()(Data const &a, DataSummary const &b) const;
    };

    class ValidateCommandImpl;
    class ValidateCommand {
    private:
        std::unique_ptr<ValidateCommandImpl> impl_;
    public:
        ValidateCommand();
        ValidateCommand(std::function<void(std::string const &)> errorLogger);
        ValidateCommand(ValidateCommand &&);
        ValidateCommand &operator=(ValidateCommand &&);
        ~ValidateCommand();
        bool operator()(Data const &data, Command const &command) const;
    };

    class ProcessCommandOnLocalDataImpl;
    class ProcessCommandOnLocalData {
    private:
        std::unique_ptr<ProcessCommandOnLocalDataImpl> impl_;
    public:
        ProcessCommandOnLocalData();
        ProcessCommandOnLocalData(std::function<void(std::string const &)> errorLogger);
        ProcessCommandOnLocalData(ProcessCommandOnLocalData &&);
        ProcessCommandOnLocalData &operator=(ProcessCommandOnLocalData &&);
        ~ProcessCommandOnLocalData();
        std::optional<DataSlice> operator()(Data const &data, Command const &command) const;
    };

    template <class A, class B, class C>
    struct ApplyShapedSlice {
        void operator()(Shaped<A,B,C> &a, ShapedOptionals<A,B,C> const &b) const {
            if (b.overallStat) {
                a.overallStat = *(b.overallStat);
            }
            for (auto const &item : b.accounts) {
                auto iter = a.accounts.find(item.first);
                if (iter == a.accounts.end()) {
                    if (item.second) {
                        a.accounts.insert({item.first, *(item.second)});
                    }
                } else {
                    if (item.second) {
                        iter->second = *(item.second);
                    } else {
                        a.accounts.erase(iter);
                    }
                }
            }
            if (b.pendingTransfers) {
                a.pendingTransfers = *(b.pendingTransfers);
            }
        }
    };

    using ApplyVersionSlice = ApplyShapedSlice<int64_t,int64_t,int64_t>;
    using ApplyDataSlice = ApplyShapedSlice<OverallStat, AccountData, TransferList>;

    std::ostream &operator<<(std::ostream &os, Version const &v);
    std::ostream &operator<<(std::ostream &os, Data const &d);
}

using namespace dev::cd606::tm;
using namespace test;

using DI = basic::transaction::current::DataStreamInterface<
    int64_t
    , Key
    , Version
    , Data
    , VersionSlice
    , DataSlice
    , std::less<int64_t>
    , CompareVersion
>;

using TI = basic::transaction::current::TransactionInterface<
    int64_t
    , Key
    , Version
    , Data
    , DataSummary
    , VersionSlice
    , Command
    , DataSlice
>;

using GS = basic::transaction::current::GeneralSubscriberTypes<
    boost::uuids::uuid, DI
>;

#endif