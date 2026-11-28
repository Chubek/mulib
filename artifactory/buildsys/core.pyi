from collections.abc import Mapping, Sequence
from subprocess import CompletedProcess
from typing import Any

class BuildError(ValueError): ...

def digest(value: Any) -> str: ...
def file_fingerprint(path: str, chunk_size: int = ...) -> str: ...
def run_action(action: BuildAction) -> CompletedProcess[str]: ...

class BuildAction:
    command: str | Sequence[str]
    cwd: str | None
    env: dict[str, str]
    inputs: list[str]
    outputs: list[str]
    timeout: float | None
    priority: int
    def __init__(
        self,
        command: str | Sequence[str],
        cwd: str | None = ...,
        env: Mapping[str, str] | None = ...,
        inputs: Sequence[str] = ...,
        outputs: Sequence[str] = ...,
        timeout: float | None = ...,
        priority: int = ...,
    ) -> None: ...
    def argv(self) -> list[str]: ...
    def fingerprint(self) -> str: ...

class Target:
    name: str
    deps: list[str]
    actions: list[BuildAction]
    outputs: list[str]
    metadata: dict[str, Any]
    def __init__(
        self,
        name: str,
        deps: Sequence[str] = ...,
        actions: Sequence[BuildAction] = ...,
        outputs: Sequence[str] = ...,
        metadata: Mapping[str, Any] | None = ...,
    ) -> None: ...

class Artifact:
    path: str
    fingerprint: str | None
    size: int
    mtime: float
    kind: str
    @classmethod
    def from_path(cls, path: str, content: bool = ...) -> Artifact: ...
    def exists(self) -> bool: ...
