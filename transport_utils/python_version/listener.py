import sys
sys.path.append("../../../tm_transport/python_lib")

import TMTransport
import asyncio
from enum import Enum
from typing import Callable
from datetime import datetime
import cbor
import aiofiles

import click

count : int = 0

async def printQueueData(qin : asyncio.Queue, printMode : str = "length"):
    printFunc : Callable[[bytes], str] = lambda x : ""
    if printMode == "length":
        printFunc = lambda x : f"{len(x)} bytes"
    elif printMode == "string":
        printFunc = lambda x : x.decode('utf-8')
    elif printMode == "cbor":
        printFunc = lambda x : repr(cbor.loads(x))
    elif printMode == "bytes":
        printFunc = lambda x : repr(x)

    global count

    #Please note that Python's protobuf module does not support
    #dynamic decoding, therefore the dynamic protobuf printing 
    #functionality in node version is not supported here

    while True:
        item : Tuple[string, bytes] = await qin.get()
        count = count+1
        if item:
            now = datetime.now()
            print(f"{now.strftime('%Y-%m-%d %H:%M:%S.%f')}: topic '{item[0]}': {printFunc(item[1])}")

@click.command()
@click.option('--address', help='PROTOCOL://LOCATOR')
@click.option('--topic', help='topic to subscribe', default=None)
@click.option('--printMode', help='length|string|cbor|none|bytes', default='length')
@click.option('--summaryPeriod', help='summary period in seconds', default=0)
@click.option('--captureFile', help='capture file name', default=None)

def run(address : str, topic : str, printmode : str, summaryperiod : int, capturefile : str):
    async def asyncMain() :
        if summaryperiod > 0:
            async def printSummary():
                global count
                while True:
                    await asyncio.sleep(summaryperiod)
                    now = datetime.now()
                    print(f"{now.strftime('%Y-%m-%d %H:%M:%S.%f')}: Received {count} messages so far")
            asyncio.create_task(printSummary())
        printQueue = asyncio.Queue()
        qouts = [printQueue]
        if capturefile:
            captureQueue = asyncio.Queue()
            async def writeCaptureFile():
                async with aiofiles.open(capturefile, "wb") as o:
                    await o.write(b'\x01\x23\x45\x67')
                    while True:
                        topic, data = await captureQueue.get()
                        now = datetime.now()
                        t : int = int(now.timestamp()*1000.0)+(now.microsecond//1000)
                        l : int = len(topic)
                        ld : int = len(data)
                        writeData = b'\x76\x54\x32\x10'\
                                    +t.to_bytes(8, byteorder='little')\
                                    +l.to_bytes(4, byteorder='little')\
                                    +topic.encode()\
                                    +ld.to_bytes(4, byteorder='little')\
                                    +data\
                                    +b'\x00'
                        await o.write(writeData)
            asyncio.create_task(writeCaptureFile())
            qouts = [printQueue, captureQueue]
        TMTransport.MultiTransportListener.input(address, qouts, topic = topic)
        await printQueueData(printQueue, printMode=printmode)
    asyncio.run(asyncMain())

if __name__ == "__main__":
    run()