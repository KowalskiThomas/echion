import asyncio
import math


async def dependency(t: float) -> None:
    await asyncio.sleep(t)


async def mixed_workload() -> int:
    await dependency(0.5)

    result = 1
    for i in range(150):
        result *= math.factorial(i)

    await dependency(0.25)
    return result


async def async_main() -> None:
    result = await mixed_workload()
    assert result > 0


def main() -> None:
    asyncio.run(async_main())


if __name__ == "__main__":
    main()
