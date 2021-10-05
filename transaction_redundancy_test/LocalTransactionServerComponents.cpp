#include "LocalTransactionServerComponents.hpp"

void LocalDSComponent::initialize(LocalDSComponent::Callback *cb) {
    watchListener_ = cb;
    cb->onUpdate(
        DI::Update {
            0
            , std::vector<DI::OneUpdateItem> {
                { DI::OneFullUpdateItem {
                    DI::Key {}
                    , DI::Version {
                        1
                        , {{"A", 1}, {"B", 1}}
                        , 1
                    }
                    , DI::Data {
                        {250}
                        , {{"A", {125, 0}}, {"B", {125, 0}}}
                        , {}
                    }
                } }
            }
        }
    );
}
void LocalDSComponent::pushUpdate(DI::Update &&u) {
    watchListener_->onUpdate(std::move(u));
}

TI::TransactionResponse LocalTHComponent::handleUpdate(std::string const &account, TI::Key const &key, std::optional<TI::VersionSlice> const &updateVersionSlice, TI::ProcessedUpdate const &dataDelta) {
    ++counter_;
    ds_->pushUpdate(
        DI::Update {
            counter_
            , std::vector<DI::OneUpdateItem> {
                { DI::OneDeltaUpdateItem {
                    DI::Key {}
                    , (updateVersionSlice?*updateVersionSlice:(TI::VersionSlice{}))
                    , dataDelta
                } }
            }
        }
    );
    return {counter_, basic::transaction::v2::RequestDecision::Success};        
}
