import sys
sys.path.append("../../../tm_transport/python_lib")

import TMTransport
import asyncio
from enum import Enum
from typing import Callable
from datetime import datetime
import cbor

async def printQueueData(qin : asyncio.Queue, printMode : str = "length"):
    printFunc : Callable[[bytes], str] = lambda x : ""
    if printMode == "length":
        printFunc = lambda x : f"{len(x)} bytes"
    elif printMode == "string":
        printFunc = lambda x : x.decode('utf-8')
    elif printMode == "cbor":
        printFunc = lambda x : repr(cbor.loads(x))

    #Please note that Python's protobuf module does not support
    #dynamic decoding, therefore the dynamic protobuf printing 
    #functionality in node version is not supported here

    while True:
        item : Tuple[string, bytes] = await qin.get()
        if item:
            now = datetime.now()
            print(f"{now.strftime('%Y-%m-%d %H:%M:%S.%f')}: topic '{item[0]}': {printFunc(item[1])}")

async def asyncMain() :
    printQueue = asyncio.Queue()
    TMTransport.MultiTransportListener.input(
        "rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]"
        , [printQueue]
        , topic = "simple_demo.plain_executables.calculator.heartbeat"
    )
    await printQueueData(printQueue, printMode="cbor")

if __name__ == "__main__":
    asyncio.run(asyncMain())