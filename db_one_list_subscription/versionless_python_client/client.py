import sys
sys.path.append("../../../tm_transport/python_lib")

import os
import TMTransport
import asyncio
from enum import Enum
from typing import Callable,List
import cbor2 #for some reason, "cbor" does not handle certain big chunks correctly, but "cbor2" does

import click

class Command(Enum):
    Subscribe = 0
    Update = 1
    Delete = 2
    Unsubscribe = 3
    List = 4
    Snapshot = 5

def attachMyIdentity(x : bytes) -> bytes:
    return cbor2.dumps(["python_client", x])

def parseCommand(s : str) -> Command:
    if s == 'subscribe':
        return Command.Subscribe
    elif s == 'update':
        return Command.Update
    elif s == 'delete':
        return Command.Delete
    elif s == 'unsubscribe':
        return Command.Unsubscribe
    elif s == 'list':
        return Command.List
    elif s == 'snapshot':
        return Command.Snapshot
    else:
        return None

def setup_queue(cmd : Command, qin : asyncio.Queue, qout : asyncio.Queue) -> List[asyncio.Task] :
    subscriptionLocator = "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_subscription_queue_2"
    transactionLocator = "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_transaction_queue_2"
    if cmd in [Command.Subscribe, Command.Unsubscribe, Command.List, Command.Snapshot]:
        return TMTransport.MultiTransportFacilityClient.facility(
            subscriptionLocator, qin, qout,
            identityAttacher=attachMyIdentity
        )
    else:
        return TMTransport.MultiTransportFacilityClient.facility(
            transactionLocator, qin, qout,
            identityAttacher=attachMyIdentity
        )

async def send(qin : asyncio.Queue, qout : asyncio.Queue, x : bytes):
    await qin.put(TMTransport.MultiTransportFacilityClient.keyify(x))
    while True:
        reply = await qout.get()
        decodedOutput = cbor2.loads(reply.output)
        print(repr(decodedOutput))
        if reply.isFinal:
            break
    os.abort() #as of now this seems to be the easiest way of exiting from asyncio code

async def subscribe(qin : asyncio.Queue, qout : asyncio.Queue):
    await send(qin, qout, cbor2.dumps([
        0, {'keys': [0]}
    ]))

async def unsubscribe(qin : asyncio.Queue, qout : asyncio.Queue, id : str):
    if id == 'all':
        await send(qin, qout, cbor2.dumps([
            3, {}
        ]))
    else:
        await send(qin, qout, cbor2.dumps([
            1, {'originalSubscriptionID': id}
        ]))

async def list(qin : asyncio.Queue, qout : asyncio.Queue):
    await send(qin, qout, cbor2.dumps([
        2, {}
    ]))

async def update(qin : asyncio.Queue, qout : asyncio.Queue, name : str, amount : int, stat : float, old_count : int):
    await send(qin, qout, cbor2.dumps([
        1, {
            "key": 0
            , "oldVersionSlice": []
            , "oldDataSummary": [old_count]
            , "dataDelta": {
                "deletes": []
                , "inserts_updates": [
                    [
                        {"name": name}
                        , {"amount": amount, "stat": stat}
                    ]
                ]
            }
        }
    ]))

async def delete(qin : asyncio.Queue, qout : asyncio.Queue, name : str, old_count : int):
    await send(qin, qout, cbor2.dumps([
        1, {
            "key": 0
            , "oldVersionSlice": []
            , "oldDataSummary": [old_count]
            , "dataDelta": {
                "deletes": [{"name": name}]
                , "inserts_updates": []
            }
        }
    ]))

async def snapshot(qin : asyncio.Queue, qout : asyncio.Queue):
    await send(qin, qout, cbor2.dumps([
        4, {'keys': [0]}
    ]))

@click.command()
@click.option('--command', help='subscribe|update|delete|unsubscribe|list')
@click.option('--name', help='name field for db row', default='')
@click.option('--amount', help='amount field for db row', default=0)
@click.option('--stat', help='stat field for db row', default=0.0)
@click.option('--old_count', help='old row count before update', default=0)
@click.option('--id', help='id to unsubscribe', default='all')

def run(command : str, name : str, amount : int, stat : float, old_count : int, id : str):
    cmd = parseCommand(command)
    if not cmd:
        print(f"Unknown command {command}")
        return
    async def asyncMain():
        qin = asyncio.Queue()
        qout = asyncio.Queue()
        tasks = setup_queue(cmd, qin, qout)
        if cmd == Command.Subscribe:
            tasks.append(asyncio.create_task(subscribe(qin, qout)))
        elif cmd == Command.Update:
            tasks.append(asyncio.create_task(update(qin, qout, name, amount, stat, old_count)))
        elif cmd == Command.Delete:
            tasks.append(asyncio.create_task(delete(qin, qout, name, old_count)))
        elif cmd == Command.Unsubscribe:
            tasks.append(asyncio.create_task(unsubscribe(qin, qout, id)))      
        elif cmd == Command.List:
            tasks.append(asyncio.create_task(list(qin, qout)))
        elif cmd == Command.Snapshot:
            tasks.append(asyncio.create_task(snapshot(qin, qout)))  

        await asyncio.gather(*tasks)
    asyncio.run(asyncMain())

if __name__ == "__main__":
    run()
