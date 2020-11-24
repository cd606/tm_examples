import * as blessed from 'blessed'
import * as TMInfra from '../../../tm_infra/node_lib/TMInfra'
import * as TMBasic from '../../../tm_basic/node_lib/TMBasic'
import * as TMTransport from '../../../tm_transport/node_lib/TMTransport'
import * as cbor from 'cbor'
import * as util from 'util'

type Key = TMBasic.VoidStruct;
type Data = boolean;
type DataSummary = TMBasic.VoidStruct;
type DataDelta = boolean;

type GSInput = TMBasic.Transaction.GeneralSubscriber.Input<Key>;
type GSOutput = TMBasic.Transaction.GeneralSubscriber.Output<number, Key, number, Data, number, DataDelta>;
type Update = TMBasic.Transaction.DataStreamInterface.Update<number,Key,number,Data,number,DataDelta>;

type TIInput = TMBasic.Transaction.TransactionInterface.Transaction<Key,number,Data,DataSummary,number,DataDelta>;
type TIOutput = TMBasic.Transaction.TransactionInterface.TransactionResponse<number>;

type LocalData = TMInfra.VersionedData<number, boolean>;

interface GuiExitEvent {}
interface UnsubscribeConfirmed {}

type E = TMBasic.ClockEnv;

interface LogicInput {
    dataHandler : (d : TMInfra.TimedDataWithEnvironment<E,LocalData>) => void;
    unsubscribeConfirmedHandler : (d : TMInfra.TimedDataWithEnvironment<E,UnsubscribeConfirmed>) => void;
}
interface LogicOutput {
    tiInputFeeder : (x : TMInfra.Key<TIInput>) => void;
    guiExitEventFeeder : (x : GuiExitEvent) => void;
}

const theKey : TMBasic.VoidStruct = 0;

function tmLogic(logicInput : LogicInput) : LogicOutput {
    let r = new TMInfra.RealTimeApp.Runner<E>(new TMBasic.ClockEnv(null, "console_enabler.log"));

    let heartbeatImporter = TMTransport.RemoteComponents.createTypedImporter<E,TMTransport.RemoteComponents.Heartbeat>(
        (d : Buffer) => cbor.decode(d)
        , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
        , "simple_demo_chain_version.#.heartbeat"
    );
    let gsFacility = new TMTransport.RemoteComponents.DynamicFacilityProxy<E,GSInput,GSOutput>(
        (t : GSInput) => Buffer.from(cbor.encode(t))
        , (d : Buffer) => cbor.decode(d) as GSOutput
        , {
            address : null
            , identityAttacher : TMTransport.RemoteComponents.Security.simpleIdentityAttacher("ConsoleEnabler.ts")
        }
    );
    let subscriptionKeyImporter = new TMInfra.RealTimeApp.Utils.ConstTriggerImporter<E,GSInput>(
        [
            TMBasic.Transaction.GeneralSubscriber.InputSubtypes.Subscription
            , {keys : [theKey]} as TMBasic.Transaction.GeneralSubscriber.Subscription<Key>
        ]
    );
    let keyify = TMInfra.RealTimeApp.Utils.liftPure<E,GSInput,TMInfra.Key<GSInput>>(
        (x) => {
            return TMInfra.keyify(x);
        }
    );
    let localData : LocalData = {
        version : undefined
        , data : undefined
    };
    let gsComboOutput = TMBasic.Transaction.Helpers.clientGSCombo<
        E, LocalData, number, Key, number, Data, number, DataDelta
    >(
        r
        , gsFacility
        , localData
        , (ld : LocalData, delta : TMBasic.Transaction.DataStreamInterface.OneDeltaUpdateItem<Key,number,DataDelta>) => {
            if (ld.version == undefined || localData.version < delta[1]) {
                let d = delta[2];
                ld.data = d;
                ld.version = delta[1];
            }
        }
        , (ld : LocalData, full : TMBasic.Transaction.DataStreamInterface.OneFullUpdateItem<Key,number,Data>) => {
            if (ld.version == undefined || localData.version < full.version) {
                ld.version = full.version;
                if (full.data.length != 0) {
                    ld.data = full.data[0];
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
    let tiFacility = new TMTransport.RemoteComponents.DynamicFacilityProxy<E,TIInput,TIOutput>(
        (t : TIInput) => Buffer.from(cbor.encode(t))
        , (d : Buffer) => cbor.decode(d) as TIOutput
        , {
            address : null
            , identityAttacher : TMTransport.RemoteComponents.Security.simpleIdentityAttacher("ConsoleEnabler.ts")
        }
    );
    let tiImporter = new TMInfra.RealTimeApp.Utils.TriggerImporter<E,TMInfra.Key<TIInput>>();

    let heartbeatAction = TMInfra.RealTimeApp.Utils.pureExporter<E,TMBasic.TypedDataWithTopic<TMTransport.RemoteComponents.Heartbeat>>(
        (h : TMBasic.TypedDataWithTopic<TMTransport.RemoteComponents.Heartbeat>) => {
            if (h.content.sender_description == "simple_demo_chain_version Enable Server") {
                if (h.content.facility_channels.hasOwnProperty("transaction_server_components/subscription_handler")) {
                    let channelInfoFromHeartbeat = h.content.facility_channels["transaction_server_components/subscription_handler"];
                    if (gsFacility.changeAddress(channelInfoFromHeartbeat)) {
                        subscriptionKeyImporter.trigger();
                    }
                }
                if (h.content.facility_channels.hasOwnProperty("transaction_server_components/transaction_handler")) {
                    let channelInfoFromHeartbeat = h.content.facility_channels["transaction_server_components/transaction_handler"];
                    tiFacility.changeAddress(channelInfoFromHeartbeat);
                }
            }
        }
    );

    r.connect(r.execute(keyify, r.importItem(subscriptionKeyImporter)), gsComboOutput.gsInputSink);
    r.connect(r.execute(guiExitHandler, r.importItem(guiExitImporter)), gsComboOutput.gsInputSink);
    r.exportItem(subscriptionIDSaver, gsComboOutput.gsOutputSource);
    r.exportItem(localDataExporter, gsComboOutput.localDataSource);
    r.exportItem(unsubscribeHandler, r.execute(unsubscribeDetector, gsComboOutput.gsOutputSource));
    r.placeOrderWithFacilityAndForget(r.importItem(tiImporter),tiFacility);
    r.exportItem(heartbeatAction, r.importItem(heartbeatImporter));
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

function setup() {
    let screen = blessed.screen({
        smartCSR: true
        , title: 'Plain Console Enabler'
    });
    let label = blessed.box({
        parent: screen
        , top: '20%'
        , left: 'center'
        , width: '150'
        , content: 'Disconnected'
        , style: {
            fg: 'white'
        }
        , focusable: false
    });
    let form = blessed.form({
        parent: screen
        , top: '60%'
        , left: '10%'
        , width: '80%'
        , height: 5
        , border: {
            type: 'line'
        }
        , style: {
            border: {
                fg: 'blue'
            }
        }
        , keys: true
    });
    let enable = blessed.button({
        parent: form
        , left: '10%'
        , width: '30%'
        , height: 3
        , content: 'Enable'
        , border: {
            type: 'line'
        }
        , style: {
            fg: 'green'
            , border: {
                fg: 'blue'
            }, focus: {
                bg: 'white'
            }
        }
        , align: 'center'
    });
    let disable = blessed.button({
        parent: form
        , left: '60%'
        , width: '30%'
        , height: 3
        , content: 'Disable'
        , border: {
            type: 'line'
        }
        , style: {
            fg: 'red'
            , border: {
                fg: 'blue'
            }
            , focus: {
                bg: 'white'
            }
        }
        , align: 'center'
    });

    let dataCopy : LocalData = {
        version : undefined
        , data : undefined
    };
    let o : LogicOutput = tmLogic({
        dataHandler : (d : TMInfra.TimedDataWithEnvironment<E,LocalData>) => {
            dataCopy = d.timedData.value;
            if (dataCopy.version !== undefined) {
                if (dataCopy.data) {
                    label.content = 'Enabled';
                    label.style.fg = 'green';
                } else {
                    label.content = 'Disabled';
                    label.style.fg = 'red';
                }
                screen.render();
            }
        }
        , unsubscribeConfirmedHandler : (d : TMInfra.TimedDataWithEnvironment<E,UnsubscribeConfirmed>) => {
            d.environment.log(TMInfra.LogLevel.Info, "Unsubscription confirmed, exiting");
            d.environment.exit();
        }
    });

    form.focus();
    screen.key(['escape', 'q', 'C-c'], function(_ch, _key) {
        o.guiExitEventFeeder({});
    });
    enable.on('press', () => {
        if (dataCopy.version !== undefined) {
            o.tiInputFeeder(
                TMInfra.keyify([
                    TMBasic.Transaction.TransactionInterface.TransactionSubtypes.UpdateAction
                    , {
                        key : theKey
                        , oldVersionSlice : [dataCopy.version]
                        , oldDataSummary : []
                        , dataDelta : true
                    }
                ])
            );
        }
    });
    disable.on('press', () => {
        if (dataCopy.version !== undefined) {
            o.tiInputFeeder(
                TMInfra.keyify([
                    TMBasic.Transaction.TransactionInterface.TransactionSubtypes.UpdateAction
                    , {
                        key : theKey
                        , oldVersionSlice : [dataCopy.version]
                        , oldDataSummary : []
                        , dataDelta : false
                    }
                ])
            );
        }
    });
    screen.render();
}

setup();