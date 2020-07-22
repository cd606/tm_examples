import * as yargs from 'yargs'
import * as cbor from 'cbor'
import * as util from 'util'
import * as proto from 'protobufjs'
import * as dateFormat from 'dateformat'
import * as fs from 'fs'

const printf = require('printf'); //this module can only be loaded this way in TypeScript

yargs
    .scriptName("listener")
    .usage("$0 <options>")
    .option('--file', {
        describe: 'FILENAME'
        , type: 'string'
        , nargs: 1
        , demand: true
    })
    .option('--printMode', {
        describe: 'length|string|cbor|none|bytes|protobuf:FILE_NAME:TYPE_NAME'
        , type: 'string'
        , nargs: 1
        , demand: false
        , default: "length"
    })
    .option('--fileMagicLength', {
        describe: 'file magic length'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: 4
    })
    .option('--recordMagicLength', {
        describe: 'record magic length'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: 4
    })
    .option('--timeFieldLength', {
        describe: 'topic length field length'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: 8
    })
    .option('--topicLengthFieldLength', {
        describe: 'topic length field length'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: 4
    })
    .option('--dataLengthFieldLength', {
        describe: 'data field length'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: 4
    })
    .option('--hasFinalFlag', {
        describe: 'whether file has final flag'
        , type: 'boolean'
        , nargs: 1
        , demand: false
        , default: true
    })
    .option('--timeUnit', {
        describe: 'second|millisecond|microsecond'
        , type: 'string'
        , nargs: 1
        , demand: false
        , default: 'microsecond'
    })
    ;

interface CaptureFileItem {
    time : string;
    topic : string;
    data : Buffer;
    isFinal : boolean;
}

let timeFormatter = function(t : bigint) : string {
    return t.toString();
}
switch (yargs.argv.timeUnit as string) {
    case 'second':
        timeFormatter = function(t : bigint) : string {
            let d = new Date(Number(t*BigInt(1000)));
            return dateFormat(d, "yyyy-mm-dd HH:MM:ss");
        }
        break;
    case 'millisecond':
        timeFormatter = function(t : bigint) : string {
            let d = new Date(Number(t));
            return dateFormat(d, "yyyy-mm-dd HH:MM:ss.l");
        }
        break;
    case 'microsecond':
        timeFormatter = function(t : bigint) : string {
            let d = new Date(Number(t/BigInt(1000)));
            return dateFormat(d, "yyyy-mm-dd HH:MM:ss")+printf('.%06d', Number(t%BigInt(1000000)));
        }
        break;
    default:
        break;
}

function * readCaptureFile(fd : number) : Generator<CaptureFileItem, void, unknown> {
    let buffer : Buffer = Buffer.alloc(8);
    let fileMagicLen = yargs.argv.fileMagicLength as number;
    if (fileMagicLen > 0) {
        if (fs.readSync(fd, buffer, 0, fileMagicLen, null) != fileMagicLen) {
            return;
        } //skip the file header
    }
    let recordMagicLen = yargs.argv.recordMagicLength as number;
    let timeFieldLen = yargs.argv.timeFieldLength as number;
    let topicLengthFieldLen = yargs.argv.topicLengthFieldLength as number;
    let dataLengthFieldLen = yargs.argv.dataLengthFieldLength as number;
    let hasFinalFlag = yargs.argv.hasFinalFlag as boolean;
    while (true) {
        if (recordMagicLen > 0) {
            if (fs.readSync(fd, buffer, 0, recordMagicLen, null) != recordMagicLen) {
                return;
            } //skip the record header
        }
        if (fs.readSync(fd, buffer, 0, timeFieldLen, null) != timeFieldLen) {
            return;
        }
        let t : bigint = buffer.readBigInt64LE();
        let formattedTime = timeFormatter(t);
        if (fs.readSync(fd, buffer, 0, topicLengthFieldLen, null) != topicLengthFieldLen) {
            return;
        }
        let topicLen = buffer.readUInt32LE();
        let topicBuf = Buffer.alloc(topicLen);
        if (fs.readSync(fd, topicBuf, 0, topicLen, null) != topicLen) {
            return;
        }
        let topic = topicBuf.toString('utf-8');
        if (fs.readSync(fd, buffer, 0, dataLengthFieldLen, null) != dataLengthFieldLen) {
            return;
        }
        let dataLen = buffer.readUInt32LE();
        let data = Buffer.alloc(dataLen);
        if (fs.readSync(fd, data, 0, dataLen, null) != dataLen) {
            return;
        }
        let isFinal = false;
        if (hasFinalFlag) {
            if (fs.readSync(fd, buffer, 0, 1, null) != 1) {
                return;
            }
            isFinal = (buffer[0] != 0);
        }
        yield {
            time : formattedTime
            , topic : topic
            , data : data
            , isFinal : isFinal
        };
    }
}

function mainLoop(p : (time : string, topic : string, data : Buffer, isFinal : boolean) => void) {
    for (let item of readCaptureFile(fs.openSync(yargs.argv.file as string, 'r'))) {
        p(item.time, item.topic, item.data, item.isFinal);
    }
}

let printMode = "length";
if (yargs.argv.printMode !== undefined) {
    printMode = yargs.argv.printMode as string;
}
let printer = function(_t : string, _topic : string, _data : Buffer, _isFinal : boolean) {
}
switch (printMode) {
    case "length":
        printer = function(t : string, topic : string, data : Buffer, isFinal : boolean) {
            console.log(`${t}: topic '${topic}': ${data.length} bytes (isFinal : ${isFinal})`);
        }
        mainLoop(printer);
        break;
    case "string":
        printer = function(t : string, topic : string, data : Buffer, isFinal : boolean) {
            console.log(`${t}: topic '${topic}': '${data}' (isFinal : ${isFinal})`);
        }
        mainLoop(printer);
        break;
    case "cbor":
        printer = function(t : string, topic : string, data : Buffer, isFinal : boolean) {
            try {
                let x = cbor.decode(data);
                let xRep = util.inspect(x, {showHidden: false, depth: null, colors: true});
                console.log(`${t}: topic '${topic}': ${xRep} (isFinal : ${isFinal})`);
            } catch (e) {
                console.log(`${t}: topic '${topic}': ${data.length} bytes, not CBOR data (isFinal : ${isFinal})`);
            }
        }
        mainLoop(printer);
        break;
    case "none":
        printer = function(_t : string, _topic : string, _data : Buffer, _isFinal : boolean) {
        }
        mainLoop(printer);
        break;
    case "bytes":
        printer = function(t : string, topic : string, data : Buffer, isFinal : boolean) {
            let dataRep = util.inspect(data, {showHidden: false, depth: null, colors: true});
            console.log(`${t}: topic '${topic}': ${dataRep} (isFinal : ${isFinal})`);
        }
        mainLoop(printer);
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
                printer = function(t : string, topic : string, data : Buffer, isFinal : boolean) {
                    try {
                        let x = parser.decode(data);
                        let xRep = util.inspect(x, {showHidden: false, depth: null, colors: true});
                        console.log(`${t}: topic '${topic}': ${xRep} (isFinal : ${isFinal})`);
                    } catch (e) {
                        console.log(`${t}: topic '${topic}': ${data.length} bytes, not protobuf data (isFinal : ${isFinal})`);
                    }
                }
                mainLoop(printer);
            })
        }
        break;
}


