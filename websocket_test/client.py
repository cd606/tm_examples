import asyncio
import pathlib
import ssl
import websockets

ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
ssl_context.load_verify_locations('../grpc_interop_test/DotNetServer/server.crt')

async def hello():
    async with websockets.connect("wss://localhost:34567", ssl=ssl_context) as websocket: 
        async for message in websocket:
            print(message)

asyncio.run(hello())