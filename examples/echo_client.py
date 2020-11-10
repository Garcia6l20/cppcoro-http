#!/usr/bin/env python3
import asyncio
import random
import string
import http.client as client


def make_random_string(length):
    return ''.join(random.choice(string.ascii_lowercase) for i in range(length))


async def main():
    con = client.HTTPConnection('127.0.0.1', 4242)

    def check(expected):
        con.request('POST', "/", expected)
        response = con.getresponse()
        data = response.read().decode()
        print(f"received: {data}")
        assert data == expected

    check(make_random_string(128))
    check(make_random_string(1024))


if __name__ == '__main__':
    asyncio.get_event_loop().run_until_complete(main())

