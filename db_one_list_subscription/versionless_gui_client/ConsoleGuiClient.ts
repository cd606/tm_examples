import * as blessed from 'blessed'
import * as GuiDataFlow from './GuiDataFlow'
import * as TMInfra from '../../../tm_infra/node_lib/TMInfra'
import * as TMBasic from '../../../tm_basic/node_lib/TMBasic'
import * as TMTransport from '../../../tm_transport/node_lib/TMTransport'
import * as cbor from 'cbor'
import * as util from 'util'

async function setup() {
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

    let dataCopy : GuiDataFlow.LocalData = null;
    let o : GuiDataFlow.LogicOutput = await GuiDataFlow.guiDataFlow({
        dataHandler : (d : TMInfra.TimedDataWithEnvironment<TMBasic.ClockEnv,GuiDataFlow.LocalData>) => {
            dataCopy = d.timedData.value;
            let rows : string[][] = [["Name", "Amount", "Stat"]];
            for (let r of dataCopy.entries()) {
                rows.push([r[0], ''+r[1].amount, ''+r[1].stat]);
            }
            table.setRows(rows);
            screen.render();
        }
        , unsubscribeConfirmedHandler : (d : TMInfra.TimedDataWithEnvironment<TMBasic.ClockEnv,GuiDataFlow.UnsubscribeConfirmed>) => {
            d.environment.log(TMInfra.LogLevel.Info, "Unsubscription confirmed, exiting");
            d.environment.exit();
        }
    });

    form.focus();
    screen.key(['escape', 'q', 'C-c'], function(_ch, _key) {
        o.guiExitEventFeeder({});
    });
    insertBtn.on('press', () => {
        o.tiInputFeeder(TMInfra.keyify([
            TMBasic.Transaction.TransactionInterface.TransactionSubtypes.UpdateAction
            , {
                key : GuiDataFlow.theKey
                , oldVersionSlice : []
                , oldDataSummary : [dataCopy.size]
                , dataDelta : {
                    deletes: []
                    , inserts_updates : [[
                            {'name': nameInput.getValue().trim()}
                            , {'amount': parseInt(amtInput.getValue().trim()), 'stat': parseFloat(statInput.getValue().trim())}
                    ]]
                }
            }
        ]))
    });
    deleteBtn.on('press', () => {
        o.tiInputFeeder(TMInfra.keyify([
            TMBasic.Transaction.TransactionInterface.TransactionSubtypes.UpdateAction
            , {
                key : GuiDataFlow.theKey
                , oldVersionSlice : []
                , oldDataSummary : [dataCopy.size]
                , dataDelta : {
                    deletes: [{'name': nameInput.getValue().trim()}]
                    , inserts_updates : []
                }
            }
        ]));
    })
    screen.render();
}

setup();