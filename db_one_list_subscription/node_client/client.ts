import * as amqp from 'amqplib'
import {v4 as uuidv4} from "uuid"
import * as cbor from 'cbor'
import * as yargs from 'yargs'
import * as util from 'util'

interface Connection {
    channel : amqp.Channel
    , queue : string
};

async function start() : Promise<Connection> {
    const url = 'amqp://guest:guest@127.0.0.1';
    let connection = await amqp.connect(url);
    let channel = await connection.createChannel();
    let replyQueue = await channel.assertQueue('', {exclusive: true});
    channel.consume(replyQueue.queue, function(msg) {
        let isFinal = false;
        let res = undefined;
        if (msg.properties.contentEncoding == 'with_final') {
            if (msg.content.byteLength > 0) {
                res = cbor.decodeAllSync(msg.content.slice(0, msg.content.byteLength-1));
                isFinal = (msg.content[msg.content.byteLength-1] != 0);
            } else {
                isFinal = false;
            }
        } else {
            res = cbor.decodeAllSync(msg.content);
            isFinal = false;
        }
        if (res !== undefined) {
            console.log(`Got update: ${util.inspect(res, {showHidden: false, depth: null})} (ID: ${msg.properties.correlationId}) (isFinal: ${isFinal})`);
        }
        if (isFinal) {
            setTimeout(function() {
                connection.close();
                process.exit(0);
            }, 500);
        }
    }, {noAck : true});
    return {channel: channel, queue: replyQueue.queue};
}

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

function sendCommand(queue : string, conn : Connection, cmd : Buffer) {
    conn.channel.sendToQueue(
        queue
        , cbor.encode(["node_client", cmd])
        , {
            correlationId : uuidv4()
            , contentEncoding : 'with_final'
            , replyTo: conn.queue
            , deliveryMode: 1
            , expiration: '5000'
        }
    );
}

async function run(args : Args) {
    let conn = await start();
    switch (args.command) {
        case Command.Subscribe:
            sendCommand("test_db_one_list_cmd_subscription_queue", conn, cbor.encode([
                0, {keys: [0]}
            ]));
            break;
        case Command.Update:
            sendCommand("test_db_one_list_cmd_transaction_queue", conn, cbor.encode([
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
            ]));
            break;
        case Command.Delete:
            sendCommand("test_db_one_list_cmd_transaction_queue", conn, cbor.encode([
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
            ]));
            break;
        case Command.Unsubscribe:
            if (args.id != "") {
                sendCommand("test_db_one_list_cmd_subscription_queue", conn, cbor.encode([
                    1
                    , {
                        original_subscription_id: args.id
                    }
                ]));
            } else {
                sendCommand("test_db_one_list_cmd_subscription_queue", conn, cbor.encode([
                    3
                    , 0
                ]));
            }
            break;
        case Command.List:
            sendCommand("test_db_one_list_cmd_subscription_queue", conn, cbor.encode([
                2
                , 0
            ]));
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