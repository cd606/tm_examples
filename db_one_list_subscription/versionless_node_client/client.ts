import * as cbor from 'cbor'
import * as yargs from 'yargs'
import * as util from 'util'
import {MultiTransportFacilityClient, FacilityOutput, RemoteComponents} from '../../../tm_transport/node_lib/TMTransport'
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
    let heartbeat = await RemoteComponents.fetchTypedFirstUpdateAndDisconnect<RemoteComponents.Heartbeat>(
        (d) => cbor.decode(d) as RemoteComponents.Heartbeat
        , "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
        , "versionless_db_one_list_subscription_server.heartbeat"
        , (data) => /versionless_db_one_list_subscription_server/.test(data.sender_description)
    );
    let subscriptionStream = await MultiTransportFacilityClient.facilityStream({
        address : heartbeat.content.facility_channels["transaction_server_components/subscription_handler"]
        , identityAttacher : attachMyIdentity
    });
    let transactionStream = await MultiTransportFacilityClient.facilityStream({
        address : heartbeat.content.facility_channels["transaction_server_components/transaction_handler"]
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
                    , oldVersionSlice : []
                    , oldDataSummary : [args.old_count]
                    , dataDelta : {
                        deletes: []
                        , inserts_updates : [[
                            {'name': args.name}
                            , {'amount': args.amount, 'stat': args.stat}
                        ]]
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
                    , oldVersionSlice : []
                    , oldDataSummary : [args.old_count]
                    , dataDelta : {
                        deletes: [{'name': args.name}]
                        , inserts_updates : []
                    }
                }
            ]);
            break;
        case Command.Unsubscribe:
            keyify.pipe(subscriptionStream[0]);
            subscriptionStream[1].pipe(resultHandlingStream);
            if (args.id != "" && args.id != "all") {
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

yargs
    .scriptName("client")
    .usage("$0 <options>")
    .option('command', {
        describe: 'subscribe|update|delete|unsubscribe|list|snapshot'
        , type: 'string'
        , nargs: 1
        , demand: true
    })
    .option('name', {
        describe: 'name in db entry'
        , type: 'string'
        , nargs: 1
        , demand: false
        , default: null
    })
    .option('amount', {
        describe: 'amount in db entry'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: null
    })
    .option('stat', {
        describe: 'stat in db entry'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: null
    })
    .option('old_count', {
        describe: 'old db item count'
        , type: 'number'
        , nargs: 1
        , demand: false
        , default: null
    })
    .option('id', {
        describe: 'id to unsubscribe'
        , type: 'string'
        , nargs: 1
        , demand: false
        , default: null
    })
    ;

(async () => {
    let cmd : Command = Command.Unknown;
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
        , name : yargs.argv["name"] as string
        , amount : yargs.argv["amount"] as number
        , stat : yargs.argv["stat"] as number
        , old_count : yargs.argv["old_count"] as number
        , id : yargs.argv["id"] as string
    }
    await run(args);
})();