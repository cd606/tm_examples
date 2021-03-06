import * as blessed from 'blessed'
import * as TMInfra from '../../../tm_infra/node_lib/TMInfra'
import * as TMBasic from '../../../tm_basic/node_lib/TMBasic'
import * as TMTransport from '../../../tm_transport/node_lib/TMTransport'
import * as cbor from 'cbor'
import * as util from 'util'

//for C++ struct CBOR types that are serialized with field names, we use interfaces
//, for those that are serialized without field names, we use types
type DBKey = string;
type DBData = [number, number]; //first is name, second is amount
type DBItem = [DBKey, DBData];
interface DBDelta {
    deletes : DBKey[];
    inserts_updates : DBItem[];
}

type Key = TMBasic.VoidStruct;
type Data = Map<DBKey, DBData>;
type DataSummary = number;
type DataDelta = DBDelta;
type LocalData = TMInfra.VersionedData<number, Map<string, DBData>>;

type GSInput = TMBasic.Transaction.GeneralSubscriber.Input<Key>;
type GSOutput = TMBasic.Transaction.GeneralSubscriber.Output<number, Key, number, Data, number, DataDelta>;
type Update = TMBasic.Transaction.DataStreamInterface.Update<number,Key,number,Data,number,DataDelta>;

type TIInput = TMBasic.Transaction.TransactionInterface.Transaction<Key,number,Data,DataSummary,number,DataDelta>;
type TIOutput = TMBasic.Transaction.TransactionInterface.TransactionResponse<number>;

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
    let r = new TMInfra.RealTimeApp.Runner<E>(new TMBasic.ClockEnv(null, "console_gui_client.log"));

    let gsFacility = TMTransport.RemoteComponents.createFacilityProxy<
        E, GSInput, GSOutput
    >(
        (t : GSInput) => Buffer.from(cbor.encode(t))
        , (d : Buffer) => cbor.decode(d) as GSOutput
        , {
            address : "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_subscription_queue"
            , identityAttacher : TMTransport.RemoteComponents.Security.simpleIdentityAttacher("ConsoleGuiClient.ts")
        }
    );
    
    let subscribeImporter = TMInfra.RealTimeApp.Utils.constFirstPushKeyImporter<E,GSInput>(
        [
            TMBasic.Transaction.GeneralSubscriber.InputSubtypes.Subscription
            , {keys : [theKey]} as TMBasic.Transaction.GeneralSubscriber.Subscription<Key>
        ]
    );
    let localData : LocalData = {
        version : undefined
        , data : new Map<string, DBData>()
    }
    let gsComboOutput = TMBasic.Transaction.Helpers.clientGSCombo<
        E, LocalData, number, Key, number, Data, number, DataDelta
    >(
        r
        , gsFacility
        , localData
        , (ld : LocalData, delta : TMBasic.Transaction.DataStreamInterface.OneDeltaUpdateItem<Key,number,DataDelta>) => {
            if (ld.version == undefined || localData.version < delta[1]) {
                let d = delta[2];
                for (let k of d.deletes) {
                    ld.data.delete(k);
                }
                for (let iu of d.inserts_updates) {
                    ld.data.set(iu[0], iu[1]);
                }
                ld.version = delta[1];
            }
        }
        , (ld : LocalData, full : TMBasic.Transaction.DataStreamInterface.OneFullUpdateItem<Key,number,Data>) => {
            if (ld.version == undefined || localData.version < full.version) {
                ld.version = full.version;
                ld.data.clear();
                if (full.data.length != 0) {
                    for (let item of Object.entries(full.data[0])) {
                        ld.data.set(item[0], item[1]);
                    }
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
    let tiFacility = TMTransport.RemoteComponents.createFacilityProxy<
        E, TIInput, TIOutput
    >(
        (t : TIInput) => Buffer.from(cbor.encode(t))
        , (d : Buffer) => cbor.decode(d) as TIOutput
        , {
            address : "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_transaction_queue"
            , identityAttacher : TMTransport.RemoteComponents.Security.simpleIdentityAttacher("ConsoleGuiClient.ts")
        }
    );
    let tiImporter = new TMInfra.RealTimeApp.Utils.TriggerImporter<E,TMInfra.Key<TIInput>>();

    r.connect(r.importItem(subscribeImporter), gsComboOutput.gsInputSink);
    r.connect(r.execute(guiExitHandler, r.importItem(guiExitImporter)), gsComboOutput.gsInputSink);
    r.exportItem(subscriptionIDSaver, gsComboOutput.gsOutputSource);
    r.exportItem(localDataExporter, gsComboOutput.localDataSource);
    r.exportItem(unsubscribeHandler, r.execute(unsubscribeDetector, gsComboOutput.gsOutputSource));
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

function setup() {
    let screen = blessed.screen({
        smartCSR: true
        , title: 'Console DB One List Client'
    });
    let table = blessed.listtable({
        parent: screen
        , top: '10%'
        , left: 'center'
        , width: '80%'
        , height: '40%'
        , border : {
            type: 'line'
        }
        , style: {
            cell : {
                border : {
                    fg: 'white'
                }
            }
        }
        , align: 'center'
        , scrollable : true
    });
    table.setRows([["Name", "Amount", "Stat"]]);
    let form = blessed.form({
        parent: screen
        , top: '60%'
        , left: '10%'
        , width: '80%'
        , height: '30%'
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
    blessed.text({
        parent: form
        , left: '6%'
        , height: 3
        , content: 'Name:' 
        , valign : 'middle'
    });
    let nameInput = blessed.textbox({
        parent: form
        , left: '13%'
        , width: '20%'
        , height: 3
        , border: {
            type: 'line'
        }
        , style: {
            border: {
                fg: 'blue'
            }, focus: {
                fg: 'white'
                , bg: 'blue'
            }
        }
        , inputOnFocus: true
    });
    blessed.text({
        parent: form
        , left: '36%'
        , height: 3
        , content: 'Amount:' 
        , valign : 'middle'
    });
    let amtInput = blessed.textbox({
        parent: form
        , left: '43%'
        , width: '20%'
        , height: 3
        , content: 'Enable'
        , border: {
            type: 'line'
        }
        , style: {
            border: {
                fg: 'blue'
            }, focus: {
                fg: 'white'
                , bg: 'blue'
            }
        }
        , inputOnFocus: true
    });
    blessed.text({
        parent: form
        , left: '66%'
        , height: 3
        , content: 'Stat:' 
        , valign : 'middle'
    });
    let statInput = blessed.textbox({
        parent: form
        , left: '73%'
        , width: '20%'
        , height: 3
        , content: 'Enable'
        , border: {
            type: 'line'
        }
        , style: {
            border: {
                fg: 'blue'
            }, focus: {
                fg: 'white'
                , bg: 'blue'
            }
        }
        , inputOnFocus: true
    });
    let insertBtn = blessed.button({
        parent: form
        , left: '5%'
        , top: 4
        , width: '60%'
        , height: 3
        , content: 'Insert/Update'
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
    let deleteBtn = blessed.button({
        parent: form
        , left: '70%'
        , top: 4
        , width: '25%'
        , height: 3
        , content: 'Delete'
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

    let dataCopy : LocalData = {
        version : undefined
        , data : null
    };
    let o : LogicOutput = tmLogic({
        dataHandler : (d : TMInfra.TimedDataWithEnvironment<E,LocalData>) => {
            dataCopy = d.timedData.value;
            let rows : string[][] = [["Name", "Amount", "Stat"]];
            for (let r of dataCopy.data.entries()) {
                rows.push([r[0], ''+r[1][0], ''+r[1][1]]);
            }
            table.setRows(rows);
            screen.render();
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
    insertBtn.on('press', () => {
        if (dataCopy.version != undefined) {
            o.tiInputFeeder(TMInfra.keyify([
                TMBasic.Transaction.TransactionInterface.TransactionSubtypes.UpdateAction
                , {
                    key : theKey
                    , oldVersionSlice : [dataCopy.version]
                    , oldDataSummary : [dataCopy.data.size]
                    , dataDelta : {
                        deletes: []
                        , inserts_updates : [[
                                nameInput.getValue().trim()
                                , [parseInt(amtInput.getValue().trim()), parseFloat(statInput.getValue().trim())]
                        ]]
                    }
                }
            ]));
        }
    });
    deleteBtn.on('press', () => {
        if (dataCopy.version != undefined) {
            o.tiInputFeeder(TMInfra.keyify([
                TMBasic.Transaction.TransactionInterface.TransactionSubtypes.UpdateAction
                , {
                    key : theKey
                    , oldVersionSlice : [dataCopy.version]
                    , oldDataSummary : [dataCopy.data.size]
                    , dataDelta : {
                        deletes: [nameInput.getValue().trim()]
                        , inserts_updates : []
                    }
                }
            ]));
        }
    })
    screen.render();
}

setup();