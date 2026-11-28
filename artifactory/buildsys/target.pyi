from collections.abc import Sequence
from typing import Any

class BuildAction:
    command: str | Sequence[str]
    cwd: str | None
    env: dict[str, str]
    inputs: list[str]
    outputs: list[str]
    timeout: float | None
    priority: int
    def argv(self) -> list[str]: ...
    def fingerprint(self) -> str: ...

class Target:
    name: str
    deps: list[str]
    actions: list[BuildAction]
    outputs: list[str]
    metadata: dict[str, Any]
