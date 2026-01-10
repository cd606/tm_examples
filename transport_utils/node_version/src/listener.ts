import * as TMInfra from '@dev_cd606/tm_infra'
import * as TMBasic from '@dev_cd606/tm_basic'
import * as TMTransport from '@dev_cd606/tm_transport'
import yargs from 'yargs/yargs'
import { hideBin } from 'yargs/helpers'
import * as cbor from 'cbor'
import * as util from 'util'
import * as proto from 'protobufjs'
import * as dateFNS from 'date-fns'

const argv = yargs(hideBin(process.argv))
    .scriptName("listener")
    .usage("$0 <options>")
    .option('address', {
        describe: 'PROTOCOL://LOCATOR'
        , type: 'string'
        , nargs: 1
        , demand: true
    })
    .option('topic', {
        describe: 'topic to subscribe'
        , type: 'string'
        , nargs: 1
        , demand: false
        , default: null
    })
    .option('printMode', {
        describe: 'length|string|cbor|none|bytes|protobuf:FILE_NAME:TYPE_NAME'
        , type: 'string'
        , nargs: 1
        , demand: false
        , default: "length"
    })
    .option('summaryPeriod', {
        describe: 'summary period in seconds'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: 0
    })
    .option('captureFile', {
        describe: 'capture file name'
        , type: 'string'
        , nargs: 1
        , demand: false
        , default: ''
    })
    .parseSync();

let address = argv['address'] as string;
let topic = argv['topic'] as string;
let printMode = "length";
if (argv['printMode'] !== undefined) {
    printMode = argv['printMode'] as string;
}
let printer = function (_topic: string, _data: Buffer) {
}
let dateFormatStr = "yyyy-MM-dd HH:mm:ss.SSS";
switch (printMode) {
    case "length":
        printer = function (topic: string, data: Buffer) {
            console.log(`${dateFNS.format(new Date(), dateFormatStr)}: topic '${topic}': ${data.length} bytes`);
        }
        break;
    case "string":
        printer = function (topic: string, data: Buffer) {
            console.log(`${dateFNS.format(new Date(), dateFormatStr)}: topic '${topic}': '${data}'`);
        }
        break;
    case "cbor":
        printer = function (topic: string, data: Buffer) {
            try {
                let x = cbor.decode(data);
                let xRep = util.inspect(x, { showHidden: false, depth: null, colors: true });
                console.log(`${dateFNS.format(new Date(), dateFormatStr)}: topic '${topic}': ${xRep}`);
            } catch (e) {
                console.log(`${dateFNS.format(new Date(), dateFormatStr)}: topic '${topic}': ${data.length} bytes, not CBOR data`);
            }
        }
        break;
    case "none":
        printer = function (_topic: string, _data: Buffer) {
        }
        break;
    case "bytes":
        printer = function (topic: string, data: Buffer) {
            let dataRep = util.inspect(data, { showHidden: false, depth: null, colors: true });
            console.log(`${dateFNS.format(new Date(), dateFormatStr)}: topic '${topic}': ${dataRep}`);
        }
        break;
    default:
        if (printMode.startsWith("protobuf:")) {
            let protoSpec = printMode.split(':');
            if (protoSpec.length != 3) {
                console.error("protobuf print mode must be in the format 'protobuf:FILE_NAME:TYPE_NAME'");
                process.exit(1);
            }
            proto.load(protoSpec[1]).then(function (root) {
                let parser = root.lookupType(protoSpec[2]);
                printer = function (topic: string, data: Buffer) {
                    try {
                        let x = parser.decode(data);
                        let xRep = util.inspect(x, { showHidden: false, depth: null, colors: true });
                        console.log(`${dateFNS.format(new Date(), dateFormatStr)}: topic '${topic}': ${xRep}`);
                    } catch (e) {
                        console.log(`${dateFNS.format(new Date(), dateFormatStr)}: topic '${topic}': ${data.length} bytes, not protobuf data`);
                    }
                }
            })
        }
        break;
}
let summaryPeriod = 0;
if (argv['summaryPeriod'] !== undefined) {
    summaryPeriod = parseInt(argv['summaryPeriod'].toString());
}
let captureFile = "";
if (argv['captureFile'] !== undefined) {
    captureFile = argv['captureFile'].toString();
}

let count = 0;
let printExporter = TMInfra.RealTimeApp.Utils.pureExporter<any, TMBasic.ByteDataWithTopic>(
    (d: TMBasic.ByteDataWithTopic) => {
        ++count;
        printer(d.topic, d.content);
    }
);

let env = new TMTransport.TLSClientConfigurationEnv();
let importer = TMTransport.RemoteComponents.createImporter<TMTransport.TLSClientConfigurationEnv>(
    address, topic
);

let r = new TMInfra.RealTimeApp.Runner<TMTransport.TLSClientConfigurationEnv>(env);
let src = r.importItem(importer);

r.exportItem(printExporter, src);

if (captureFile !== "") {
    let fileExporter = TMBasic.Files.byteDataWithTopicOutput<TMTransport.TLSClientConfigurationEnv>(
        captureFile, Buffer.from([0x01, 0x23, 0x45, 0x67]), Buffer.from([0x76, 0x54, 0x32, 0x10])
    );
    r.exportItem(fileExporter, src);
}

if (summaryPeriod !== 0) {
    let now = r.environment().now();
    let timerImporter = TMBasic.ClockImporter.createRecurringConstClockImporter<TMTransport.TLSClientConfigurationEnv, number>(
        now
        , new Date(now.getTime() + 24 * 3600 * 1000)
        , summaryPeriod * 1000
        , 0
    );
    let summaryExporter = TMInfra.RealTimeApp.Utils.pureExporter<TMTransport.TLSClientConfigurationEnv, number>(
        (_x: number) => {
            r.environment().log(TMInfra.LogLevel.Info, `Received ${count} messages so far`);
        }
    );
    r.exportItem(summaryExporter, r.importItem(timerImporter));
}

r.finalize();
