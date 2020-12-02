import * as TMBasic from '../../../tm_basic/node_lib/TMBasic'
import {SharedChainUtils, EtcdSharedChain, RedisSharedChain} from '../../../tm_transport/node_lib/TMTransport'
import * as yargs from 'yargs'
import * as cbor from 'cbor'
import * as util from 'util'

yargs
    .scriptName("cbor_data_chain_dumper")
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
    });

let chainLocator = yargs.argv.chain as string;
let useFileOutput = (yargs.argv.captureFile != '');
let dumpString = function (s : any) {
    console.log(util.inspect(s, {showHidden: false, depth: null, colors: true}));
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
                    , content : Buffer.from(cbor.encode(s.data))
                }
                , finalFlag : false
            }
        });
    }
}

let chainSpec = SharedChainUtils.parseChainLocator(chainLocator);
if (chainSpec === null) {
    console.log(`Unknown chain spec ${chainLocator}`);
    process.exit(0);
}

let chain : EtcdSharedChain | RedisSharedChain;
if (chainSpec[0] == "etcd") {
    chain = new EtcdSharedChain(chainSpec[1]);
} else {
    chain = new RedisSharedChain(chainSpec[1]);
}
(async () => {
    await chain.start({});
    while (await chain.next()) {
        dumpString(chain.currentValue());
    }
    process.exit(0);
})();