import {MultiTransportFacilityClient, FacilityOutput} from '../../../tm_transport/node_lib/TMTransport'
import * as Stream from 'stream'
import {load, Type} from "protobufjs"
import {eddsa as EDDSA} from "elliptic"
import * as cbor from 'cbor'

const signature_key_bytes = Buffer.from([
    0x89,0xD9,0xE6,0xED,0x17,0xD6,0x7B,0x30,0xE6,0x16,0xAC,0xB4,0xE6,0xD0,0xAD,0x47,
    0xD5,0x0C,0x6E,0x5F,0x11,0xDF,0xB1,0x9F,0xFE,0x4D,0x23,0x2A,0x0D,0x45,0x84,0x8E
]);

let modeStr = process.argv[2];
enum Mode {
    Plain,
    Sign
};
let mode : Mode = Mode.Plain;
switch (modeStr) {
    case "plain":
        mode = Mode.Plain;
        break;
    case "sign":
        mode = Mode.Sign;
        break;
    default:
        console.log("Mode must be plain or sign");
        process.exit(0);
}
let cmd = process.argv[3];

let attacher : (Buffer) => Buffer = null;

switch (mode) {
    case Mode.Sign:
        let signature_key = new EDDSA("ed25519").keyFromSecret(signature_key_bytes);
        attacher = function(data : Buffer) {
            let signature = signature_key.sign(data);
            return cbor.encode({"signature" : Buffer.from(signature.toBytes()), "data" : data});
        }
        break;
    case Mode.Plain:
        attacher = function(data : Buffer) {
            return cbor.encode(["simple_demo_client.ts", data]);
        }
        break;
    default:
        break;
}

function printStream(t : Type) : Stream.Writable {
    return new Stream.Writable({
        write : function(chunk : FacilityOutput, _encoding, callback) {
            let parsed = t.decode(chunk.output);
            console.log({isFinal : chunk.isFinal, data : parsed});
            if (chunk.isFinal) {
                setTimeout(function() {
                    process.exit(0);
                }, 500);
            }
        }
        , objectMode : true
    });
}

async function runConfigure(args : Array<string>) {
    let enabled : boolean;
    switch (args[0]) {
    case 'enable':
        enabled = true;
        break;
    case 'disable':
        enabled = false;
        break;
    default:
        console.log('Usage ... configure enable|disable');
        process.exit();
    }
    let root = await load('../proto/defs.proto');
    let inputT = root.lookupType('simple_demo.ConfigureCommand');
    let outputT = root.lookupType('simple_demo.ConfigureResult');
    let streams = await MultiTransportFacilityClient.facilityStream({
        address : 'rabbitmq://127.0.0.1::guest:guest:test_config_queue'
        , identityAttacher : attacher
    });
    let keyify = MultiTransportFacilityClient.keyify();
    keyify.pipe(streams[0]);
    streams[1].pipe(printStream(outputT));
    keyify.write(inputT.encode({enabled : enabled}).finish());
}
async function runQuery(args: Array<string>) {
    let root = await load('../proto/defs.proto');
    let inputT = root.lookupType('simple_demo.OutstandingCommandsQuery');
    let outputT = root.lookupType('simple_demo.OutstandingCommandsResult');
    let streams = await MultiTransportFacilityClient.facilityStream({
        address : 'rabbitmq://127.0.0.1::guest:guest:test_query_queue'
    });
    let keyify = MultiTransportFacilityClient.keyify();
    keyify.pipe(streams[0]);
    streams[1].pipe(printStream(outputT));
    keyify.write(inputT.encode({}).finish());
}
async function runClear(args: Array<string>) {
    const ids = args.map((x) => parseInt(x));
    let root = await load('../proto/defs.proto');
    let inputT = root.lookupType('simple_demo.ClearCommands');
    let outputT = root.lookupType('simple_demo.ClearCommandsResult');
    let streams = await MultiTransportFacilityClient.facilityStream({
        address : 'rabbitmq://127.0.0.1::guest:guest:test_clear_queue'
        , identityAttacher : attacher
    });
    let keyify = MultiTransportFacilityClient.keyify();
    keyify.pipe(streams[0]);
    streams[1].pipe(printStream(outputT));
    keyify.write(inputT.encode({ids: ids}).finish());
}

async function runClient(args : Array<string>) {
    switch (cmd) {
    case 'configure':
        await runConfigure(args);
        break;
    case 'query':
        await runQuery(args);
        break;
    case 'clear':
        await runClear(args);
        break;
    default:
        break;
    }
}

(async () => {
    await runClient(process.argv.slice(4));
})();