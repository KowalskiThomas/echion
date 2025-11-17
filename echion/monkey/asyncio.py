from __future__ import annotations

from typing import Coroutine, Any, Iterable, Iterator, Optional, Set, TypeVar, cast


import sys
import typing as t
import asyncio
from asyncio import tasks
from asyncio.events import BaseDefaultEventLoopPolicy
from functools import wraps
from threading import current_thread

import echion.core as echion


# -----------------------------------------------------------------------------

_set_event_loop = BaseDefaultEventLoopPolicy.set_event_loop


@wraps(_set_event_loop)
def set_event_loop(self, loop) -> None:
    echion.track_asyncio_loop(t.cast(int, current_thread().ident), loop)
    return _set_event_loop(self, loop)


# -----------------------------------------------------------------------------

_gather = tasks._GatheringFuture.__init__  # type: ignore[attr-defined]


@wraps(_gather)
def gather(self, children, *, loop):
    # Link the parent gathering task to the gathered children
    parent = tasks.current_task(loop)

    assert parent is not None

    for child in children:
        echion.link_tasks(parent, child)

    return _gather(self, children, loop=loop)


# -----------------------------------------------------------------------------

_wait = tasks._wait  # pyright: ignore[reportAttributeAccessIssue]

T = TypeVar('T')

@wraps(tasks._wait)  # pyright: ignore[reportAttributeAccessIssue]
def wait(fs: Iterable[asyncio.Future[T]], timeout: Optional[float], return_when, loop: asyncio.AbstractEventLoop) -> Coroutine[Any, Any, tuple[Set[Any], Set[Any]]]:   
    loop = asyncio.get_running_loop()
    parent = tasks.current_task(loop)
    assert parent is not None

    for child in fs:
        echion.link_tasks(parent, cast(asyncio.Task, child))

    return _wait(fs, timeout, return_when, loop)

# -----------------------------------------------------------------------------

def patch():
    BaseDefaultEventLoopPolicy.set_event_loop = set_event_loop  # type: ignore[method-assign]
    tasks._GatheringFuture.__init__ = gather  # type: ignore[attr-defined]
    tasks._wait = wait  # type: ignore[attr-defined]


def unpatch():
    BaseDefaultEventLoopPolicy.set_event_loop = _set_event_loop  # type: ignore[method-assign]
    tasks._GatheringFuture.__init__ = _gather  # type: ignore[attr-defined]
    tasks._wait = _wait  # type: ignore[attr-defined]


def track():
    if sys.hexversion >= 0x030C0000:
        scheduled_tasks = tasks._scheduled_tasks.data  # pyright: ignore[reportAttributeAccessIssue]
        eager_tasks = tasks._eager_tasks  # pyright: ignore[reportAttributeAccessIssue]
    else:
        scheduled_tasks = tasks._all_tasks.data  # pyright: ignore[reportAttributeAccessIssue]
        eager_tasks = None

    echion.init_asyncio(tasks._current_tasks, scheduled_tasks, eager_tasks)  # type: ignore[attr-defined]
