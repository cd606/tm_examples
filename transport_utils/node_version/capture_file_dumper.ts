import * as yargs from 'yargs'
import * as cbor from 'cbor'
import * as util from 'util'
import * as proto from 'protobufjs'
import * as fs from 'fs'
import * as TMBasic from '../../../tm_basic/node_lib/TMBasic'

yargs
    .scriptName("capture file dumper")
    .usage("$0 <options>")
    .option('file', {
        describe: 'FILENAME'
        , type: 'string'
        , nargs: 1
        , demand: true
    })
    .option('printMode', {
        describe: 'length|string|cbor|none|bytes|protobuf:FILE_NAME:TYPE_NAME'
        , type: 'string'
        , nargs: 1
        , demand: false
        , default: "length"
    })
    .option('fileMagicLength', {
        describe: 'file magic length'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: 4
    })
    .option('recordMagicLength', {
        describe: 'record magic length'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: 4
    })
    .option('timeFieldLength', {
        describe: 'time field length'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: 8
    })
    .option('topicLengthFieldLength', {
        describe: 'topic length field length'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: 4
    })
    .option('dataLengthFieldLength', {
        describe: 'data field length'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: 4
    })
    .option('hasFinalFlag', {
        describe: 'whether file has final flag'
        , type: 'boolean'
        , nargs: 1
        , demand: false
        , default: true
    })
    .option('timeUnit', {
        describe: 'second|millisecond|microsecond'
        , type: 'string'
        , nargs: 1
        , demand: false
        , default: 'microsecond'
    })
    ;

let transformerOptions : TMBasic.Files.TopicCaptureFileRecordReaderOption = {
    fileMagicLength : yargs.argv.fileMagicLength as number
    , recordMagicLength : yargs.argv.recordMagicLength as number
    , timeFieldLength : yargs.argv.timeFieldLength as number
    , topicFieldLength : yargs.argv.topicLengthFieldLength as number
    , dataLengthFieldLength : yargs.argv.dataLengthFieldLength as number
    , timePrecision : yargs.argv.timeUnit as ("second" | "millisecond" | "microsecond")
    , hasFinalFlagField : yargs.argv.hasFinalFlag as boolean
};

function mainLoop(p : (time : string, topic : string, data : Buffer, isFinal : boolean) => void) {
    let inputStream = fs.createReadStream(yargs.argv.file as string, {encoding : null});
    let transformer = TMBasic.Files.genericRecordStream(
        new TMBasic.Files.TopicCaptureFileRecordReader(
            transformerOptions
        )
    );
    inputStream.pipe(transformer);
    (async () => {
        for await (const item of transformer) {
            let typedItem = item as TMBasic.Files.TopicCaptureFileRecord;
            p(typedItem.timeString, typedItem.topic, typedItem.data, typedItem.isFinal);
        }
    })();
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


