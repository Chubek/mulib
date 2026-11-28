"""Shared, deliberately small build-system data types and primitives.

The first version of the build kernels used ``repr`` for identities and let
invalid actions reach ``subprocess``.  That is convenient for a prototype, but
it makes cache keys unstable and turns configuration errors into late process
failures.  This module is the trust boundary for the rest of ``buildsys``:
inputs are normalized here and identities are canonicalized here.
"""

from __future__ import annotations

import hashlib
import json
import math
import os
import shlex
import subprocess
from collections.abc import Mapping, Sequence
from dataclasses import asdict, dataclass, field, is_dataclass
from pathlib import Path
from typing import Any


class BuildError(ValueError):
    """Base error for malformed build descriptions and actions."""


def _canonical(value: Any) -> Any:
    """Return a JSON-safe, deterministic representation of *value*."""
    if isinstance(value, Path):
        return value.as_posix()
    if is_dataclass(value):
        return _canonical(asdict(value))
    if isinstance(value, Mapping):
        return {
            str(key): _canonical(item)
            for key, item in sorted(value.items(), key=lambda pair: str(pair[0]))
        }
    if isinstance(value, (set, frozenset)):
        return sorted((_canonical(item) for item in value), key=repr)
    if isinstance(value, (list, tuple)):
        return [_canonical(item) for item in value]
    if isinstance(value, float) and not math.isfinite(value):
        raise BuildError("non-finite values cannot be used in a build identity")
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    return str(value)


def digest(value: Any) -> str:
    """Hash a value using deterministic JSON encoding or raw bytes."""
    if isinstance(value, bytes):
        data = value
    else:
        data = json.dumps(
            _canonical(value),
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
        ).encode("utf-8")
    return hashlib.sha256(data).hexdigest()


def _normalize_strings(values: Sequence[str] | None, field_name: str) -> list[str]:
    result: list[str] = []
    for value in values or ():
        if not isinstance(value, str) or not value:
            raise BuildError(f"{field_name} entries must be non-empty strings")
        if "\x00" in value:
            raise BuildError(f"{field_name} entries cannot contain NUL bytes")
        if value not in result:
            result.append(value)
    return result


@dataclass
class BuildAction:
    command: str | Sequence[str]
    cwd: str | None = None
    env: dict[str, str] = field(default_factory=dict)
    inputs: list[str] = field(default_factory=list)
    outputs: list[str] = field(default_factory=list)
    timeout: float | None = None
    priority: int = 0

    def __post_init__(self) -> None:
        if isinstance(self.command, str):
            if not self.command.strip():
                raise BuildError("command must not be empty")
        elif not self.command:
            raise BuildError("command must contain at least one argument")
        else:
            self.command = [str(argument) for argument in self.command]
        if self.cwd is not None:
            if not isinstance(self.cwd, str) or not self.cwd:
                raise BuildError("cwd must be a non-empty string or None")
            if "\x00" in self.cwd:
                raise BuildError("cwd cannot contain NUL bytes")
        if not isinstance(self.env, dict):
            self.env = dict(self.env)
        normalized_env: dict[str, str] = {}
        for key, value in self.env.items():
            if not isinstance(key, str) or not key or "\x00" in key:
                raise BuildError("environment names must be non-empty strings")
            if "\x00" in str(value):
                raise BuildError("environment values cannot contain NUL bytes")
            normalized_env[key] = str(value)
        self.env = normalized_env
        self.inputs = _normalize_strings(self.inputs, "inputs")
        self.outputs = _normalize_strings(self.outputs, "outputs")
        if self.timeout is not None:
            self.timeout = float(self.timeout)
            if not math.isfinite(self.timeout) or self.timeout <= 0:
                raise BuildError("timeout must be a positive finite number")
        self.priority = int(self.priority)

    def argv(self) -> list[str]:
        """Return an argv vector and reject shell-like empty commands."""
        argv = shlex.split(self.command) if isinstance(self.command, str) else list(self.command)
        if not argv or not argv[0]:
            raise BuildError("command expands to an empty argv")
        if any("\x00" in argument for argument in argv):
            raise BuildError("command arguments cannot contain NUL bytes")
        return argv

    def fingerprint(self) -> str:
        return digest(
            {
                "argv": self.argv(),
                "cwd": self.cwd,
                "env": self.env,
                "inputs": sorted(self.inputs),
                "outputs": sorted(self.outputs),
                "timeout": self.timeout,
                "priority": self.priority,
            }
        )


@dataclass
class Target:
    name: str
    deps: list[str] = field(default_factory=list)
    actions: list[BuildAction] = field(default_factory=list)
    outputs: list[str] = field(default_factory=list)
    metadata: dict[str, Any] = field(default_factory=dict)

    def __post_init__(self) -> None:
        if not isinstance(self.name, str) or not self.name.strip():
            raise BuildError("target name must be a non-empty string")
        self.name = self.name.strip()
        self.deps = _normalize_strings(self.deps, "dependencies")
        self.outputs = _normalize_strings(self.outputs, "outputs")
        self.actions = list(self.actions)
        if any(not isinstance(action, BuildAction) for action in self.actions):
            raise BuildError("target actions must be BuildAction instances")
        self.metadata = dict(self.metadata or {})


@dataclass
class Artifact:
    path: str
    fingerprint: str | None = None
    size: int = 0
    mtime: float = 0.0
    kind: str = "file"

    @classmethod
    def from_path(cls, path: str, content: bool = True) -> Artifact:
        p = Path(path)
        st = p.stat()
        kind = "directory" if p.is_dir() else "file"
        fp = file_fingerprint(str(p)) if content and p.is_file() else None
        return cls(str(p), fp, st.st_size, st.st_mtime, kind)

    def exists(self) -> bool:
        return Path(self.path).exists()


def run_action(action: BuildAction) -> subprocess.CompletedProcess[str]:
    """Run an action without invoking a shell and with declared output dirs."""
    argv = action.argv()
    if action.cwd is not None and not Path(action.cwd).is_dir():
        raise BuildError(f"action cwd does not exist or is not a directory: {action.cwd}")
    for output in action.outputs:
        Path(output).parent.mkdir(parents=True, exist_ok=True)
    return subprocess.run(
        argv,
        cwd=action.cwd,
        env={**os.environ, **action.env},
        capture_output=True,
        text=True,
        timeout=action.timeout,
        check=False,
    )


def file_fingerprint(path: str, chunk_size: int = 1024 * 1024) -> str:
    """Hash a regular file incrementally."""
    if chunk_size <= 0:
        raise BuildError("chunk_size must be positive")
    file_path = Path(path)
    if not file_path.is_file():
        raise FileNotFoundError(path)
    h = hashlib.sha256()
    with file_path.open("rb") as stream:
        while chunk := stream.read(chunk_size):
            h.update(chunk)
    return h.hexdigest()


__all__ = [
    "Artifact",
    "BuildAction",
    "BuildError",
    "Target",
    "digest",
    "file_fingerprint",
    "run_action",
]
