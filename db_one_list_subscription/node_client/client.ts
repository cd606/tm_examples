import * as cbor from 'cbor'
import * as yargs from 'yargs'
import * as util from 'util'
import {MultiTransportFacilityClient, FacilityOutput} from '../../../tm_transport/node_lib/TMTransport'
import * as Stream from 'stream'

enum Command {
    Subscribe
    , Update 
    , Delete 
    , Unsubscribe
    , List
    , Snapshot
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
    stream.write(cbor.encode(cmd));
}

function attachMyIdentity(data : Buffer) : Buffer {
    return Buffer.from(cbor.encode(["node_client", data]));
}

async function run(args : Args) {
    let subscriptionStream = await MultiTransportFacilityClient.facilityStream({
        address : "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_subscription_queue"
        , identityAttacher : attachMyIdentity
    });
    let transactionStream = await MultiTransportFacilityClient.facilityStream({
        address : "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_transaction_queue"
        , identityAttacher : attachMyIdentity
    });
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
            keyify.pipe(subscriptionStream[0]);
            subscriptionStream[1].pipe(resultHandlingStream);
            sendCommand(keyify, [
                0, {keys: [0]}
            ]);
            break;
        case Command.Update:
            keyify.pipe(transactionStream[0]);
            transactionStream[1].pipe(resultHandlingStream);
            sendCommand(keyify, [
                1
                , {
                    key : 0
                    , oldVersionSlice : [args.old_version]
                    , oldDataSummary : [args.old_count]
                    , dataDelta : {
                        deletes: {keys: []}
                        , inserts_updates : {
                            items: [{
                                key : [args.name]
                                , data : [args.amount, args.stat]
                            }]
                        }
                    }
                }
            ]);
            break;
        case Command.Delete:
            keyify.pipe(transactionStream[0]);
            transactionStream[1].pipe(resultHandlingStream);
            sendCommand(keyify, [
                1
                , {
                    key : 0
                    , oldVersionSlice : [args.old_version]
                    , oldDataSummary : [args.old_count]
                    , dataDelta : {
                        deletes: {keys: [[args.name]]}
                        , inserts_updates : {
                            items: []
                        }
                    }
                }
            ]);
            break;
        case Command.Unsubscribe:
            keyify.pipe(subscriptionStream[0]);
            subscriptionStream[1].pipe(resultHandlingStream);
            if (args.id != "") {
                sendCommand(keyify, [
                    1
                    , {
                        originalSubscriptionID: args.id
                    }
                ]);
            } else {
                sendCommand(keyify, [
                    3
                    , {}
                ]);
            }
            break;
        case Command.List:
            keyify.pipe(subscriptionStream[0]);
            subscriptionStream[1].pipe(resultHandlingStream);
            sendCommand(keyify, [
                2
                , {}
            ]);
            break;
        case Command.Snapshot:
            keyify.pipe(subscriptionStream[0]);
            subscriptionStream[1].pipe(resultHandlingStream);
            sendCommand(keyify, [
                4, {keys: [0]}
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
        case 'snapshot':
            cmd = Command.Snapshot;
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