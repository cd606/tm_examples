import {MultiTransportListener, MultiTransportPublisher} from '../../../tm_transport/node_lib/TMTransport'
import * as yargs from 'yargs'
import * as dateFormat from 'dateformat'
import * as Stream from 'stream'
import * as cbor from 'cbor'
import * as microtime from 'microtime'

yargs
    .scriptName("delay_measurer")
    .usage("$0 <options>")
    .option('--mode', {
        describe: 'sender or receiver'
        , type: 'string'
        , nargs: 1
        , demand: true
    })
    .option('--interval', {
        describe: 'interval for publishing in milliseconds'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: 0
    })
    .option('--bytes', {
        describe: 'byte count for publishing'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: 0
    })
    .option('--address', {
        describe: 'PROTOCOL://LOCATOR'
        , type: 'string'
        , nargs: 1
        , demand: true
    })
    .option('--summaryPeriod', {
        describe: 'summary period in seconds'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: 0
    })
    ;

enum Mode {
    Sender 
    , Receiver
}

let mode : Mode;
let modeStr = yargs.argv.mode as string;
if (modeStr == 'sender') {
    mode = Mode.Sender;
} else if (modeStr == 'receiver') {
    mode = Mode.Receiver;
} else {
    console.error(`Unknown mode string '${mode}', must be sender or receiver`);
    process.exit(0);
}
let address = yargs.argv.address as string;
let interval = Math.trunc(yargs.argv.interval as number);
let bytes = Math.trunc(yargs.argv.bytes as number);
let summaryPeriod = Math.trunc(yargs.argv.summaryPeriod as number);
if (mode == Mode.Sender) {
    if (interval <= 0) {
        console.error('sender interval cannot be 0');
        process.exit(0);
    }
} else if (mode == Mode.Receiver) {
    if (summaryPeriod <= 0) {
        console.error('receiver summary period cannot be 0');
        process.exit(0);
    }
}

async function runSender(address : string, interval : number, bytes : number, summaryPeriod : number) {
    let dateFormatStr = "yyyy-mm-dd HH:MM:ss.l";
    let counter = 0;
    let sourceStream = new Stream.Readable({
        read : function(_size : number) {}
        , objectMode : true
    });
    let publisher = await MultiTransportPublisher.outputStream(address);
    sourceStream.pipe(publisher);
    let dataToSend = Buffer.alloc(bytes, ' ');
    setInterval(function() {
        let data = [
            ++counter
            , microtime.now()
            , dataToSend
        ];
        sourceStream.push(['test.data', cbor.encode(data)]);
    }, interval);
    if (summaryPeriod > 0) {
        setInterval(function() {
            console.log(`${dateFormat(new Date(), dateFormatStr)}: Sent ${counter} messages`);
        }, summaryPeriod*1000);
    }
}

async function runReceiver(address : string, summaryPeriod : number) {
    let dateFormatStr = "yyyy-mm-dd HH:MM:ss.l";
    let incomingStream = MultiTransportListener.inputStream(address, 'test.data');
    let stats = {
        count : 0
        , totalDelay : 0.0
        , totalDelaySq : 0.0
        , minID : 0
        , maxID : 0
        , minDelay : -1000000
        , maxDelay : 0
    }
    let statCalc = new Stream.Writable({
        write : function(chunk : [string, Buffer], _encoding, callback) {
            let [_topic, data] = chunk;
            let parsed = cbor.decodeFirstSync(data);
            let now = microtime.now();
            let delay = now-parsed[1];
            //console.log(`parsed=${parsed},now=${now}`);
            let id = parsed[0];
            ++stats.count;
            stats.totalDelay += 1.0*delay;
            stats.totalDelaySq += 1.0*delay*delay;
            if (stats.minID == 0 || stats.minID > id) {
                stats.minID = id;
            }
            if (stats.maxID < id) {
                stats.maxID = id;
            }
            if (stats.minDelay > delay || stats.minDelay < -100000) {
                stats.minDelay = delay;
            }
            if (stats.maxDelay < delay) {
                stats.maxDelay = delay;
            }
            callback();
        }
        , objectMode : true 
    });
    incomingStream.pipe(statCalc);
    if (summaryPeriod > 0) {
        setInterval(function() {
            let mean = 0.0;
            let sd = 0.0;
            let missed = 0;
            if (stats.count > 0) {
                mean = stats.totalDelay/stats.count;
                missed = stats.maxID-stats.minID+1-stats.count;
            }
            if (stats.count > 1) {
                sd = Math.sqrt((stats.totalDelaySq-mean*mean*stats.count)/(stats.count-1));
            }
            console.log(`${dateFormat(new Date(), dateFormatStr)}: Got ${stats.count} messages, mean delay ${mean} micros, std delay ${sd} micros, missed ${missed} messages, min delay ${stats.minDelay} micros, max delay ${stats.maxDelay} micros`);
        }, summaryPeriod*1000);
    }
}

if (mode == Mode.Sender) {
    runSender(address, interval, bytes, summaryPeriod);
} else {
    runReceiver(address, summaryPeriod);
}