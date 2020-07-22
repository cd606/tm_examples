import sys
sys.path.append("../../../tm_transport/python_lib")

import os
import TMTransport
import asyncio
from enum import Enum
from typing import Callable
import cbor2 

import nacl.encoding
import nacl.signing
from Crypto.Cipher import AES

async def asyncMain() :
    heartbeatLocator = 'rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]'
    heartbeatTopic = 'simple_demo.secure_executables.calculator.heartbeat'

    serverPublicKey = b'\x69\x61\xB9\xCF\xBA\x37\xD0\xE2\x70\x32\x84\xF9\x41\x02\x17\x22\xFA\x89\x0F\xE4\xBA\xAC\xC8\x73\xB9\x00\x99\x24\x38\x42\xC2\x9A'
    decryptKey = ('testkey'+(' '*9)).encode('utf-8')

    verifyKey = nacl.signing.VerifyKey(serverPublicKey)

    def verifyAndDecrypt(x : bytes) -> bytes:
        d = cbor2.loads(x)
        res = verifyKey.verify(d['data'], d['signature'])
        if not res:
            return None
        l = int.from_bytes(res[0:8], byteorder='little')
        cipher = AES.new(decryptKey, AES.MODE_CBC, iv=res[8:24])
        plainText = cipher.decrypt(res[24:])
        return plainText[0:l]

    q = asyncio.Queue()
    TMTransport.MultiTransportListener.input(heartbeatLocator, [q], heartbeatTopic, wireToUserHook=verifyAndDecrypt)
    while True:
        topic, x = await q.get()
        print(f"{topic}: {repr(cbor2.loads(x))}")

if __name__ == "__main__":
    asyncio.run(asyncMain())

