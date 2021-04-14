#include "GuiClientDataFlow.hpp"
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>

namespace {
    const std::string SERVER_HEARTBEAT_ID = "versionless_db_one_list_subscription_server";
}

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

    auto heartbeatSource = 
        transport::MultiTransportBroadcastListenerManagingUtils<R>
        ::oneBroadcastListener<
            transport::HeartbeatMessage
        >(
            r 
            , "heartbeatListener"
            , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
            , SERVER_HEARTBEAT_ID+".heartbeat"
        );
    auto tiFacilityInfo = transport::MultiTransportRemoteFacilityManagingUtils<R>
        ::setupOneNonDistinguishedRemoteFacility<TI::Transaction,TI::TransactionResponse>(
            r 
            , heartbeatSource.clone()
            , std::regex(SERVER_HEARTBEAT_ID)
            , "transaction_server_components/transaction_handler"
        );
    auto diFacilityInfo = transport::MultiTransportRemoteFacilityManagingUtils<R>
        ::setupOneDistinguishedRemoteFacility<GS::Input,GS::Output>(
            r 
            , heartbeatSource.clone()
            , std::regex(SERVER_HEARTBEAT_ID)
            , "transaction_server_components/subscription_handler"
            , []() -> GS::Input {
                return GS::Input {
                    GS::Subscription { std::vector<DI::Key> {DI::Key {}} }
                };
            }
            , [](GS::Input const &, GS::Output const &) {
                return true;
            }
        );

    using FacilityKey = std::tuple<transport::ConnectionLocator, GS::Input>;
    auto clientOutputs = basic::transaction::complex_key_value_store::as_collection::Combinations<R,DBKey,DBData>
        ::basicDataStreamClientCombinationFunc<FacilityKey>()
    (
        r 
        , "outputHandling"
        , diFacilityInfo.feedOrderResults
        , nullptr
    );

    auto extractFullUpdateData = M::liftPure<M::KeyedData<FacilityKey,DI::FullUpdate>>(
        [](M::KeyedData<FacilityKey,DI::FullUpdate> &&d) -> DI::FullUpdate {
            return std::move(d.data);
        }
    );
    r.registerAction("extractFullUpdateData", extractFullUpdateData);
    updateSink(r, r.execute(extractFullUpdateData, clientOutputs.clone()));

    auto gsIDPtr = std::make_shared<std::map<transport::ConnectionLocator, TheEnvironment::IDType>>();
    r.preservePointer(gsIDPtr);
    auto gsIDSaver = M::pureExporter<M::KeyedData<FacilityKey,GS::Output>>(
        [gsIDPtr](M::KeyedData<FacilityKey,GS::Output> &&update) {
            auto id = update.key.id();
            std::visit([&update,&id,gsIDPtr](auto const &x) {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T,GS::Subscription>) {
                    (*gsIDPtr)[std::get<0>(update.key.key())] =  id;
                }
            }, update.data.value);
        }
    );
    diFacilityInfo.feedOrderResults(r, r.exporterAsSink("gsIDSaver", gsIDSaver));

    auto removeID = M::pureExporter<std::tuple<transport::ConnectionLocator,bool>>(
        [gsIDPtr](std::tuple<transport::ConnectionLocator,bool> &&t) {
            if (!std::get<1>(t)) {
                gsIDPtr->erase(std::get<0>(t));
            }
        }
    );
    diFacilityInfo.feedConnectionChanges(r, r.exporterAsSink("removeID", removeID));

    auto gsUnsubscriber = M::liftMulti<GuiExitEvent>(
        [gsIDPtr](GuiExitEvent &&) -> std::vector<M::Key<FacilityKey>> {
            std::vector<M::Key<FacilityKey>> ret;
            for (auto const &item : *gsIDPtr) {
                ret.push_back(M::keyify(FacilityKey {
                    item.first
                    , GS::Input {
                        GS::Unsubscription {TheEnvironment::id_to_string(item.second)}
                    }
                }));
            }
            return ret;
        }
    );
    r.registerAction("gsUnsubscriber", gsUnsubscriber);
    exitEventSource(r, r.actionAsSink(gsUnsubscriber));
    diFacilityInfo.orderReceiver(r, r.actionAsSource(gsUnsubscriber));

    auto unsubscribeDetector = M::liftMaybe<M::KeyedData<FacilityKey,GS::Output>>(
        [gsIDPtr](M::KeyedData<FacilityKey,GS::Output> &&update) -> std::optional<UnsubscribeConfirmed> {
            auto id = update.key.id();
            return std::visit([&id,gsIDPtr](auto const &x) -> std::optional<UnsubscribeConfirmed> {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T,GS::Unsubscription>) {
                    std::optional<transport::ConnectionLocator> l = std::nullopt;
                    for (auto const &item : *gsIDPtr) {
                        if (item.second == id) {
                            l = item.first;
                            break;
                        }
                    }
                    if (l) {
                        gsIDPtr->erase(*l);
                    }
                    if (gsIDPtr->empty()) {
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
    diFacilityInfo.feedOrderResults(r, r.actionAsSink(unsubscribeDetector));
    unsubscribeSink(r, r.actionAsSource(unsubscribeDetector));
    
    using COF = basic::real_time_clock::ClockOnOrderFacility<TheEnvironment>;
    auto unsubscribeTimeout = COF::createClockCallback<basic::VoidStruct, UnsubscribeConfirmed>
        (
            [](std::chrono::system_clock::time_point const &, std::size_t, std::size_t) {
                return UnsubscribeConfirmed {};
            }
        );
    r.registerOnOrderFacility("unsubscribeTimeout", unsubscribeTimeout);
    auto unsubscribeTimeoutInput = M::liftPure<GuiExitEvent>(
        [](GuiExitEvent &&) -> M::Key<COF::FacilityInput<basic::VoidStruct>> {
            return {{
                {}
                , {std::chrono::seconds(1)}
            }};
        }
    );
    r.registerAction("unsubscribeTimeoutInput", unsubscribeTimeoutInput);
    auto unsubscribeTimeoutOutput = infra::KleisliUtils<M>::action(
        basic::CommonFlowUtilComponents<M>::extractDataFromKeyedData<
            COF::FacilityInput<basic::VoidStruct>, UnsubscribeConfirmed
        >()
    );
    r.registerAction("unsubscribeTimeoutOutput", unsubscribeTimeoutOutput);
    exitEventSource(r, r.actionAsSink(unsubscribeTimeoutInput));
    r.placeOrderWithFacility(
        r.actionAsSource(unsubscribeTimeoutInput)
        , unsubscribeTimeout
        , r.actionAsSink(unsubscribeTimeoutOutput)
    );
    unsubscribeSink(r, r.actionAsSource(unsubscribeTimeoutOutput));

    //Now set up transactions
    auto tiKeyify = M::template kleisli<typename TI::Transaction>(
        basic::CommonFlowUtilComponents<M>::template keyify<typename TI::Transaction>()
    );
    r.registerAction("tiKeyify", tiKeyify);
    transactionCommandSource(r, r.actionAsSink(tiKeyify));
    tiFacilityInfo.facility(
        r
        , r.actionAsSource(tiKeyify)
        , std::nullopt
    );
}
