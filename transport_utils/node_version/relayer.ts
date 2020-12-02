import * as TMInfra from '../../../tm_infra/node_lib/TMInfra'
import * as TMBasic from '../../../tm_basic/node_lib/TMBasic'
import * as TMTransport from '../../../tm_transport/node_lib/TMTransport'
import * as yargs from 'yargs'

yargs
    .scriptName("relayer")
    .usage("$0 <options>")
    .option('incomingAddress', {
        describe: 'PROTOCOL://LOCATOR'
        , type: 'string'
        , nargs: 1
        , demand: true
    })
    .option('outgoingAddress', {
        describe: 'PROTOCOL://LOCATOR'
        , type: 'string'
        , nargs: 1
        , demand: true
    })
    .option('summaryPeriod', {
        describe: 'summary period in seconds'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: 0
    })
    ;

let incomingAddress = yargs.argv.incomingAddress as string;
let outgoingAddress = yargs.argv.outgoingAddress as string;
let summaryPeriod = 0;
if (yargs.argv.summaryPeriod !== undefined) {
    summaryPeriod = parseInt(yargs.argv.summaryPeriod as string);
}

let count = 0;

let importer = TMTransport.RemoteComponents.createImporter<TMBasic.ClockEnv>(incomingAddress);
let exporter = TMTransport.RemoteComponents.createExporter<TMBasic.ClockEnv>(outgoingAddress);
let r = new TMInfra.RealTimeApp.Runner<TMBasic.ClockEnv>(new TMBasic.ClockEnv());
let src = r.importItem(importer);
r.exportItem(exporter, src);
if (summaryPeriod != 0) {
    let now = r.environment().now();
    let timerImporter = TMBasic.ClockImporter.createRecurringConstClockImporter<TMBasic.ClockEnv,number>(
        now
        , new Date(now.getTime()+24*3600*1000)
        , summaryPeriod*1000
        , 0
    );
    let summaryExporter = TMInfra.RealTimeApp.Utils.pureExporter<TMBasic.ClockEnv,number>(
        (_x : number) => {
            r.environment().log(TMInfra.LogLevel.Info, `Received ${count} messages so far`);
        }
    );
   r.exportItem(summaryExporter, r.importItem(timerImporter));
}
r.finalize();