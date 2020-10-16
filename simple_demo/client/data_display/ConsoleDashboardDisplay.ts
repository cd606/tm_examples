import * as blessed from 'blessed'
import * as bcontrib from 'blessed-contrib'
import * as TMInfra from '../../../../tm_infra/node_lib/TMInfra'
import * as TMBasic from '../../../../tm_basic/node_lib/TMBasic'
import * as TMTransport from '../../../../tm_transport/node_lib/TMTransport'

import * as yargs from 'yargs'
import * as cbor from 'cbor'
import * as proto from 'protobufjs'
import * as sodium from 'sodium-native'

const printf = require('printf'); //this module cannot be "import"ed in Typescript

yargs
    .scriptName("ConsoleDashboardDisplay")
    .usage("$0 <options>")
    .option('secure', {
        describe: 'use with secure data source'
        , type: 'boolean'
        , demand: false
        , default: false
    });

let secure = yargs.argv.secure as boolean;

let decryptKey = Buffer.alloc(sodium.crypto_generichash_BYTES);
sodium.crypto_generichash(decryptKey, Buffer.from("input_data_key"));
decryptKey = decryptKey.slice(0, 32);

proto.load('../../proto/defs.proto').then(function(root) {
    let parser = root.lookupType("simple_demo.InputData");
    start(parser);
});

function start(parser : proto.Type) {
    let screen = blessed.screen();
    let grid = new bcontrib.grid({
        rows: 10
        , cols: 10
        , screen: screen
    });
    let line = grid.set(0, 0, 10, 10, bcontrib.line, {
        maxY: 100
        , showNthLabel: 20
        , label: 'Input Data'
        , showLegend: true
        , legend: {width: 20}
    });
    var lineData = {
        title: 'Value'
        , style: {line: 'red'}
        , x: []
        , y: []
    };
    line.setData([lineData]);
    
    screen.key(['escape', 'q', 'C-c'], function(ch, key) {
        return process.exit(0);
    });
    screen.on('resize', function() {
        line.emit('attach');
    });
    screen.render();

    function secureDataHook(data: Buffer) : Buffer {
        let ret = Buffer.alloc(data.byteLength-sodium.crypto_secretbox_NONCEBYTES-sodium.crypto_secretbox_MACBYTES);
        if (sodium.crypto_secretbox_open_easy(
            ret
            , data.slice(sodium.crypto_secretbox_NONCEBYTES)
            , data.slice(0, sodium.crypto_secretbox_NONCEBYTES)
            , decryptKey
        )) {
            return ret;
        } else {
            return null;
        }
    }

    type E = TMBasic.ClockEnv;
    interface InputData {
        value : number;
    }
    
    let heartbeatImporter = TMTransport.RemoteComponents.createTypedImporter<E,TMTransport.RemoteComponents.Heartbeat>(
        (d : Buffer) => cbor.decode(d) as TMTransport.RemoteComponents.Heartbeat
        , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
        , (secure?
            "simple_demo.secure_executables.data_source.heartbeat"
            :"simple_demo.plain_executables.data_source.heartbeat")
    );
    let dataImporter = new TMTransport.RemoteComponents.DynamicTypedImporter<E,InputData>(
        (d : Buffer) => (parser.decode(d) as unknown) as InputData
        , null
        , null
        , (secure?secureDataHook:undefined)
    );
    let heartbeatHandler = TMInfra.RealTimeApp.Utils.pureExporter<E,TMBasic.TypedDataWithTopic<TMTransport.RemoteComponents.Heartbeat>>(
        (x : TMBasic.TypedDataWithTopic<TMTransport.RemoteComponents.Heartbeat>) => {
            if (x.content.sender_description == (secure?"simple_demo secure DataSource":"simple_demo DataSource")) {
                dataImporter.addSubscription(x.content.broadcast_channels["input data publisher"][0], "input.data");
            }
        }
    );
    let dataHandler = TMInfra.RealTimeApp.Utils.pureExporter<E,TMBasic.TypedDataWithTopic<InputData>>(
        (x : TMBasic.TypedDataWithTopic<InputData>) => {
            let v = x.content.value;
            lineData.y.push(v);
            if (lineData.y.length > 100) {
                lineData.y.shift();
            } else {
                lineData.x.push(""+lineData.x.length);
            }
            lineData.title = ('Value: '+printf("%.6lf", v));
            line.setData([lineData]);
            screen.render();
        }
    );

    let r = new TMInfra.RealTimeApp.Runner<E>(new TMBasic.ClockEnv());
    r.exportItem(heartbeatHandler, r.importItem(heartbeatImporter));
    r.exportItem(dataHandler, r.importItem(dataImporter));
    r.finalize();
}
