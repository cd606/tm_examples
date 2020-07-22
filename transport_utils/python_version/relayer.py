import sys
sys.path.append("../../../tm_transport/python_lib")

import TMTransport
import asyncio
from datetime import datetime

import click

count : int = 0

@click.command()
@click.option('--incomingAddress', help='PROTOCOL://LOCATOR')
@click.option('--outgoingAddress', help='PROTOCOL://LOCATOR')
@click.option('--summaryPeriod', help='summary period in seconds', default=0)

def run(incomingaddress : str, outgoingaddress : str, summaryperiod : int):
    async def asyncMain() :
        relayQueue = asyncio.Queue()
        qouts = [relayQueue]
        if summaryperiod > 0:
            summaryQueue = asyncio.Queue()
            async def incrCount():
                global count
                while True:
                    await summaryQueue.get()
                    count = count+1
            asyncio.create_task(incrCount())
            async def printSummary():
                global count
                while True:
                    await asyncio.sleep(summaryperiod)
                    now = datetime.now()
                    print(f"{now.strftime('%Y-%m-%d %H:%M:%S.%f')}: Relayed {count} messages so far")
            asyncio.create_task(printSummary())
            qouts = [relayQueue, summaryQueue]
        TMTransport.MultiTransportListener.input(incomingaddress, qouts, topic = None)
        await TMTransport.MultiTransportPublisher.output(outgoingaddress, relayQueue)
    asyncio.run(asyncMain())
        
if __name__ == "__main__":
    run()