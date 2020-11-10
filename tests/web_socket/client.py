#!/usr/bin/env python3
import asyncio
import random
import string
from websockets import client


def make_random_string(length):
    return ''.join(random.choice(string.ascii_lowercase) for i in range(length))


async def main():
    ws = await client.connect("ws://localhost:4242")
    expected = make_random_string(65537)
    print(f"Sending '{expected}'...")
    await ws.send(expected)
    print("Sent")
    print("Receiving...")
    result = await ws.recv()
    print(f"Received {result}")
    assert result.decode('utf-8') == expected


if __name__ == '__main__':
    asyncio.get_event_loop().run_until_complete(main())
