import {MultiTransportListener, MultiTransportPublisher} from '../../../tm_transport/node_lib/TMTransport'
import * as yargs from 'yargs'
import * as dateFormat from 'dateformat'
import * as Stream from 'stream'

yargs
    .scriptName("relayer")
    .usage("$0 <options>")
    .option('--incomingAddress', {
        describe: 'PROTOCOL://LOCATOR'
        , type: 'string'
        , nargs: 1
        , demand: true
    })
    .option('--outgoingAddress', {
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

let incomingAddress = yargs.argv.incomingAddress as string;
let outgoingAddress = yargs.argv.outgoingAddress as string;
let summaryPeriod = 0;
if (yargs.argv.summaryPeriod !== undefined) {
    summaryPeriod = parseInt(yargs.argv.summaryPeriod as string);
}

let dateFormatStr = "yyyy-mm-dd HH:MM:ss.l";
let count = 0;

(async () => {
    let outgoingStream = await MultiTransportPublisher.outputStream(outgoingAddress);
    let incomingStream = MultiTransportListener.inputStream(incomingAddress);
    incomingStream.pipe(outgoingStream);
    incomingStream.pipe(new Stream.Writable({
        write : function(_chunk, _encoding, callback) {
            ++count;
            callback();
        },
        objectMode : true
    }));
})();

if (summaryPeriod !== 0) {
    setInterval(function() {
        console.log(`${dateFormat(new Date(), dateFormatStr)}: Relayed ${count} messages so far`);
    }, summaryPeriod*1000);
}