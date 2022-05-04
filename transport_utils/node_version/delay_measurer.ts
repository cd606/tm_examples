import * as TMInfra from '../../../tm_infra/node_lib/TMInfra'
import * as TMBasic from '../../../tm_basic/node_lib/TMBasic'
import * as TMTransport from '../../../tm_transport/node_lib/TMTransport'
import * as yargs from 'yargs'
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
let modeStr = yargs.argv['mode'] as string;
if (modeStr == 'sender') {
    mode = Mode.Sender;
} else if (modeStr == 'receiver') {
    mode = Mode.Receiver;
} else {
    console.error(`Unknown mode string '${mode}', must be sender or receiver`);
    process.exit(0);
}
let address = yargs.argv['address'] as string;
let interval = Math.trunc(yargs.argv['interval'] as number);
let bytes = Math.trunc(yargs.argv['bytes'] as number);
let summaryPeriod = Math.trunc(yargs.argv['summaryPeriod'] as number);
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
    type E = TMBasic.ClockEnv;
    let env = new TMBasic.ClockEnv();
    let publisher = TMTransport.RemoteComponents.createTypedExporter<E,[number,number,Buffer]>(
        (x : [number,number,Buffer]) => Buffer.from(cbor.encode(x))
        , address
    );
    let counter = 0;
    let dataToSend = Buffer.alloc(bytes, ' ');
    let source = TMBasic.ClockImporter.createRecurringClockImporter<E,TMBasic.TypedDataWithTopic<[number,number,Buffer]>>(
        env.now()
        , new Date(env.now().getTime()+24*3600*1000)
        , interval
        , function (_d : Date) {
            return {
                topic : 'test.data'
                , content : [
                    ++counter
                    , microtime.now()
                    , dataToSend
                ]
            };
        } 
    );

    let r = new TMInfra.RealTimeApp.Runner<E>(env);
    r.exportItem(publisher, r.importItem(source));
    if (summaryPeriod > 0) {
        let summarySource = TMBasic.ClockImporter.createRecurringClockImporter<E,string>(
            env.now()
            , new Date(env.now().getTime()+24*3600*1000)
            , summaryPeriod*1000
            , function (_d : Date) {
                return `Sent ${counter} messages`;
            } 
        );
        let summaryExporter = TMInfra.RealTimeApp.Utils.pureExporter<E,string>(
            (x : string) => {
                env.log(TMInfra.LogLevel.Info, x);
            }
        );
        r.exportItem(summaryExporter, r.importItem(summarySource));
    }
    r.finalize();
}

async function runReceiver(address : string, summaryPeriod : number) {
    type E = TMBasic.ClockEnv;
    let env = new TMBasic.ClockEnv();
    let source = TMTransport.RemoteComponents.createTypedImporter<E,[number,number,Buffer]>(
        (d : Buffer) => cbor.decode(d)
        , address
        , 'test.data'
    );
    let stats = {
        count : 0
        , totalDelay : 0.0
        , totalDelaySq : 0.0
        , minID : 0
        , maxID : 0
        , minDelay : -1000000
        , maxDelay : 0
    }
    let statCalc = TMInfra.RealTimeApp.Utils.pureExporter<E,TMBasic.TypedDataWithTopic<[number,number,Buffer]>>(
        (x : TMBasic.TypedDataWithTopic<[number,number,Buffer]>) => {
            let now = microtime.now();
            let delay = now-x.content[1];
            let id = x.content[0];
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
        }
    );
    let r = new TMInfra.RealTimeApp.Runner<E>(env);
    r.exportItem(statCalc, r.importItem(source));

    if (summaryPeriod > 0) {
        let summarySource = TMBasic.ClockImporter.createRecurringConstClockImporter<E,TMBasic.VoidStruct>(
            env.now()
            , new Date(env.now().getTime()+24*3600*1000)
            , summaryPeriod*1000
            , 0 
        );
        let summaryExporter = TMInfra.RealTimeApp.Utils.pureExporter<E,TMBasic.VoidStruct>(
            (_x : TMBasic.VoidStruct) => {
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
                env.log(TMInfra.LogLevel.Info, `Got ${stats.count} messages, mean delay ${mean} micros, std delay ${sd} micros, missed ${missed} messages, min delay ${stats.minDelay} micros, max delay ${stats.maxDelay} micros`);
            }
        );
        r.exportItem(summaryExporter, r.importItem(summarySource));
    }
    r.finalize();
}

if (mode == Mode.Sender) {
    runSender(address, interval, bytes, summaryPeriod);
} else {
    runReceiver(address, summaryPeriod);
}
