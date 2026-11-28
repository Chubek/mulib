from __future__ import annotations

from collections.abc import Iterable
from concurrent.futures import ThreadPoolExecutor

from .executor import Executor


def schedule(
    actions: Iterable,
    executor: Executor | None = None,
    workers: int = 1,
    *,
    fail_fast: bool = True,
):
    """Execute independent actions concurrently, preserving input order.

    Dependency-aware target scheduling belongs to ``resources.py``; this
    primitive intentionally schedules an already-resolved action batch.
    """
    if workers <= 0:
        raise ValueError("workers must be positive")
    action_list = list(actions)
    ex = executor or Executor()
    with ThreadPoolExecutor(max_workers=workers) as pool:
        if fail_fast:
            return list(pool.map(ex.execute, action_list))
        futures = [pool.submit(ex.execute, action) for action in action_list]
        return [future.result() for future in futures]
