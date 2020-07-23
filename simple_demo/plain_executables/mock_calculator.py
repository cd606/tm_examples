import sys
sys.path.append("../../../tm_transport/python_lib")
sys.path.append("../client")

import os
import TMTransport
import asyncio
from enum import Enum
from typing import Callable,Tuple
import cbor2 
import uuid
from datetime import datetime
import socket

import defs_pb2

async def run():
    def decode(x : bytes) -> defs_pb2.CalculateCommand:
        c = defs_pb2.CalculateCommand()
        c.ParseFromString(x)
        return c
    async def calc(x : Tuple[str,defs_pb2.CalculateCommand], q : asyncio.Queue):
        cmdID = x[1].id
        cmdVal = x[1].value
        r = defs_pb2.CalculateResult()
        r.id = cmdID
        r.result = cmdVal*2.0
        await q.put((x[0], r.SerializeToString(), False))
        await asyncio.sleep(2)
        r = defs_pb2.CalculateResult()
        r.id = cmdID
        r.result = -1
        await q.put((x[0], r.SerializeToString(), True))
    def resolveIdentity(x : bytes) -> Tuple[bool,str,bytes]:
        parsed = cbor2.loads(x)
        if len(parsed) == 2:
            return (True,parsed[0],parsed[1])
        else:
            return (False,None,None)
    qin = asyncio.Queue()
    qout = asyncio.Queue()
    #serviceAddr = "redis://127.0.0.1:6379:::test_queue"
    serviceAddr = "rabbitmq://127.0.0.1::guest:guest:test_queue"
    TMTransport.MultiTransportFacilityServer.facility(
        serviceAddr
        , qin
        , qout
        , identityResolver=resolveIdentity
    )
    heartbeatQ = asyncio.Queue()
    TMTransport.MultiTransportPublisher.output(
        "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
        , heartbeatQ
    )
    async def publishHeartbeat():
        heartbeatID = str(uuid.uuid4())
        host = socket.gethostname()
        pid = os.getpid()
        while True:
            await asyncio.sleep(1)
            await heartbeatQ.put(
                (
                    "simple_demo.plain_executables.calculator.heartbeat"
                    , cbor2.dumps({
                        "uuid_str" : heartbeatID
                        , "timestamp" : int(datetime.now().timestamp())
                        , "host" : host
                        , "pid" : pid
                        , "sender_description" : 'simple_demo plain Calculator'
                        , "broadcast_channels" : []
                        , "facility_channels" : {
                            'facility' : serviceAddr
                        }
                        , "details" : {
                            'program' : {
                                'status' : 'Good'
                                , 'info' : ''
                            }
                        }
                    })
                )
            )
    asyncio.create_task(publishHeartbeat())
    while True:
        id, dataAndIdentity = await qin.get()
        identity, data = dataAndIdentity
        cmd = decode(data)
        asyncio.create_task(calc((id,cmd), qout))

if __name__ == '__main__':
    asyncio.run(run())