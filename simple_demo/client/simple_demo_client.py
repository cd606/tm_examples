import sys
sys.path.append("../../../tm_transport/python_lib")

import os
import TMTransport
import asyncio
from enum import Enum
from typing import Callable,List
import cbor2 

import nacl.encoding
import nacl.signing

import defs_pb2

def sign(mode : str) -> Callable[[bytes],bytes]:
    if mode == 'sign':
        priv_key = b'\x89\xD9\xE6\xED\x17\xD6\x7B\x30\xE6\x16\xAC\xB4\xE6\xD0\xAD\x47\xD5\x0C\x6E\x5F\x11\xDF\xB1\x9F\xFE\x4D\x23\x2A\x0D\x45\x84\x8E'
        signatureKey = nacl.signing.SigningKey(priv_key)
        def realSign(x : bytes) -> bytes:
            sig = signatureKey.sign(x)
            return cbor2.dumps({"data": x, "signature": sig.signature})
        return realSign
    else:
        return lambda x : cbor2.dumps(('simple_demo_client.py', x))

async def printData(q : asyncio.Queue, protoClass):
    while True:
        x : TMTransport.FacilityOutput = await q.get()
        parsed = protoClass()
        parsed.ParseFromString(x.output)
        print(f"{repr(parsed)} (isFinal: {x.isFinal})")
        if x.isFinal:
            os.abort()

async def runConfigure(mode : str, cmdData : str):
    qin = asyncio.Queue()
    qout = asyncio.Queue()
    TMTransport.MultiTransportFacilityClient.facility(
        'rabbitmq://127.0.0.1::guest:guest:test_config_queue'
        , qin, qout
        ,identityAttacher=sign(mode)
    )
    sendCmd = defs_pb2.ConfigureCommand()
    sendCmd.enabled = (cmdData == 'enable')
    await qin.put(TMTransport.MultiTransportFacilityClient.keyify(
        sendCmd.SerializeToString()
    ))
    await printData(qout, defs_pb2.ConfigureResult)

async def runQuery():
    qin = asyncio.Queue()
    qout = asyncio.Queue()
    TMTransport.MultiTransportFacilityClient.facility(
        'rabbitmq://127.0.0.1::guest:guest:test_query_queue'
        , qin, qout
    )
    sendCmd = defs_pb2.OutstandingCommandsQuery()
    await qin.put(TMTransport.MultiTransportFacilityClient.keyify(
        sendCmd.SerializeToString()
    ))
    await printData(qout, defs_pb2.OutstandingCommandsResult)

async def runClear(mode : str, cmdData : List[int]):
    qin = asyncio.Queue()
    qout = asyncio.Queue()
    TMTransport.MultiTransportFacilityClient.facility(
        'rabbitmq://127.0.0.1::guest:guest:test_clear_queue'
        , qin, qout
        ,identityAttacher=sign(mode)
    )
    sendCmd = defs_pb2.ClearCommands()
    for id in cmdData:
        sendCmd.ids.append(id)
    await qin.put(TMTransport.MultiTransportFacilityClient.keyify(
        sendCmd.SerializeToString()
    ))
    await printData(qout, defs_pb2.ClearCommandsResult)

if __name__ == "__main__":
    mode = sys.argv[1]
    cmd = sys.argv[2]
    if cmd == 'configure':
        asyncio.run(runConfigure(mode, sys.argv[3]))
    elif cmd == 'query':
        asyncio.run(runQuery())
    elif cmd == 'clear':
        ids = map(int, sys.argv[3:])
        asyncio.run(runClear(mode, ids))