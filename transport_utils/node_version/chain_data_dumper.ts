import * as TMBasic from '../../../tm_basic/node_lib/TMBasic'
import {SharedChainUtils, EtcdSharedChain, RedisSharedChain} from '../../../tm_transport/node_lib/TMTransport_Chains'
import * as yargs from 'yargs'
import * as cbor from 'cbor'
import * as util from 'util'
import * as proto from 'protobufjs'

yargs
    .scriptName("chain_data_dumper")
    .usage("$0 <options>")
    .option('chain', {
        describe: 'chain locator'
        , type: 'string'
        , nargs: 1
        , demand: true
    })
    .option('captureFile', {
        describe: 'capture file name'
        , type: 'string'
        , nargs: 1
        , demand: false
        , default: ''
    })
    .option('printMode', {
        describe: 'cbor|bytes|protobuf:FILE_NAME:TYPE_NAME'
        , type: 'string'
        , nargs: 1
        , demand: false
        , default: 'bytes'
    });

let chainLocator = yargs.argv.chain as string;
let useFileOutput = (yargs.argv.captureFile != '');

let chainSpec = SharedChainUtils.parseChainLocator(chainLocator);
if (chainSpec === null) {
    console.log(`Unknown chain spec ${chainLocator}`);
    process.exit(0);
}

let decoder = (x) => x;
let encoder = (x) => x;

function start() {
    let dumpString = function (s : any) {
        console.log(util.inspect(s.data, {showHidden: false, depth: null, colors: true}));
    };
    if (useFileOutput) {
        let fileExporter = TMBasic.Files.byteDataWithTopicOutput<TMBasic.ClockEnv>(
            yargs.argv.captureFile as string, Buffer.from([0x01, 0x23, 0x45, 0x67]), Buffer.from([0x76, 0x54, 0x32, 0x10])
        );
        let env = new TMBasic.ClockEnv();
        fileExporter.start(env);
        dumpString = function(s : any) {
            fileExporter.handle({
                environment : env 
                , timedData : {
                    timePoint : env.now()
                    , value : {
                        topic : ""
                        , content : encoder(s.data)
                    }
                    , finalFlag : false
                }
            });
        }
    }
    let chain : EtcdSharedChain | RedisSharedChain;
    if (chainSpec[0] == "etcd") {
        chain = new EtcdSharedChain(chainSpec[1], decoder);
    } else {
        chain = new RedisSharedChain(chainSpec[1], decoder);
    }
    (async () => {
        await chain.start({});
        while (await chain.next()) {
            dumpString(chain.currentValue());
        }
        process.exit(0);
    })();
}

let printMode = yargs.argv.printMode as string;
if (printMode == 'cbor') {
    decoder = (x) => cbor.decode(x);
    encoder = (x) => cbor.encode(x);
    start();
} else if (printMode.startsWith("protobuf:")) {
    let protoSpec = printMode.split(':');
    if (protoSpec.length != 3) {
        console.error("protobuf print mode must be in the format 'protobuf:FILE_NAME:TYPE_NAME'");
        process.exit(1);
    }
    proto.load(protoSpec[1]).then(function(root) {
        let parser = root.lookupType(protoSpec[2]);
        decoder = (x) => parser.decode(x);
        encoder = (x) => parser.encode(x).finish();
        start();
    })
} else {
    start();
}

