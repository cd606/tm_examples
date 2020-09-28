#include "GuiClientDataFlow.hpp"

void guiClientDataFlow(
    R &r
    , std::string const &clientName
    , R::Sourceoid<TI::Transaction> transactionCommandSource
    , R::Sourceoid<GuiExitEvent> exitEventSource
    , R::Sinkoid<DI::FullUpdate> updateSink
    , R::Sinkoid<UnsubscribeConfirmed> unsubscribeSink
) {
    //We start with setting up client identities
    r.environment()->transport::ClientSideSimpleIdentityAttacherComponent<std::string,GS::Input>::operator=(
        transport::ClientSideSimpleIdentityAttacherComponent<std::string,GS::Input>(
            clientName
        )
    ); 
    r.environment()->transport::ClientSideSimpleIdentityAttacherComponent<std::string,TI::Transaction>::operator=(
        transport::ClientSideSimpleIdentityAttacherComponent<std::string,TI::Transaction>(
            clientName
        )
    );
    
    //Next, redirect log file
    r.environment()->setLogFilePrefix(clientName);

    //Now set up data subscription and unsubscription
    auto gsFacility = transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::createTypedRPCOnOrderFacility
        <GS::Input,GS::Output>(
        transport::ConnectionLocator::parse("127.0.0.1::guest:guest:test_db_one_list_cmd_subscription_queue")
    );
    r.registerOnOrderFacility("gsFacility", gsFacility);
    auto gsSubscriptionCmdCreator = M::constFirstPushKeyImporter<GS::Input>(
        GS::Input {
            GS::Subscription { std::vector<Key> {Key {}} }
        }
    );
    r.registerImporter("gsSubscriptionCmdCreator", gsSubscriptionCmdCreator);
    auto gsInputPipe = M::kleisli<M::Key<GS::Input>>(basic::CommonFlowUtilComponents<M>::idFunc<M::Key<GS::Input>>());
    r.registerAction("gsInputPipe", gsInputPipe);
    auto gsClientOutputs = basic::transaction::v2::dataStreamClientCombination<
        R, DI
        , basic::transaction::v2::TriviallyMerge<int64_t, int64_t>
        , ApplyDelta
    >(
        r 
        , "gsOutputHandling"
        , R::facilityConnector(gsFacility)
        , r.execute(gsInputPipe, r.importItem(gsSubscriptionCmdCreator))
    );
    auto extractFullUpdateData = M::liftPure<M::KeyedData<GS::Input,DI::FullUpdate>>(
        [](M::KeyedData<GS::Input,DI::FullUpdate> &&d) -> DI::FullUpdate {
            return std::move(d.data);
        }
    );
    r.registerAction("extractFullUpdateData", extractFullUpdateData);
    updateSink(r, r.execute(extractFullUpdateData, gsClientOutputs.fullUpdates.clone()));
    auto gsIDPtr = std::make_shared<TheEnvironment::IDType>();
    r.preservePointer(gsIDPtr);
    auto gsIDSaver = M::pureExporter<M::KeyedData<GS::Input,GS::Output>>(
        [gsIDPtr](M::KeyedData<GS::Input,GS::Output> &&update) {
            auto id = update.key.id();
            std::visit([&id,gsIDPtr](auto const &x) {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T,GS::Subscription>) {
                    *gsIDPtr = id;
                }
            }, update.data.value);
        }
    );
    r.registerExporter("gsIDSaver", gsIDSaver);
    r.exportItem(gsIDSaver, gsClientOutputs.rawSubscriptionOutputs.clone());
    auto gsUnsubscriber = M::liftPure<GuiExitEvent>(
        [gsIDPtr](GuiExitEvent &&) {
            return M::Key<GS::Input>(GS::Input {
                GS::Unsubscription {TheEnvironment::id_to_string(*gsIDPtr)}
            });
        }
    );
    r.registerAction("gsUnsubscriber", gsUnsubscriber);
    exitEventSource(r, r.actionAsSink(gsUnsubscriber));
    r.execute(gsInputPipe, r.actionAsSource(gsUnsubscriber));
    auto unsubscribeDetector = M::liftMaybe<M::KeyedData<GS::Input,GS::Output>>(
        [gsIDPtr](M::KeyedData<GS::Input,GS::Output> &&update) -> std::optional<UnsubscribeConfirmed> {
            auto id = update.key.id();
            return std::visit([&id,gsIDPtr](auto const &x) -> std::optional<UnsubscribeConfirmed> {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T,GS::Unsubscription>) {
                    if (id == *gsIDPtr) {
                        return UnsubscribeConfirmed {};
                    } else {
                        return std::nullopt;
                    }
                } else {
                    return std::nullopt;
                }
            }, update.data.value);
        }
    );
    r.registerAction("unsubscribeDetector", unsubscribeDetector);
    unsubscribeSink(r, r.execute(unsubscribeDetector, gsClientOutputs.rawSubscriptionOutputs.clone()));

    //Now set up transactions
    auto tiFacility = transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::createTypedRPCOnOrderFacility
        <TI::Transaction,TI::TransactionResponse>(
        transport::ConnectionLocator::parse("127.0.0.1::guest:guest:test_db_one_list_cmd_transaction_queue")
    );
    r.registerOnOrderFacility("tiFacility", tiFacility);
    auto tiKeyify = M::template kleisli<typename TI::Transaction>(
        basic::CommonFlowUtilComponents<M>::template keyify<typename TI::Transaction>()
    );
    r.registerAction("tiKeyify", tiKeyify);
    transactionCommandSource(r, r.actionAsSink(tiKeyify));
    r.placeOrderWithFacilityAndForget(r.actionAsSource(tiKeyify), tiFacility);
}