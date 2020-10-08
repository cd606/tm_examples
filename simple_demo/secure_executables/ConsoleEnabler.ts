import * as blessed from 'blessed'
import {MultiTransportListener,MultiTransportFacilityClient, FacilityOutput} from '../../../tm_transport/node_lib/TMTransport'
import * as cbor from 'cbor'
import * as proto from 'protobufjs'
import * as Stream from 'stream'
import * as sodium from 'sodium-native'
import {eddsa as EDDSA} from "elliptic"

const serverPublicKey = Buffer.from([
    0xDA,0xA0,0x15,0xD4,0x33,0xE8,0x92,0xC9,0xF2,0x96,0xA1,0xF8,0x1F,0x79,0xBC,0xF4,
    0x2D,0x7A,0xDE,0x48,0x03,0x47,0x16,0x0C,0x57,0xBD,0x1F,0x45,0x81,0xB5,0x18,0x2E 
]);
let decryptKey = Buffer.alloc(sodium.crypto_generichash_BYTES);
sodium.crypto_generichash(decryptKey, Buffer.from("testkey"));
decryptKey = decryptKey.slice(0, 32);

const signature_key_bytes = Buffer.from([
    0x89,0xD9,0xE6,0xED,0x17,0xD6,0x7B,0x30,0xE6,0x16,0xAC,0xB4,0xE6,0xD0,0xAD,0x47,
    0xD5,0x0C,0x6E,0x5F,0x11,0xDF,0xB1,0x9F,0xFE,0x4D,0x23,0x2A,0x0D,0x45,0x84,0x8E
]);
const signature_key = new EDDSA("ed25519").keyFromSecret(signature_key_bytes);

function verifyAndDecrypt(data : Buffer) : Buffer {
    let cborDecoded = cbor.decode(data);
    if (!sodium.crypto_sign_verify_detached(
        cborDecoded.signature
        , cborDecoded.data
        , serverPublicKey
    )) {
        return null;
    }
    let ret = Buffer.alloc(cborDecoded.data.byteLength-sodium.crypto_secretbox_NONCEBYTES-sodium.crypto_secretbox_MACBYTES);
    if (sodium.crypto_secretbox_open_easy(
        ret
        , cborDecoded.data.slice(sodium.crypto_secretbox_NONCEBYTES)
        , cborDecoded.data.slice(0, sodium.crypto_secretbox_NONCEBYTES)
        , decryptKey
    )) {
        return ret;
    } else {
        return null;
    }
}

proto.load('../proto/defs.proto').then(function(root) {
    let inputT = root.lookupType('simple_demo.ConfigureCommand');
    let outputT = root.lookupType('simple_demo.ConfigureResult');
    run(inputT, outputT);
});

function run(inputT : proto.Type, outputT : proto.Type) : void {
    let screen = blessed.screen({
        smartCSR: true
        , title: 'Plain Console Enabler'
    });
    let label = blessed.box({
        parent: screen
        , top: '20%'
        , left: 'center'
        , width: '150'
        , content: 'Disconnected'
        , style: {
            fg: 'white'
        }
        , focusable: false
    });
    let form = blessed.form({
        parent: screen
        , top: '60%'
        , left: '10%'
        , width: '80%'
        , height: 5
        , border: {
            type: 'line'
        }
        , style: {
            border: {
                fg: 'blue'
            }
        }
        , keys: true
    });
    let enable = blessed.button({
        parent: form
        , left: '10%'
        , width: '30%'
        , height: 3
        , content: 'Enable'
        , border: {
            type: 'line'
        }
        , style: {
            fg: 'green'
            , border: {
                fg: 'blue'
            }, focus: {
                bg: 'white'
            }
        }
        , align: 'center'
    });
    let disable = blessed.button({
        parent: form
        , left: '60%'
        , width: '30%'
        , height: 3
        , content: 'Disable'
        , border: {
            type: 'line'
        }
        , style: {
            fg: 'red'
            , border: {
                fg: 'blue'
            }
            , focus: {
                bg: 'white'
            }
        }
        , align: 'center'
    });
    form.focus();
    screen.key(['escape', 'q', 'C-c'], function(_ch, _key) {
        return process.exit(0);
    });
    screen.render();

    let cfgChannelInfo : string = null;
    let cfgStreams : [Stream.Writable, Stream.Readable] = null;
    let keyify = MultiTransportFacilityClient.keyify();
    let callbackStream = new Stream.Writable({
        write : function(chunk : FacilityOutput, _encoding, callback) {
            let parsed = outputT.decode(chunk.output);
            if (parsed.hasOwnProperty("enabled") && parsed["enabled"]) {
                label.content = 'Enabled';
                label.style.fg = 'green';
            } else {
                label.content = 'Disabled';
                label.style.fg = 'red';
            }
            screen.render();
            callback();
        }
        , objectMode : true
    });
    function setupCfgChannel() {
        (async () => {
            cfgStreams = await MultiTransportFacilityClient.facilityStream({
                address : cfgChannelInfo
                , identityAttacher : function(data : Buffer) {
                    let signature = signature_key.sign(data);
                    return cbor.encode({"signature" : Buffer.from(signature.toBytes()), "data" : data});
                }
            });
            keyify.pipe(cfgStreams[0]);
            cfgStreams[1].pipe(callbackStream);
        })();
    }

    enable.on('press', () => {
        keyify.write(inputT.encode({enabled : true}).finish());
    });
    disable.on('press', () => {
        keyify.write(inputT.encode({enabled : false}).finish());
    })

    let heartbeatStream = MultiTransportListener.inputStream(
        "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
        , "simple_demo.secure_executables.#.heartbeat"
        , verifyAndDecrypt
    );
    let heartbeatHandler = new Stream.Writable({
        write: function(chunk : [string, Buffer], _encoding, callback) {
            let x = cbor.decode(chunk[1]);
            if (x.hasOwnProperty("sender_description")) {
                if (x.sender_description == "simple_demo secure MainLogic") {
                    if (x.hasOwnProperty("facility_channels") && x.facility_channels.hasOwnProperty("cfgFacility")) {
                        let channelInfoFromHeartbeat = x.facility_channels["cfgFacility"];
                        if (cfgChannelInfo == null) {
                            cfgChannelInfo = channelInfoFromHeartbeat;
                            setupCfgChannel();
                        }
                    }
                    let status = x.details.calculation_status.info as string;
                    if (status == 'enabled') {
                        label.content = 'Enabled';
                        label.style.fg = 'green';
                    } else {
                        label.content = 'Disabled';
                        label.style.fg = 'red';
                    }
                    screen.render();
                }
            }
            callback();
        }
        , objectMode : true
    });
    heartbeatStream.pipe(heartbeatHandler);
}