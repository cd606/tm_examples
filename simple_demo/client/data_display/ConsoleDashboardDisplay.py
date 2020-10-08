import sys
sys.path.append("../../../../tm_transport/python_lib")
sys.path.append("..")

import os
from typing import Callable
import asyncio
import blessed
import dashing
from tkinter import *
import aiotkinter
import cbor2
import click
import nacl.encoding
import nacl.signing
import nacl.secret
import nacl.utils
import nacl.hash

import TMTransport 
import defs_pb2
from time import sleep

dashing_ui = None
dashing_chart = None
dashing_textDisplay = None

def setup_dashing():
    global dashing_ui
    global dashing_chart
    global dashing_textDisplay
    dashing_ui = dashing.VSplit(
        dashing.Text('Value', border_color=2, color = 1)
        , dashing.HBrailleFilledChart(border_color=2, color=1)
    )
    dashing_chart = dashing_ui.items[1]
    dashing_textDisplay = dashing_ui.items[0]
    dashing_ui.display()

def handleData_dashing(x : float):
    global dashing_ui
    global dashing_chart
    global dashing_textDisplay
    dashing_textDisplay.text = "Value: %.6f" % x
    dashing_chart.append(x)
    dashing_ui.display()

tk_label = None
tk_canvas = None
tk_canvas_line = None
tk_canvas_line_coords = []

def setup_tk():
    root = Tk()
    root.title("Python/Tkinter dashboard display")
    root.protocol("WM_DELETE_WINDOW", lambda: os.abort())
    global tk_label
    tk_label = Label(root, text="Value")
    tk_label.pack()
    global tk_canvas
    tk_canvas = Canvas(root, width='500', height='100')
    tk_canvas.pack()
    global tk_canvas_line
    tk_canvas_line = tk_canvas.create_line(0,0,500,100,fill='red')
    asyncio.set_event_loop_policy(aiotkinter.TkinterEventLoopPolicy())

def handleData_tk(x : float):
    global tk_label
    global tk_canvas
    global tk_canvas_line
    tk_label.config(text=("Value: %.6f" % x))
    global tk_canvas_line_coords
    tk_canvas_line_coords.append(x)
    if (len(tk_canvas_line_coords) > 500):
        tk_canvas_line_coords.pop(0)
    if (len(tk_canvas_line_coords) >= 2):
        real_coords = [tk_canvas_line]
        c_x = 0
        for y in tk_canvas_line_coords:
            real_coords.append(c_x)
            c_x = c_x+1
            real_coords.append(round(100.0-y))
        tk_canvas.coords(*real_coords)

def realRun(secure : bool, dataCallback: Callable[[float],None]):
    async def asyncMain():
        tasks = []
        heartbeatQueue = asyncio.Queue()
        tasks.extend(TMTransport.MultiTransportListener.input(
            'rabbitmq://127.0.0.1::guest:guest:amq.topic[durable=true]'
            , [heartbeatQueue]
            , topic = ('simple_demo.secure_executables.data_source.heartbeat' if secure else 'simple_demo.plain_executables.data_source.heartbeat')
        ))
        dataChannel : str = None
        async def handleHeartbeat(queue : asyncio.Queue):
            nonlocal dataChannel
            while True:
                item: Tuple[string, bytes] = await queue.get()
                heartbeat = cbor2.loads(item[1])
                if 'sender_description' in heartbeat:
                    if heartbeat['sender_description'] == ('simple_demo secure DataSource' if secure else 'simple_demo DataSource'):
                        if 'broadcast_channels' in heartbeat and 'input data publisher' in heartbeat['broadcast_channels']:
                            channelInfoFromHeartbeat : str = heartbeat['broadcast_channels']['input data publisher'][0]
                            if dataChannel is None or dataChannel != channelInfoFromHeartbeat:
                                dataChannel = channelInfoFromHeartbeat
                                subscribeToData(dataChannel)
        tasks.append(asyncio.create_task(handleHeartbeat(heartbeatQueue)))
        dataQueue = asyncio.Queue()
        decryptKey = nacl.hash.generichash('input_data_key'.encode('utf-8'), encoder=nacl.encoding.RawEncoder)[0:32]
        def secureDataHook(data : [bytes]) -> bytes:
            nonlocal decryptKey
            nonce = data[0:nacl.secret.SecretBox.NONCE_SIZE]
            return nacl.secret.SecretBox(decryptKey).decrypt(data[nacl.secret.SecretBox.NONCE_SIZE:], nonce)
        def subscribeToData(channel : str) :
            tasks.extend(TMTransport.MultiTransportListener.input(
                channel
                , [dataQueue]
                , topic = 'input.data'
                , wireToUserHook = secureDataHook if secure else None
            ))
        async def handleData(queue : asyncio.Queue):
            nonlocal dataCallback
            while True:
                item: Tuple[string, bytes] = await queue.get()
                x = defs_pb2.InputData()
                x.ParseFromString(item[1])
                y = x.value
                dataCallback(y)
        tasks.append(asyncio.create_task(handleData(dataQueue)))
        await asyncio.gather(*tasks)
    asyncio.run(asyncMain())

@click.command()
@click.option('--secure/--plain', default=False, help='Data source is the secure version')
@click.option('--gui', default='dashing', help='GUI choice: dashing or tk')

def run(secure : bool, gui : str):
    if gui == 'dashing':
        setup_dashing()
        realRun(secure, handleData_dashing)
    elif gui == 'tk':
        setup_tk()
        realRun(secure, handleData_tk)
    else:
        pass

if __name__ == "__main__":
    run()