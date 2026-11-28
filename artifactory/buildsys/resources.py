"""Dependency-aware scheduling with simple CPU and memory budgets."""

from __future__ import annotations

from collections.abc import Iterable, Mapping
from dataclasses import dataclass
from typing import Any

from .dep_resolve import _mapping


@dataclass(frozen=True)
class ResourceRequest:
    cpu: int = 1
    memory_mb: int = 0
    priority: int = 0

    def __post_init__(self) -> None:
        if self.cpu <= 0:
            raise ValueError("cpu request must be positive")
        if self.memory_mb < 0:
            raise ValueError("memory request cannot be negative")


@dataclass(frozen=True)
class ResourceBudget:
    cpu: int = 1
    memory_mb: int = 0

    def __post_init__(self) -> None:
        if self.cpu <= 0:
            raise ValueError("CPU budget must be positive")
        if self.memory_mb < 0:
            raise ValueError("memory budget cannot be negative")


class ResourceScheduler:
    """Produce deterministic execution waves without oversubscribing resources."""

    def __init__(self, budget: ResourceBudget | None = None):
        self.budget = budget or ResourceBudget()

    def plan(
        self,
        graph: Mapping[str, Iterable[str]] | Any,
        requests: Mapping[str, ResourceRequest] | None = None,
    ) -> list[list[str]]:
        deps = _mapping(graph)
        requests = requests or {}
        normalized = {
            name: request if isinstance(request, ResourceRequest) else ResourceRequest(**request)
            for name, request in requests.items()
        }
        unknown_requests = set(normalized) - set(deps)
        if unknown_requests:
            raise ValueError(f"resource request for unknown target: {sorted(unknown_requests)[0]}")
        for name, request in normalized.items():
            if request.cpu > self.budget.cpu or (
                self.budget.memory_mb and request.memory_mb > self.budget.memory_mb
            ):
                raise ValueError(f"resource request for {name!r} exceeds scheduler budget")

        remaining = {name: set(values) for name, values in deps.items()}
        waves: list[list[str]] = []
        while remaining:
            ready = [name for name, values in remaining.items() if not values]
            if not ready:
                raise ValueError("dependency cycle detected")
            ready.sort(
                key=lambda name: (
                    -normalized.get(name, ResourceRequest()).priority,
                    name,
                )
            )
            wave: list[str] = []
            used_cpu = 0
            used_memory = 0
            for name in ready:
                request = normalized.get(name, ResourceRequest())
                if used_cpu + request.cpu > self.budget.cpu:
                    continue
                if (
                    self.budget.memory_mb
                    and used_memory + request.memory_mb > self.budget.memory_mb
                ):
                    continue
                wave.append(name)
                used_cpu += request.cpu
                used_memory += request.memory_mb
            if not wave:
                raise ValueError("ready targets cannot fit the resource budget")
            waves.append(wave)
            for name in wave:
                remaining.pop(name)
            for values in remaining.values():
                values.difference_update(wave)
        return waves


def schedule(
    graph: Mapping[str, Iterable[str]] | Any,
    requests: Mapping[str, ResourceRequest] | None = None,
    budget: ResourceBudget | None = None,
) -> list[list[str]]:
    return ResourceScheduler(budget).plan(graph, requests)


__all__ = ["ResourceBudget", "ResourceRequest", "ResourceScheduler", "schedule"]
