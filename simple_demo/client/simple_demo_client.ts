import {TMHelper} from "./tm_helpers"
import { runInContext } from "vm";

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

let tmHelper = new TMHelper();
switch (mode) {
    case Mode.Sign:
        tmHelper.set_identity(signature_key_bytes);
        break;
    case Mode.Plain:
        tmHelper.set_identity("simple_demo_client.ts");
        break;
    default:
        break;
}

function callback(isFinal : boolean, data : {}) {
    let obj = {isFinal : isFinal, data : data};
    console.log(obj);
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
    await tmHelper.set_incoming_and_outgoing_type(
        '../proto/defs.proto'
        , 'simple_demo.ConfigureResult'
        , 'simple_demo.ConfigureCommand'
    );
    tmHelper.enable_identity();
    await tmHelper.send_request(
        'localhost::guest:guest:test_config_queue'
        , {enabled: enabled}
        , callback
    );
}
async function runQuery(args: Array<string>) {
    await tmHelper.set_incoming_and_outgoing_type(
        '../proto/defs.proto'
        , 'simple_demo.OutstandingCommandsResult'
        , 'simple_demo.OutstandingCommandsQuery'
    );
    tmHelper.disable_identity();
    await tmHelper.send_request(
        'localhost::guest:guest:test_query_queue'
        , {}
        , callback
    );
}
async function runClear(args: Array<string>) {
    const ids = args.map((x) => parseInt(x));
    await tmHelper.set_incoming_and_outgoing_type(
        '../proto/defs.proto'
        , 'simple_demo.ClearCommandsResult'
        , 'simple_demo.ClearCommands'
    );
    tmHelper.enable_identity();
    await tmHelper.send_request(
        'localhost::guest:guest:test_clear_queue'
        , {ids: ids}
        , callback
    );
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