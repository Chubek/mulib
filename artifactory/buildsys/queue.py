from __future__ import annotations

import heapq
import itertools


class BuildQueue:
    def __init__(self):
        self._q = []
        self._seq = itertools.count()

    def push(self, item, priority=0):
        heapq.heappush(self._q, (-priority, next(self._seq), item))

    def pop(self):
        return heapq.heappop(self._q)[2]

    def peek(self):
        if not self._q:
            raise IndexError("peek from empty build queue")
        return self._q[0][2]

    def __bool__(self):
        return bool(self._q)
