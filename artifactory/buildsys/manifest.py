"""Validation and canonicalization for small build manifests."""

from __future__ import annotations

import json
import os
import tempfile
from collections.abc import Mapping
from pathlib import Path
from typing import Any

from .core import BuildAction, BuildError, Target


class ManifestError(BuildError):
    """Raised when a build manifest violates the kernel schema."""


def _action(value: Any) -> BuildAction:
    if isinstance(value, BuildAction):
        return value
    if not isinstance(value, Mapping):
        raise ManifestError("actions must be mappings")
    command = value.get("command", value.get("argv"))
    if command is None:
        raise ManifestError("action requires 'command' or 'argv'")
    return BuildAction(
        command=command,
        cwd=value.get("cwd"),
        env=dict(value.get("env", {})),
        inputs=list(value.get("inputs", [])),
        outputs=list(value.get("outputs", [])),
        timeout=value.get("timeout"),
        priority=value.get("priority", 0),
    )


def _target(name: str, value: Any) -> Target:
    if isinstance(value, Target):
        return value
    if not isinstance(value, Mapping):
        raise ManifestError(f"target {name!r} must be a mapping")
    return Target(
        name=name,
        deps=list(value.get("deps", value.get("dependencies", []))),
        actions=[_action(action) for action in value.get("actions", [])],
        outputs=list(value.get("outputs", [])),
        metadata=dict(value.get("metadata", {})),
    )


def validate(manifest: Mapping[str, Any], *, require_actions: bool = False) -> dict[str, Any]:
    """Validate and return a deterministic, JSON-safe manifest."""
    if not isinstance(manifest, Mapping):
        raise ManifestError("manifest must be a mapping")
    raw_targets = manifest.get("targets", {})
    if isinstance(raw_targets, list):
        entries = {}
        for item in raw_targets:
            if not isinstance(item, Mapping) or not item.get("name"):
                raise ManifestError("target list entries require a name")
            name = str(item["name"])
            if name in entries:
                raise ManifestError(f"duplicate target: {name}")
            entries[name] = item
    elif isinstance(raw_targets, Mapping):
        entries = {}
        for raw_name, value in raw_targets.items():
            name = str(raw_name).strip()
            if not name:
                raise ManifestError("target names must be non-empty")
            if name in entries:
                raise ManifestError(f"duplicate target: {name}")
            entries[name] = value
    else:
        raise ManifestError("'targets' must be a mapping or list")

    targets: dict[str, Target] = {}
    for name, value in entries.items():
        target = _target(name, value)
        if require_actions and not target.actions:
            raise ManifestError(f"target {name!r} has no actions")
        targets[name] = target

    known = set(targets)
    unknown = sorted(
        {
            dependency
            for target in targets.values()
            for dependency in target.deps
            if dependency not in known
        }
    )
    if unknown:
        raise ManifestError(f"unknown target dependencies: {', '.join(unknown)}")

    try:
        version = int(manifest.get("version", 1))
    except (TypeError, ValueError) as exc:
        raise ManifestError("manifest version must be an integer") from exc
    if version <= 0:
        raise ManifestError("manifest version must be positive")

    return {
        "version": version,
        "name": str(manifest.get("name", "")),
        "targets": {
            name: {
                "deps": list(target.deps),
                "outputs": list(target.outputs),
                "actions": [
                    {
                        "command": action.argv(),
                        "cwd": action.cwd,
                        "env": dict(sorted(action.env.items())),
                        "inputs": list(action.inputs),
                        "outputs": list(action.outputs),
                        "timeout": action.timeout,
                        "priority": action.priority,
                    }
                    for action in target.actions
                ],
                "metadata": target.metadata,
            }
            for name, target in sorted(targets.items())
        },
    }


def load(path: str, *, require_actions: bool = False) -> dict[str, Any]:
    try:
        value = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ManifestError(f"unable to read manifest {path!r}: {exc}") from exc
    return validate(value, require_actions=require_actions)


def dump(manifest: Mapping[str, Any], path: str) -> dict[str, Any]:
    normalized = validate(manifest)
    destination = Path(path)
    destination.parent.mkdir(parents=True, exist_ok=True)
    payload = (json.dumps(normalized, ensure_ascii=False, indent=2, sort_keys=True) + "\n").encode(
        "utf-8"
    )
    fd, temporary = tempfile.mkstemp(prefix=".manifest-", suffix=".tmp", dir=destination.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, destination)
    except OSError:
        Path(temporary).unlink(missing_ok=True)
        raise
    return normalized


__all__ = ["ManifestError", "dump", "load", "validate"]
