"""Action provenance records for explainable and reproducible builds."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from .core import BuildAction, digest, file_fingerprint


@dataclass(frozen=True)
class ActionProvenance:
    command: tuple[str, ...]
    cwd: str | None
    env: tuple[tuple[str, str], ...]
    inputs: tuple[str, ...]
    outputs: tuple[str, ...]
    input_digests: dict[str, str] = field(default_factory=dict)

    @classmethod
    def from_action(cls, action: BuildAction, *, include_content: bool = False):
        digests: dict[str, str] = {}
        if include_content:
            for path in action.inputs:
                try:
                    digests[path] = file_fingerprint(path)
                except FileNotFoundError:
                    digests[path] = "<missing>"
        return cls(
            command=tuple(action.argv()),
            cwd=action.cwd,
            env=tuple(sorted(action.env.items())),
            inputs=tuple(action.inputs),
            outputs=tuple(action.outputs),
            input_digests=digests,
        )

    def to_dict(self) -> dict[str, Any]:
        return {
            "command": list(self.command),
            "cwd": self.cwd,
            "env": dict(self.env),
            "inputs": list(self.inputs),
            "outputs": list(self.outputs),
            "input_digests": dict(sorted(self.input_digests.items())),
        }

    def fingerprint(self) -> str:
        return digest(self.to_dict())


__all__ = ["ActionProvenance"]
