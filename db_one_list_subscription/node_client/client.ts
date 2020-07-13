import * as cbor from 'cbor'
import * as yargs from 'yargs'
import * as util from 'util'
import {MultiTransportFacilityClient, FacilityOutput} from '../../transport_utils/node_version/TMTransport'
import * as Stream from 'stream'

enum Command {
    Subscribe
    , Update 
    , Delete 
    , Unsubscribe
    , List
    , Unknown
};

interface Args {
    command : Command
    , name : string
    , amount : number
    , stat : number
    , old_version: number
    , old_count : number
    , id : string
};

function sendCommand(stream : Stream.Writable, cmd : any) {
    stream.write(cbor.encode(["node_client", cbor.encode(cmd)]));
}

async function run(args : Args) {
    let subscriptionStream = await MultiTransportFacilityClient.facilityStream(
        "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_subscription_queue"
    );
    let transactionStream = await MultiTransportFacilityClient.facilityStream(
        "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_transaction_queue"
    );
    let keyify = MultiTransportFacilityClient.keyify();
    let resultHandlingStream = new Stream.Writable({
        write : function(chunk : FacilityOutput, _encoding, callback) {
            let result = cbor.decodeAllSync(chunk.output);
            console.log(`Got update: ${util.inspect(result, {showHidden: false, depth: null})} (ID: ${chunk.id}) (isFinal: ${chunk.isFinal})`);
            callback();
            if (chunk.isFinal) {
                setTimeout(function() {
                    process.exit(0);
                }, 500);
            }
        }
        , objectMode : true
    });
    switch (args.command) {
        case Command.Subscribe:
            keyify.pipe(subscriptionStream).pipe(resultHandlingStream);
            sendCommand(keyify, [
                0, {keys: [0]}
            ]);
            break;
        case Command.Update:
            keyify.pipe(transactionStream).pipe(resultHandlingStream);
            sendCommand(keyify, [
                1
                , {
                    key: 0
                    , old_version_slice: [args.old_version]
                    , old_data_summary: [args.old_count]
                    , data_delta: {
                        deletes: {keys: []}
                        , inserts_updates: {items: [
                            {
                                key: {name: args.name}
                                , data: {amount: args.amount, stat: args.stat}
                            }
                        ]}
                    }
                }
            ]);
            break;
        case Command.Delete:
            keyify.pipe(transactionStream).pipe(resultHandlingStream);
            sendCommand(keyify, [
                1
                , {
                    key: 0
                    , old_version_slice: [args.old_version]
                    , old_data_summary: [args.old_count]
                    , data_delta: {
                        deletes: {keys: [{name: args.name}]}
                        , inserts_updates: {items: []}
                    }
                }
            ]);
            break;
        case Command.Unsubscribe:
            keyify.pipe(subscriptionStream).pipe(resultHandlingStream);
            if (args.id != "") {
                sendCommand(keyify, [
                    1
                    , {
                        original_subscription_id: args.id
                    }
                ]);
            } else {
                sendCommand(keyify, [
                    3
                    , 0
                ]);
            }
            break;
        case Command.List:
            keyify.pipe(subscriptionStream).pipe(resultHandlingStream);
            sendCommand(keyify, [
                2
                , 0
            ]);
            break;
        default:
            break;
    }
}

(async () => {
    let cmd : Command = Command.Unknown;
    let f = function (x : unknown) : string {
        return (typeof x === 'undefined'?"":(x as string));
    };
    switch (yargs.argv.command) {
        case 'subscribe':
            cmd = Command.Subscribe;
            break;
        case 'update':
            cmd = Command.Update;
            break;
        case 'delete':
            cmd = Command.Delete;
            break;
        case 'unsubscribe':
            cmd = Command.Unsubscribe;
            break;
        case 'list':
            cmd = Command.List;
            break;
        default:
            break;
    }
    if (cmd == Command.Unknown) {
        return;
    }
    let args : Args = {
        command : cmd
        , name : f(yargs.argv["name"])
        , amount : parseInt(f(yargs.argv["amount"]))
        , stat : parseFloat(f(yargs.argv["stat"]))
        , old_version : parseInt(f(yargs.argv["old_version"]))
        , old_count : parseInt(f(yargs.argv["old_count"]))
        , id : f(yargs.argv["id"])
    }
    await run(args);
})();