import {MultiTransportFacilityServer, MultiTransportPublisher} from '../../../tm_transport/node_lib/TMTransport'
import * as proto from 'protobufjs'
import { Stream } from 'stream';
import * as cbor from 'cbor';
import {v4 as uuidv4} from "uuid"
import * as os from 'os'

async function run() {
    let defs = await proto.load('../proto/defs.proto');
    let cmdType = defs.lookupType('simple_demo.CalculateCommand');
    let resType = defs.lookupType('simple_demo.CalculateResult');

    let decodeStream = new Stream.Transform({
        transform : function(chunk : [string, [string, Buffer]], encoding, callback) {
            let [id, [_client_identity, data]] = chunk;
            let parsedCmd = cmdType.decode(data);
            this.push([id, parsedCmd], encoding);
            callback();
        },
        objectMode : true
    });
    let calcStream = new Stream.Transform({
        transform : function(chunk : [string, proto.Message<{}>], _encoding, callback) {
            let [id, parsedCmd] = chunk;
            let cmdID = parsedCmd["id"] as number;
            let cmdVal = parsedCmd["value"] as number;
            this.push([id, [cmdID, cmdVal*2.0, false]]);
            setTimeout(function() {
                calcStream.push([id, [cmdID, -1.0, true]]);
            }, 2000);
            callback();
        },
        objectMode : true
    });
    let encodeStream = new Stream.Transform({
        transform : function(chunk : [string, [string, number, boolean]], _encoding, callback) {
            let [id, [cmdID, val, final]] = chunk;
            let encoded = resType.encode({
                "id" : cmdID
                , "result" : val
            }).finish();
            this.push([id, encoded, final]);
            callback();
        },
        objectMode : true
    });
    let serviceStream = await MultiTransportFacilityServer.facilityStream({
        address : "rabbitmq://127.0.0.1::guest:guest:test_queue"
        , identityResolver : function(data : Buffer) {
            let parsed = cbor.decode(data);
            if (parsed.length == 2) {
                return [true, parsed[0], parsed[1]];
            } else {
                return [false, null, null];
            }
        }
    });
    serviceStream[1]
        .pipe(decodeStream)
        .pipe(calcStream)
        .pipe(encodeStream)
        .pipe(serviceStream[0]);
    let heartbeatPublisher = await MultiTransportPublisher.outputStream(
        "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
    );
    let heartbeatID = uuidv4();
    setInterval(function() {
        heartbeatPublisher.write(
            [
                "simple_demo.plain_executables.calculator.heartbeat"
                , cbor.encode({
                    uuid_str : heartbeatID
                    , timestamp : new Date().getTime()*1000
                    , host : os.hostname()
                    , pid : process.pid
                    , sender_description: 'simple_demo plain Calculator'
                    , broadcast_channels: []
                    , facility_channels : {
                        'facility' : 'rabbitmq://127.0.0.1::guest:guest:test_queue'
                    }
                    , details : {
                        'program' : {
                            'status' : 'Good'
                            , 'info' : ''
                        }
                    }
                })
            ]
        )
    }, 1000);
}

run();