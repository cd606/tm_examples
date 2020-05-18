import {load, Root, Message, Type} from "protobufjs"
import * as amqp from 'amqplib'
import {v4 as uuidv4} from "uuid"
import {eddsa as EDDSA} from "elliptic"

export class TMHelper {
    private static parseLocator(locator : string) {
        let idx = locator.indexOf('[');
        let mainPortion : string;
        let propertyPortion : string;
        if (idx < 0) {
            mainPortion = locator;
            propertyPortion = "";
        } else {
            mainPortion = locator.substr(0, idx);
            propertyPortion = locator.substr(idx);
        }
        let mainParts = mainPortion.split(':');
        let properties = {};
        if (propertyPortion.length > 2 && propertyPortion[propertyPortion.length-1] == ']') {
            let realPropertyPortion = propertyPortion.substr(1, propertyPortion.length-2);
            let propertyParts = realPropertyPortion.split(',');
            for (let p of propertyParts) {
                let nameAndValue = p.split('=');
                if (nameAndValue.length == 2) {
                    let name = nameAndValue[0];
                    let value = nameAndValue[1];
                    properties[name] = value;
                }
            }
        }
        let ret = {
            "properties" : properties
        };
        if (mainParts.length >= 1) {
            ret["host"] = mainParts[0];
        } else {
            ret["host"] = "";
        }
        if (mainParts.length >= 2 && mainParts[1] != "") {
            ret["port"] = parseInt(mainParts[1]);
        } else {
            ret["port"] = 0;
        }
        if (mainParts.length >= 3) {
            ret["username"] = mainParts[2];
        } else {
            ret["username"] = "";
        }
        if (mainParts.length >= 4) {
            ret["password"] = mainParts[3];
        } else {
            ret["password"] = "";
        }
        if (mainParts.length >= 5) {
            ret["identifier"] = mainParts[4];
        } else {
            ret["identifier"] = "";
        }
        return ret;
    }

    private has_identity : boolean;
    private signature_key : EDDSA.KeyPair;
    private plain_name : string;
    private plain_name_length : number;
    private use_signature : boolean;
    private incoming_type : Type;
    private outgoing_type : Type;

    constructor() {
        this.has_identity = false;
    }

    async set_incoming_type(protoFile: string, typeName : string) {
        const protoRoot = await load(protoFile);
        this.incoming_type = protoRoot.lookupType(typeName);
    }
    async set_outgoing_type(protoFile: string, typeName : string) {
        const protoRoot = await load(protoFile);
        this.outgoing_type = protoRoot.lookupType(typeName);
    }
    async set_incoming_and_outgoing_type(protoFile: string, incomingTypeName : string, outgoingTypeName : string) {
        const protoRoot = await load(protoFile);
        this.incoming_type = protoRoot.lookupType(incomingTypeName);
        this.outgoing_type = protoRoot.lookupType(outgoingTypeName);
    }

    set_identity(identity: (Buffer|string)) {
        if (Buffer.isBuffer(identity)) {
            this.signature_key = new EDDSA("ed25519").keyFromSecret(identity);
            this.use_signature = true;
        } else {
            this.plain_name = identity;
            this.plain_name_length = this.plain_name.length;
            this.use_signature = false;
        }
        this.has_identity = true;
    }
    disable_identity() {
        this.has_identity = false;
    }
    enable_identity() {
        this.has_identity = true;
    }

    attach_identity(input: Uint8Array) : Uint8Array {
        if (!this.has_identity) {
            return input;
        }
        if (this.use_signature) {
            let inputBuffer = Buffer.from(input);
            let signature = this.signature_key.sign(inputBuffer);
            let ret = Buffer.concat([inputBuffer,Buffer.from(signature.toBytes())]);
            return new Uint8Array(ret);
        } else {
            let inputBuffer = Buffer.from(input);
            let b = Buffer.alloc(4);
            b.writeUInt32LE(this.plain_name_length);
            let ret = Buffer.concat([b, Buffer.from(this.plain_name, 'ascii'), inputBuffer]);
            return new Uint8Array(ret);
        }
    }

    private static locatorToURL(locator : {}) {
        let portStr = (locator["port"]>0)?(":"+locator["port"]):"";
        return `amqp://${locator["username"]}:${locator["password"]}@${locator["host"]}${portStr}`;
    }

    async listen(locatorStr : string, topic : string, callback : (topic: string, data: Message<{}>) => void) {
        let locator = TMHelper.parseLocator(locatorStr);
        const url = TMHelper.locatorToURL(locator);
        let connection = await amqp.connect(url);
        let channel = await connection.createChannel();
        channel.assertExchange(locator["identifier"], "topic", {
            durable : (locator["properties"]["durable"] == "true")
        });
        let queue = await channel.assertQueue('', {exclusive: true});
        channel.bindQueue(queue.queue, locator["identifier"], topic);
        let parser = this.incoming_type;
        channel.consume(queue.queue, function(msg) {
            if (msg.content) {
                let data = parser.decode(msg.content);
                callback(topic, data);
            }
        });
    }

    async send_request(locatorStr : string, request : {}, callback : (isFinal: boolean, reply: Message<{}>) => void) {
        let locator = TMHelper.parseLocator(locatorStr);
        const url = TMHelper.locatorToURL(locator);
        let connection = await amqp.connect(url);
        let channel = await connection.createChannel();
        let queue = await channel.assertQueue('', {exclusive:true});
        let correlationId = uuidv4();
        let parser = this.incoming_type;
        channel.consume(queue.queue, function(reply) {
            if (reply.properties.correlationId == correlationId) {
                let encoding = reply.properties.contentEncoding;
                if (encoding == 'with_final') {
                    let finalFlag = (reply.content[reply.content.byteLength-1] != 0);
                    let content = parser.decode(reply.content.slice(0, reply.content.byteLength-1));
                    callback(finalFlag, content);
                    if (finalFlag) {
                        setTimeout(function() {
                            connection.close();
                            process.exit(0);
                        }, 500);
                    }
                } else {
                    let content = parser.decode(reply.content);
                    callback(false, content);
                }
            }
        }, {noAck : true});
        let buffer = this.outgoing_type.encode(this.outgoing_type.create(request)).finish();
        let sendBuffer = this.attach_identity(buffer);
        channel.sendToQueue(
            locator["identifier"]
            , Buffer.from(sendBuffer)
            , {
                correlationId : correlationId
                , replyTo: queue.queue
            }
        );
    }
}
/*
(async () => {
    var helper = new TMHelper();
    await helper.set_incoming_type('../proto/defs.proto', 'simple_demo.InputData');
    await helper.listen(
        'localhost::guest:guest:amq.topic[durable=true]'
        , 'input.data'
        , function(topic:string, data:Message<{}>) {
            let obj = {topic: topic, data: data};
            console.log(obj);
        }
    );
})();*/