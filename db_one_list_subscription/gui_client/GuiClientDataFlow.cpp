#include "GuiClientDataFlow.hpp"
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>

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
    auto gsFacility = transport::MultiTransportRemoteFacilityManagingUtils<R>
        ::setupSimpleRemoteFacilityWithProtocol<basic::CBOR,GS::Input,GS::Output>(
        r, "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_subscription_queue"
    );
    r.registerOnOrderFacility("gsFacility", gsFacility);
    auto gsSubscriptionCmdCreator = M::constFirstPushKeyImporter<GS::Input>(
        basic::transaction::named_value_store::subscription<M,db_data>()
    );
    r.registerImporter("gsSubscriptionCmdCreator", gsSubscriptionCmdCreator);
    //The reason that we pass gsSubscriptionCmdCreator through gsInputPipe is that,
    //by default, when an on-order facility is added into the data flow graph, its 
    //maximum output connection number is set at 1. (This can be changed through a 
    //method call to AppRunner, and as long as the call happens before finalize() it
    //will be effective, however, since we have a way to deal with the problem through 
    //introducing gsInputPipe here, we do not use this method.) In this program, we
    //want to pass two things to gsFacility, one is the subscription (at the beginning
    //of the program) and another is the unsubscription (at the end of the program), and
    //we do have two sources (gsSubscriptionCmdCreator above, and gsUnsubscriber below),
    //however, if we do two placeOrderWithFacility calls, then we need to provide two
    //handlers for the output from the facility, and this will cause the maximum output
    //connection number check to fail. If we make one of these calls placeOrderWithFacilityAndForget,
    //then we won't be able to process the corresponding callback, but we want both
    //callbacks in order for the program to function well. Therefore, we pass both 
    //gsSubscriptionCmdCreator and gsUnsubscriber through gsInputPipe, and thus only one
    //thing (the output from gsInputPipe) needs to be passed to gsFacility, and only one
    //handler for the output is needed (which is provided from within dataStreamClientCombination).
    auto gsInputPipe = M::kleisli<M::Key<GS::Input>>(basic::CommonFlowUtilComponents<M>::idFunc<M::Key<GS::Input>>());
    r.registerAction("gsInputPipe", gsInputPipe);
    auto gsClientOutputs = basic::transaction::v2::dataStreamClientCombination<
        R, DI
        , basic::transaction::named_value_store::VersionMerger
        , basic::transaction::named_value_store::ApplyDelta<db_data>
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
    auto tiFacility = transport::MultiTransportRemoteFacilityManagingUtils<R>
        ::setupSimpleRemoteFacilityWithProtocol<
            basic::CBOR, TI::Transaction, TI::TransactionResponse
        >(
            r, "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_transaction_queue"
        );
    r.registerOnOrderFacility("tiFacility", tiFacility);
    auto tiKeyify = M::template kleisli<typename TI::Transaction>(
        basic::CommonFlowUtilComponents<M>::template keyify<typename TI::Transaction>()
    );
    r.registerAction("tiKeyify", tiKeyify);
    transactionCommandSource(r, r.actionAsSink(tiKeyify));
    r.placeOrderWithFacilityAndForget(r.actionAsSource(tiKeyify), tiFacility);
}
