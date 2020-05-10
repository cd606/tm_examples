import {load, Type} from "protobufjs"
import {connect} from "amqplib/callback_api"
import {v4 as uuidv4} from "uuid"
import {eddsa as EDDSA} from "elliptic"

const signature_key_bytes = Buffer.from([
    0x89,0xD9,0xE6,0xED,0x17,0xD6,0x7B,0x30,0xE6,0x16,0xAC,0xB4,0xE6,0xD0,0xAD,0x47,
    0xD5,0x0C,0x6E,0x5F,0x11,0xDF,0xB1,0x9F,0xFE,0x4D,0x23,0x2A,0x0D,0x45,0x84,0x8E
]);
const curve = new EDDSA("ed25519");
const signature_key = curve.keyFromSecret(signature_key_bytes);

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

function attachIdentityToBuffer(input : Uint8Array, mode : Mode) {
    switch (mode) {
        case Mode.Plain:
            {
                let inputBuffer = Buffer.from(input);
                const myIdentity = "ts_client";
                const identityLength = myIdentity.length;
                let b = Buffer.alloc(8);
                b.writeBigUInt64LE(BigInt(identityLength));
                let ret = Buffer.concat([b, Buffer.from(myIdentity, 'ascii'), inputBuffer]);
                return new Uint8Array(ret);
            }
            break;
        case Mode.Sign:
            {
                let inputBuffer = Buffer.from(input);
                let signature = signature_key.sign(inputBuffer);
                let ret = Buffer.concat([inputBuffer,Buffer.from(signature.toBytes())]);
                return new Uint8Array(ret);
            }
            break;
        default:
            return input;
    }
}

function sendBufferAndWaitForReply(queue : string, buffer : Uint8Array, replyType : Type) {
    connect("amqp://guest:guest@localhost", function(error, connection) {
        if (error) {
            throw error;
        }
        connection.createChannel(function (error, channel) {
            if (error) {
                throw error;
            }
            channel.assertQueue('', {exclusive: true}, function (error, q) {
                if (error) {
                    throw error;
                }
                var correlationId = uuidv4();
                channel.consume(q.queue, function(reply) {
                    if (reply.properties.correlationId == correlationId) {
                        let encoding = reply.properties.contentEncoding;
                        if (encoding == 'with_final') {
                            let finalFlag = (reply.content[reply.content.byteLength-1] != 0);
                            let content = replyType.decode(reply.content.slice(0, reply.content.byteLength-1));
                            console.log({
                                content: content,
                                finalFlag : finalFlag
                            });
                            if (finalFlag) {
                                setTimeout(function() {
                                    connection.close();
                                    process.exit(0);
                                }, 500);
                            }
                        } else {
                            let content = replyType.decode(reply.content);
                            console.log(content);
                            setTimeout(function() {
                                connection.close();
                                process.exit(0);
                            }, 500);
                        }
                    }
                }, {noAck : true});
                channel.sendToQueue(queue, Buffer.from(buffer), {
                    correlationId : correlationId
                    , replyTo: q.queue
                });
            });
        });
    });
}

load("../proto/defs.proto", function(error, root) {
    if (error) {
        throw error;
    }
    switch (cmd) {
    case 'configure':
        {
            let enabled : boolean;
            switch (process.argv[4]) {
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
            const ConfigureCommand = root.lookupType("simple_demo.ConfigureCommand");
            const ConfigureResult = root.lookupType("simple_demo.ConfigureResult");
            let message = ConfigureCommand.create({
                enabled : enabled
            });
            let buffer = attachIdentityToBuffer(ConfigureCommand.encode(message).finish(), mode);
            sendBufferAndWaitForReply("test_config_queue", buffer, ConfigureResult);
        }
        break;
    case 'query':
        {
            const OutstandingCommandsQuery = root.lookupType("simple_demo.OutstandingCommandsQuery");
            const OutstandingCommandsResult = root.lookupType("simple_demo.OutstandingCommandsResult");
            let message = OutstandingCommandsQuery.create({
            });
            //query does not require identity
            let buffer = OutstandingCommandsQuery.encode(message).finish();
            sendBufferAndWaitForReply("test_query_queue", buffer, OutstandingCommandsResult);
        }
        break;
    case 'clear':
        {
            const ids = process.argv.slice(4).map((x) => parseInt(x));
            const ClearCommands = root.lookupType("simple_demo.ClearCommands");
            const ClearCommandsResult = root.lookupType("simple_demo.ClearCommandsResult");
            let message = ClearCommands.create({
                ids: ids
            });
            let buffer = attachIdentityToBuffer(ClearCommands .encode(message).finish(), mode);
            sendBufferAndWaitForReply("test_clear_queue", buffer, ClearCommandsResult);
        }
        break;
    default:
        break;
    }
});