import sys
sys.path.append("../../../tm_transport/python_lib")

import TMTransport
import asyncio
from datetime import datetime
import cbor2
import time

def runSender(interval : int, bytes : int, address : str, summaryPeriod : int):
    async def asyncMain():
        tasks = []
        publishQueue = asyncio.Queue()
        tasks.extend(TMTransport.MultiTransportPublisher.output(address, publishQueue))
        data = (' '*bytes).encode('utf-8')
        global counter
        counter = 0
        async def createData():
            global counter
            while True:
                await asyncio.sleep(interval*0.001)
                now = int(round(time.time()*1000.0))
                counter = counter+1
                publishQueue.put_nowait(('test.data', cbor2.dumps((counter, now, data))))
        tasks.append(asyncio.create_task(createData()))
        if summaryPeriod > 0:
            async def printSummary():
                global counter
                while True:
                    await asyncio.sleep(summaryPeriod)
                    now = datetime.now()
                    print(f"{now.strftime('%Y-%m-%d %H:%M:%S.%f')}: Sent {counter} messages")
            tasks.append(asyncio.create_task(printSummary()))
        await asyncio.gather(*tasks)
    asyncio.run(asyncMain())

def runReceiver(address : str, summaryPeriod : int):
    class Stats:
        count : int
        totalDelay : float
        totalDelaySq : float
        minID : int
        maxID : int
        def __init__(self):
            self.count = 0
            self.totalDelay = 0.0
            self.totalDelaySq = 0.0
            self.minID = 0
            self.maxID = 0
    async def asyncMain():
        tasks = []
        receiveQueue = asyncio.Queue()
        tasks.extend(TMTransport.MultiTransportListener.input(address, [receiveQueue], topic = 'test.data'))
        global stats
        stats = Stats()
        async def updateStats():
            global stats
            while True:
                topic, data = await receiveQueue.get()
                parsed = cbor2.loads(data)
                now = int(round(time.time()*1000.0))
                delay = now-parsed[1]
                id = parsed[0]
                stats.count = stats.count+1
                stats.totalDelay = stats.totalDelay+1.0*delay
                stats.totalDelaySq = stats.totalDelaySq+1.0*delay*delay
                if stats.minID == 0 or stats.minID > id:
                    stats.minID = id
                if stats.maxID < id:
                    stats.maxID = id
        tasks.append(asyncio.create_task(updateStats()))
        async def printSummary():
            global stats
            while True:
                await asyncio.sleep(summaryPeriod)
                now = datetime.now()
                mean = 0.0
                sd = 0.0
                missed = 0
                if stats.count > 0:
                    mean = stats.totalDelay/stats.count
                    missed = stats.maxID-stats.minID+1-stats.count
                if stats.count > 1:
                    sd = (stats.totalDelaySq-mean*mean*stats.count)/(stats.count-1)
                print(f"{now.strftime('%Y-%m-%d %H:%M:%S.%f')}: : Got {stats.count} messages, mean delay {mean} ms, std delay {sd} ms, missed {missed} messages")
        tasks.append(asyncio.create_task(printSummary()))
        await asyncio.gather(*tasks)
    asyncio.run(asyncMain())

import click

@click.command()
@click.option('--mode', help='sender or receiver')
@click.option('--interval', help='interval for publishing in milliseconds', default=0)
@click.option('--bytes', help='byte count for publishing', default=0)
@click.option('--address', help='PROTOCOL://LOCATOR')
@click.option('--summaryPeriod', help='summary period in seconds', default=0)

def run(mode : str, interval : int, bytes: int, address : str, summaryperiod : int):
    if mode == 'sender':
        if interval <= 0:
            print('sender interval cannot be 0')
            sys.exit(0)
        runSender(interval, bytes, address, summaryperiod)
    elif mode == 'receiver':
        if summaryperiod <= 0:
            print('receiver summary period cannot be 0')
            sys.exit(0)
        runReceiver(address, summaryperiod)
    else:
        print(f"Unknown mode string '{mode}', must be sender or receiver")
        sys.exit(0)

if __name__ == "__main__":
    run()