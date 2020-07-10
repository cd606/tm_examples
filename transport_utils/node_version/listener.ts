import {MultiTransportListener} from './MultiTransportListener'
import * as yargs from 'yargs'
import * as cbor from 'cbor'
import * as util from 'util'
import * as proto from 'protobufjs'
import * as dateFormat from 'dateformat'
import * as fs from 'fs'

let address = yargs.argv.address as string;
let topic = yargs.argv.topic as string;
let printMode = "length";
if (yargs.argv.printMode !== undefined) {
    printMode = yargs.argv.printMode as string;
}
let printer = function(_topic : string, _data : Buffer) {
}
let dateFormatStr = "yyyy-mm-dd HH:MM:ss.l";
switch (printMode) {
    case "length":
        printer = function(topic : string, data : Buffer) {
            console.log(`${dateFormat(new Date(), dateFormatStr)}: topic '${topic}': ${data.length} bytes`);
        }
        break;
    case "string":
        printer = function(topic : string, data : Buffer) {
            console.log(`${dateFormat(new Date(), dateFormatStr)}: topic '${topic}': '${data}'`);
        }
        break;
    case "cbor":
        printer = function(topic : string, data : Buffer) {
            try {
                let x = cbor.decodeFirstSync(data);
                let xRep = util.inspect(x, {showHidden: false, depth: null, colors: true});
                console.log(`${dateFormat(new Date(), dateFormatStr)}: topic '${topic}': ${xRep}`);
            } catch (e) {
                console.log(`${dateFormat(new Date(), dateFormatStr)}: topic '${topic}': ${data.length} bytes, not CBOR data`);
            }
        }
        break;
    case "none":
        break;
    case "bytes":
        printer = function(topic : string, data : Buffer) {
            let dataRep = util.inspect(data, {showHidden: false, depth: null, colors: true});
            console.log(`${dateFormat(new Date(), dateFormatStr)}: topic '${topic}': ${dataRep}`);
        }
        break;
    default:
        if (printMode.startsWith("protobuf:")) {
            let protoSpec = printMode.split(':');
            if (protoSpec.length != 3) {
                console.error("protobuf print mode must be in the format 'protobuf:FILE_NAME:TYPE_NAME'");
                process.exit(1);
            }
            proto.load(protoSpec[1]).then(function(root) {
                let parser = root.lookupType(protoSpec[2]);
                printer = function(topic : string, data : Buffer) {
                    try {
                        let x = parser.decode(data);
                        let xRep = util.inspect(x, {showHidden: false, depth: null, colors: true});
                        console.log(`${dateFormat(new Date(), dateFormatStr)}: topic '${topic}': ${xRep}`);
                    } catch (e) {
                        console.log(`${dateFormat(new Date(), dateFormatStr)}: topic '${topic}': ${data.length} bytes, not protobuf data`);
                    }
                }
            })
        }
        break;
}
let summaryPeriod = 0;
if (yargs.argv.summaryPeriod !== undefined) {
    summaryPeriod = parseInt(yargs.argv.summaryPeriod as string);
}
let captureFile = "";
if (yargs.argv.captureFile !== undefined) {
    captureFile = yargs.argv.captureFile as string;
}

let captureFileHandle = null;
if (captureFile !== "") {
    captureFileHandle = fs.openSync(captureFile, 'w');
    fs.writeSync(captureFileHandle, Buffer.from([0x01, 0x23, 0x45, 0x67]));
    fs.fdatasyncSync(captureFileHandle);
}

let count = 0;

let s = MultiTransportListener.inputStream(address, topic);
s.on('data', function(chunk : [string, Buffer]) {
    ++count;
    printer(chunk[0], chunk[1]);
    if (captureFileHandle !== null) {
        let buffer = new Buffer(4+8+4+chunk[0].length+4+chunk[1].length+1);
        buffer[0] = 0x76;
        buffer[1] = 0x54;
        buffer[2] = 0x32;
        buffer[3] = 0x10;
        buffer.writeBigInt64LE(BigInt(new Date().getTime())*BigInt(1000), 4);
        buffer.writeUInt32LE(chunk[0].length, 12);
        buffer.write(chunk[0], 16);
        buffer.writeUInt32LE(chunk[1].length, 16+chunk[0].length);
        chunk[1].copy(buffer, 20+chunk[0].length);
        buffer[buffer.length-1] = 0x0;

        fs.writeFileSync(captureFileHandle, buffer);
        fs.fdatasyncSync(captureFileHandle);
    }
});
s.on('end', function() {
    process.exit(0);
})

if (summaryPeriod !== 0) {
    setInterval(function() {
        console.log(`${dateFormat(new Date(), dateFormatStr)}: Received ${count} messages so far`);
    }, summaryPeriod*1000);
}

