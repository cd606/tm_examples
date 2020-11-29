#include "EnablerGUIDataFlow.hpp"
#include <tm_kit/basic/transaction/v2/DataStreamClientCombination.hpp>
#include <tm_kit/transport/MultiTransportBroadcastListenerManagingUtils.hpp>
#include <tm_kit/transport/RemoteTransactionSubscriberManagingUtils.hpp>

void enablerGUIDataFlow(
    R &r
    , std::string const &clientName
    , R::Sourceoid<bool> const &configureSource
    , R::Sinkoid<bool> const &statusSink
    , std::optional<R::Source<basic::VoidStruct>> const &exitSource
) {
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
    r.environment()->setLogFilePrefix(clientName, true);

    auto heartbeatSource = 
        transport::MultiTransportBroadcastListenerManagingUtils<R>
        ::oneBroadcastListener<
            transport::HeartbeatMessage
        >(
            r 
            , "heartbeatListener"
            , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
            , "simple_demo_chain_version.#.heartbeat"
        );
    auto enableServerSubscriberAndUpdater = transport::RemoteTransactionSubscriberManagingUtils<R>
        ::createSubscriberAndUpdater<GS,TI>
        (
            r 
            , heartbeatSource.clone()
            , std::regex("simple_demo_chain_version Enable Server")
            , "transaction_server_components/subscription_handler"
            , "transaction_server_components/transaction_handler"
            , GS::Subscription {{basic::VoidStruct {}}}
            , exitSource
        );
    std::shared_ptr<basic::transaction::v2::TransactionDataStore<DI>> dataStore;
    auto enableServerDataSource = basic::transaction::v2::basicDataStreamClientCombination<R,DI,GS::Input>(
        r 
        , "translateEnableServerDataSource"
        , enableServerSubscriberAndUpdater.feedSubscriberData
        , &dataStore
    );
    auto convertToBool = M::liftPure<M::KeyedData<GS::Input,DI::FullUpdate>>(
        [](M::KeyedData<GS::Input,DI::FullUpdate> &&update) -> bool {
            bool res = false;
            for (auto const &oneUpdate : update.data.data) {
                res = (oneUpdate.data && *(oneUpdate.data));
            }
            return res;
        }
    );
    r.registerAction("convertToBool", convertToBool);
    r.execute(convertToBool, std::move(enableServerDataSource));
    statusSink(r, r.actionAsSource(convertToBool));

    auto createCommand = M::liftPure<bool>(
        [dataStore](bool &&x) -> M::Key<TI::Transaction> {
            return M::keyify(
                TI::Transaction {
                    TI::UpdateAction {
                        Key {}
                        , {dataStore->globalVersion_}
                        , DataSummary {}
                        , Data {x}
                    }
                }
            );
        }
    );

    r.registerAction("createCommand", createCommand);
    configureSource(r, r.actionAsSink(createCommand));

    auto discardResult = M::trivialExporter<M::KeyedData<TI::Transaction,TI::TransactionResponse>>();
    r.registerExporter("discardResult", discardResult);

    enableServerSubscriberAndUpdater.connectUpdateRequest(
        r 
        , r.actionAsSource(createCommand)
        , r.exporterAsSink(discardResult)
    );
}

void enablerOneShotDataFlow(
    R &r
    , std::string const &clientName
    , bool enableCommand
) {
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

    auto heartbeatSource = 
        transport::MultiTransportBroadcastListenerManagingUtils<R>
        ::oneBroadcastListener<
            transport::HeartbeatMessage
        >(
            r 
            , "heartbeatListener"
            , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
            , "simple_demo_chain_version.#.heartbeat"
        );
    auto *env = r.environment();

    auto enableServerUpdater = transport::MultiTransportRemoteFacilityManagingUtils<R>
        ::setupOneNonDistinguishedRemoteFacility<TI::Transaction,TI::TransactionResponse>
        (
            r 
            , heartbeatSource.clone()
            , std::regex("simple_demo_chain_version Enable Server")
            , "transaction_server_components/transaction_handler"
        );
    auto sendRequest = M::liftMaybe<std::size_t>(
        [enableCommand](std::size_t &&count) -> std::optional<M::Key<TI::Transaction>> {
            if (count > 0) {
                return M::keyify(
                    TI::Transaction {
                        TI::UpdateAction {
                            Key {}
                            , std::nullopt
                            , DataSummary {}
                            , Data {enableCommand}
                        }
                    }
                );
            } else {
                return std::nullopt;
            }
        }
        , infra::LiftParameters<M::TimePoint>().FireOnceOnly(true)
    );
    auto wrapUp = M::pureExporter<M::KeyedData<TI::Transaction,TI::TransactionResponse>>(
        [env](M::KeyedData<TI::Transaction,TI::TransactionResponse> &&data) {
            if (data.data.value.requestDecision == basic::transaction::v2::RequestDecision::Success) {
                env->log(infra::LogLevel::Info, "Succeeded");
            } else {
                env->log(infra::LogLevel::Info, "Failed");
            }
            env->exit();
        }
    );
    r.registerAction("sendRequest", sendRequest);
    r.registerExporter("wrapUp", wrapUp);
    enableServerUpdater.feedUnderlyingCount(r, r.actionAsSink(sendRequest));
    enableServerUpdater.facility(r, r.actionAsSource(sendRequest), r.exporterAsSink(wrapUp));
}