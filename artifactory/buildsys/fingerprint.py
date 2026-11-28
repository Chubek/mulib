from __future__ import annotations

from collections.abc import Mapping
from pathlib import Path
from typing import Any

from .core import BuildAction, digest, file_fingerprint


def command_fingerprint(
    command: Any,
    env: Mapping[str, str] | None = None,
    cwd: str | None = None,
) -> str:
    return digest({"command": command, "env": dict(env or {}), "cwd": cwd})


def action_fingerprint(action: BuildAction, *, include_inputs: bool = False) -> str:
    """Fingerprint an action, optionally incorporating current input content."""
    value: dict[str, Any] = {"action": action.fingerprint()}
    if include_inputs:
        value["inputs"] = {
            path: file_fingerprint(path) if Path(path).is_file() else "<missing>"
            for path in action.inputs
        }
    return digest(value)


__all__ = ["action_fingerprint", "command_fingerprint", "digest", "file_fingerprint"]
