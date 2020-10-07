import {MultiTransportListener} from '../../../tm_transport/node_lib/TMTransport'
import * as Stream from 'stream'
//import {eddsa as EDDSA} from "elliptic"
import * as cbor from 'cbor'
//import * as aes from 'aes-js'
import * as util from 'util'
import * as sodium from 'sodium-native'

const heartbeatLocator = 'rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]'
const heartbeatTopic = 'simple_demo.secure_executables.calculator.heartbeat'

const serverPublicKey = Buffer.from([
    0x69,0x61,0xB9,0xCF,0xBA,0x37,0xD0,0xE2,0x70,0x32,0x84,0xF9,0x41,0x02,0x17,0x22,
    0xFA,0x89,0x0F,0xE4,0xBA,0xAC,0xC8,0x73,0xB9,0x00,0x99,0x24,0x38,0x42,0xC2,0x9A 
]);
/*let verifier = new EDDSA('ed25519');
const decryptKey = aes.utils.utf8.toBytes("testkey"+(' '.repeat(9)));*/
//const decryptKey = Buffer.from("testkey"+(' '.repeat(25)));
let decryptKey = Buffer.alloc(sodium.crypto_generichash_BYTES);
sodium.crypto_generichash(decryptKey, Buffer.from("testkey"));
decryptKey = decryptKey.slice(0, 32);

function verifyAndDecrypt(data : Buffer) : Buffer {
    let cborDecoded = cbor.decode(data);
    /*
    if (!verifier.verify(cborDecoded.data.toString('hex'), cborDecoded.signature.toString('hex'), serverPublicKey.toString('hex'))) {
        return null;
    }
    let len = Number((cborDecoded.data as Buffer).slice(0, 8).readBigUInt64LE());
    let decryptor = new aes.ModeOfOperation.cbc(decryptKey, cborDecoded.data.slice(8, 24));
    return Buffer.from(decryptor.decrypt(cborDecoded.data.slice(24))).slice(0, len);
    */
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

let heartbeatListener = MultiTransportListener.inputStream(
    heartbeatLocator
    , heartbeatTopic
    , verifyAndDecrypt
);

let outputStream = new Stream.Writable({
    write : function(chunk : [string, Buffer], _encoding, callback) {
        let content = cbor.decode(chunk[1]);
        let contentRep = util.inspect(content, { showHidden: false, depth: null, colors: true });
        console.log(`Topic ${chunk[0]}: ${contentRep}`);
        callback();
    }
    , objectMode : true
});

heartbeatListener.pipe(outputStream);