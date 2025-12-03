import asyncio
import random
import string
import time

def do_web_request(product_name: str) -> str:
    time.sleep(0.2 + random.random() * 0.1)
    return f"<html>Hello, world {product_name}!</html>"

async def do_search(product_name: str) -> str:
    return await asyncio.to_thread(do_web_request, product_name)

def random_string(length: int) -> str:
    return ''.join(random.choices(string.ascii_letters + string.digits, k=length))

async def main() -> None:
    start = time.time()
    while True:
        tasks = [do_search(random_string(10)) for _ in range(10)]
        await asyncio.gather(*tasks)
        if time.time() - start > 2:
            break

if __name__ == "__main__":
    asyncio.run(main())