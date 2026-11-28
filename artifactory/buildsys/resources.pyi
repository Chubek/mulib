from collections.abc import Iterable, Mapping
from typing import Any

class ResourceRequest:
    cpu: int
    memory_mb: int
    priority: int
    def __init__(self, cpu: int = ..., memory_mb: int = ..., priority: int = ...) -> None: ...

class ResourceBudget:
    cpu: int
    memory_mb: int
    def __init__(self, cpu: int = ..., memory_mb: int = ...) -> None: ...

class ResourceScheduler:
    budget: ResourceBudget
    def __init__(self, budget: ResourceBudget | None = ...) -> None: ...
    def plan(
        self,
        graph: Mapping[str, Iterable[str]] | Any,
        requests: Mapping[str, ResourceRequest] | None = ...,
    ) -> list[list[str]]: ...

def schedule(
    graph: Mapping[str, Iterable[str]] | Any,
    requests: Mapping[str, ResourceRequest] | None = ...,
    budget: ResourceBudget | None = ...,
) -> list[list[str]]: ...
