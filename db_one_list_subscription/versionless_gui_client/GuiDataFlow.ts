import * as TMInfra from '../../../tm_infra/node_lib/TMInfra'
import * as TMBasic from '../../../tm_basic/node_lib/TMBasic'
import * as TMTransport from '../../../tm_transport/node_lib/TMTransport'
import * as cbor from 'cbor'
import * as util from 'util'

//for C++ struct CBOR types that are serialized with field names, we use interfaces
//, for those that are serialized without field names, we use types
export type DBKey = {name: string};
export type DBData = {amount: number, stat: number};
type DBItem = [DBKey, DBData];
interface DBDelta {
    deletes : DBKey[];
    inserts_updates : DBItem[];
}

type Key = TMBasic.VoidStruct;
type Data = Map<DBKey, DBData>;
export type LocalData = Map<string, DBData>;
type DataSummary = number;
type DataDelta = DBDelta;

type GSInput = TMBasic.Transaction.GeneralSubscriber.Input<Key>;
type GSOutput = TMBasic.Transaction.GeneralSubscriber.Output<number, Key, number, Data, number, DataDelta>;
type GSOutput_Update = TMBasic.Transaction.GeneralSubscriber.SubscriptionUpdate<number, Key, number, Data, number, DataDelta>;

export type TIInput = TMBasic.Transaction.TransactionInterface.Transaction<Key,number,Data,DataSummary,number,DataDelta>;
type TIOutput = TMBasic.Transaction.TransactionInterface.TransactionResponse<number>;

export interface GuiExitEvent {}
export interface UnsubscribeConfirmed {}

type E = TMBasic.ClockEnv;

export interface LogicInput {
    dataHandler : (d : TMInfra.TimedDataWithEnvironment<TMBasic.ClockEnv,LocalData>) => void;
    unsubscribeConfirmedHandler : (d : TMInfra.TimedDataWithEnvironment<TMBasic.ClockEnv,UnsubscribeConfirmed>) => void;
}
export interface LogicOutput {
    tiInputFeeder : (x : TMInfra.Key<TIInput>) => void;
    guiExitEventFeeder : (x : GuiExitEvent) => void;
}

export const theKey : TMBasic.VoidStruct = 0;

export async function guiDataFlow(logicInput : LogicInput) : Promise<LogicOutput> {
    let r = new TMInfra.RealTimeApp.Runner<E>(new TMBasic.ClockEnv(null, "console_gui_client.log"));

    let heartbeat = await TMTransport.RemoteComponents.fetchTypedFirstUpdateAndDisconnect<TMTransport.RemoteComponents.Heartbeat>(
        (d) => cbor.decode(d) as TMTransport.RemoteComponents.Heartbeat
        , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
        , "versionless_db_one_list_subscription_server.heartbeat"
        , (data) => /versionless_db_one_list_subscription_server/.test(data.sender_description)
    );

    let gsFacility = TMTransport.RemoteComponents.createFacilityProxy<
        E, GSInput, GSOutput
    >(
        (t : GSInput) => cbor.encode(t)
        , function(d: Buffer) : GSOutput {
            let o = cbor.decode(d) as GSOutput;
            if (o[0] == 2) {
                //update
                TMBasic.Transaction.DataStreamInterface.eraseVersion(
                    o[1] as GSOutput_Update
                );
            }
            return o;
        }
        , {
            address : heartbeat.content.facility_channels["transaction_server_components/subscription_handler"]
            , identityAttacher : TMTransport.RemoteComponents.Security.simpleIdentityAttacher("ConsoleGuiClient.ts")
        }
    );
    
    let subscribeImporter = TMInfra.RealTimeApp.Utils.constFirstPushKeyImporter<E,GSInput>(
        [
            TMBasic.Transaction.GeneralSubscriber.InputSubtypes.Subscription
            , {keys : [theKey]} as TMBasic.Transaction.GeneralSubscriber.Subscription<Key>
        ]
    );
    let localData : LocalData = new Map<string, DBData>();
    let gsComboOutput = TMBasic.Transaction.Helpers.clientGSCombo<
        E, LocalData, number, Key, number, Data, number, DataDelta
    >(
        r
        , gsFacility
        , localData
        , (ld : LocalData, delta : TMBasic.Transaction.DataStreamInterface.OneDeltaUpdateItem<Key,number,DataDelta>) => {
            let d = delta[2];
            for (let k of d.deletes) {
                ld.delete(k.name);
            }
            for (let iu of d.inserts_updates) {
                ld.set(iu[0].name, iu[1]);
            }
        }
        , (ld : LocalData, full : TMBasic.Transaction.DataStreamInterface.OneFullUpdateItem<Key,number,Data>) => {
            ld.clear();
            if (full.data.length != 0) {
                for (let item of Array.from(full.data[0].entries())) {
                    ld.set(item[0].name, item[1]);
                }
            }
        }
    );
    let localDataExporter = TMInfra.RealTimeApp.Utils.simpleExporter<
        E,LocalData
    >(logicInput.dataHandler);
    let subscriptionID : string = "";
    let subscriptionIDSaver = TMInfra.RealTimeApp.Utils.pureExporter<
        E,TMInfra.KeyedData<GSInput,GSOutput>
    >(
        (x : TMInfra.KeyedData<GSInput,GSOutput>) => {
            if (x.data[0] == TMBasic.Transaction.GeneralSubscriber.OutputSubtypes.Subscription) {
                subscriptionID = x.key.id;
            }
        }
    );
    let unsubscribeDetector = TMInfra.RealTimeApp.Utils.liftMaybe<
        E,TMInfra.KeyedData<GSInput,GSOutput>,UnsubscribeConfirmed
    >(
        (x : TMInfra.KeyedData<GSInput,GSOutput>) => {
            if (x.data[0] == TMBasic.Transaction.GeneralSubscriber.OutputSubtypes.Unsubscription) {
                let id = (x.data[1] as TMBasic.Transaction.GeneralSubscriber.Unsubscription).originalSubscriptionID;
                if (id == subscriptionID) {
                    return {};
                }
            }
            return null;
        }
    );
    let guiExitImporter = new TMInfra.RealTimeApp.Utils.ConstTriggerImporter<E,GuiExitEvent>(
        {} as GuiExitEvent
    );
    let guiExitHandler = TMInfra.RealTimeApp.Utils.liftMaybe<
        E,GuiExitEvent,TMInfra.Key<GSInput>
    >(
        (_x : GuiExitEvent) => {
            if (subscriptionID != "") {
                return TMInfra.keyify([
                    TMBasic.Transaction.GeneralSubscriber.InputSubtypes.Unsubscription
                    , {originalSubscriptionID : subscriptionID} as TMBasic.Transaction.GeneralSubscriber.Unsubscription
                ]);
            } else {
                return null;
            }
        }
    );
    let unsubscribeHandler = TMInfra.RealTimeApp.Utils.simpleExporter<
        E, UnsubscribeConfirmed
    >(logicInput.unsubscribeConfirmedHandler);
    let guiExitTimeout = TMBasic.ClockOnOrderFacility.createClockCallback<E, TMBasic.VoidStruct, UnsubscribeConfirmed>(
        function (_d : Date, _index : number, _count : number) : UnsubscribeConfirmed {
            return {};
        } 
    );
    let guiExitTimeoutInput = TMInfra.RealTimeApp.Utils.liftPure(
        function(_x : GuiExitEvent) : TMInfra.Key<TMBasic.ClockOnOrderFacilityInput<TMBasic.VoidStruct>> {
            return TMInfra.keyify({
                inputData : 0
                , callbackDurations : [1000]
            });
        }
    );
    let guiExitTimeoutOutput = TMInfra.RealTimeApp.Utils.liftPure(
        function(x : TMInfra.KeyedData<TMBasic.ClockOnOrderFacilityInput<TMBasic.VoidStruct>,UnsubscribeConfirmed>) : UnsubscribeConfirmed {
            return x.data;
        }
    );
    let tiFacility = TMTransport.RemoteComponents.createFacilityProxy<
        E, TIInput, TIOutput
    >(
        (t : TIInput) => cbor.encode(t)
        , (d : Buffer) => cbor.decode(d) as TIOutput
        , {
            address : heartbeat.content.facility_channels["transaction_server_components/transaction_handler"]
            , identityAttacher : TMTransport.RemoteComponents.Security.simpleIdentityAttacher("ConsoleGuiClient.ts")
        }
    );
    let tiImporter = new TMInfra.RealTimeApp.Utils.TriggerImporter<E,TMInfra.Key<TIInput>>();

    r.connect(r.importItem(subscribeImporter), gsComboOutput.gsInputSink);
    r.connect(r.execute(guiExitHandler, r.importItem(guiExitImporter)), gsComboOutput.gsInputSink);
    r.exportItem(subscriptionIDSaver, gsComboOutput.gsOutputSource);
    r.exportItem(localDataExporter, gsComboOutput.localDataSource);
    r.placeOrderWithFacility(
        r.execute(guiExitTimeoutInput, r.importItem(guiExitImporter))
        , guiExitTimeout
        , r.actionAsSink(guiExitTimeoutOutput)
    );
    r.exportItem(unsubscribeHandler, r.execute(unsubscribeDetector, gsComboOutput.gsOutputSource));
    r.exportItem(unsubscribeHandler, r.actionAsSource(guiExitTimeoutOutput));
    r.placeOrderWithFacilityAndForget(r.importItem(tiImporter),tiFacility);
    r.finalize();

    return {
        tiInputFeeder : (x : TMInfra.Key<TIInput>) => {
            r.environment().log(TMInfra.LogLevel.Info, `Sending command ${util.inspect(x.key, {depth: null})}`);
            tiImporter.trigger(x);
        }
        , guiExitEventFeeder : (_x : GuiExitEvent) => {
            r.environment().log(TMInfra.LogLevel.Info, "Exiting from GUI");
            guiExitImporter.trigger();
        }
    }
}