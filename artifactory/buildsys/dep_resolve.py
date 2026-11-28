from __future__ import annotations

import heapq
from collections.abc import Iterable, Mapping
from typing import Any


def _mapping(graph: Mapping[str, Iterable[str]] | Any) -> dict[str, set[str]]:
    source = graph.mapping() if hasattr(graph, "mapping") else graph
    if not isinstance(source, Mapping):
        raise TypeError("graph must be a mapping or DependencyGraph")
    deps: dict[str, set[str]] = {}
    for name, dependencies in source.items():
        if not isinstance(name, str) or not name:
            raise ValueError("graph node names must be non-empty strings")
        if isinstance(dependencies, (str, bytes)):
            raise ValueError(f"dependencies for {name!r} must be an iterable of names")
        deps[name] = set(dependencies)
    for dependencies in list(deps.values()):
        for dependency in dependencies:
            if not isinstance(dependency, str) or not dependency:
                raise ValueError("graph dependencies must be non-empty strings")
            deps.setdefault(dependency, set())
    return deps


def topological_sort(graph: Mapping[str, Iterable[str]] | Any) -> list[str]:
    """Return a deterministic dependency-first order."""
    deps = _mapping(graph)
    reverse: dict[str, set[str]] = {name: set() for name in deps}
    for dependent, dependencies in deps.items():
        for dependency in dependencies:
            reverse[dependency].add(dependent)
    ready = [name for name, dependencies in deps.items() if not dependencies]
    heapq.heapify(ready)
    result: list[str] = []
    while ready:
        name = heapq.heappop(ready)
        result.append(name)
        for dependent in sorted(reverse[name]):
            deps[dependent].discard(name)
            if not deps[dependent]:
                heapq.heappush(ready, dependent)
    if len(result) != len(deps):
        raise ValueError("dependency cycle detected")
    return result


def detect_cycles(graph: Mapping[str, Iterable[str]] | Any) -> list[list[str]]:
    """Return canonical cycle paths, including self-cycles."""
    normalized = _mapping(graph)
    state: dict[str, int] = {}
    stack: list[str] = []
    cycles: list[list[str]] = []
    seen: set[tuple[str, ...]] = set()

    def canonical(cycle: list[str]) -> tuple[str, ...]:
        body = cycle[:-1]
        rotations = [tuple(body[index:] + body[:index]) for index in range(len(body))]
        return min(rotations)

    def visit(n: str) -> None:
        state[n] = 1
        stack.append(n)
        for d in sorted(normalized[n]):
            if state.get(d) == 1:
                i = stack.index(d)
                cycle = [*stack[i:], d]
                key = canonical(cycle)
                if key not in seen:
                    seen.add(key)
                    cycles.append([*list(key), key[0]])
            elif not state.get(d):
                visit(d)
        stack.pop()
        state[n] = 2

    for n in sorted(normalized):
        if not state.get(n):
            visit(n)
    return cycles


__all__ = ["detect_cycles", "topological_sort"]
