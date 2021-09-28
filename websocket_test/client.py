import asyncio
import ssl
import websockets
import cbor2
import sys

ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
ssl_context.load_verify_locations('../grpc_interop_test/DotNetServer/server.crt')

async def test():
    async with websockets.connect("wss://localhost:34567", ssl=ssl_context) as websocket: 
        async for message in websocket:
            print(message.decode("utf-8"))

async def test2():
    async with websockets.connect("ws://localhost:45678") as websocket:
        async for message in websocket:
            x = cbor2.loads(message)
            print(cbor2.loads(x[1]))

if len(sys.argv) > 1 and sys.argv[1] == 'cbor':
    asyncio.run(test2())
else:
    asyncio.run(test())