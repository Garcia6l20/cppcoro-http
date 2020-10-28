#!/usr/bin/env python3
import asyncio
from websockets import client


async def main():
    ws = await client.connect("ws://localhost:4242")
    print("Sending 'Hello, World'...")
    await ws.send("Hello, World")
    print("Sent")
    print("Receiving...")
    result = await ws.recv()
    print(f"Received {result}")


if __name__ == '__main__':
    asyncio.get_event_loop().run_until_complete(main())
