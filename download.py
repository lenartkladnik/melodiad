import asyncio
import websockets

with open("tracks.txt") as f:
    tracks = f.readlines()

async def main():
    async with websockets.connect("ws://localhost:5400/download") as ws:
        for track in tracks:
            await ws.send(track)

            msg = await ws.recv()
            print(msg)

asyncio.run(main())
