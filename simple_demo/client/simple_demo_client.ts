import {load, Type} from "protobufjs"
import {connect} from "amqplib/callback_api"
import {v4 as uuidv4} from "uuid"

let cmd = process.argv[2];

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
            switch (process.argv[3]) {
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
            let buffer = ConfigureCommand.encode(message).finish();
            sendBufferAndWaitForReply("test_config_queue", buffer, ConfigureResult);
        }
        break;
    case 'query':
        {
            const OutstandingCommandsQuery = root.lookupType("simple_demo.OutstandingCommandsQuery");
            const OutstandingCommandsResult = root.lookupType("simple_demo.OutstandingCommandsResult");
            let message = OutstandingCommandsQuery.create({
            });
            let buffer = OutstandingCommandsQuery.encode(message).finish();
            sendBufferAndWaitForReply("test_query_queue", buffer, OutstandingCommandsResult);
        }
        break;
    case 'clear':
        {
            const ids = process.argv.slice(3).map((x) => parseInt(x));
            const ClearCommands = root.lookupType("simple_demo.ClearCommands");
            const ClearCommandsResult = root.lookupType("simple_demo.ClearCommandsResult");
            let message = ClearCommands.create({
                ids: ids
            });
            let buffer = ClearCommands .encode(message).finish();
            sendBufferAndWaitForReply("test_clear_queue", buffer, ClearCommandsResult);
        }
        break;
    default:
        break;
    }
});