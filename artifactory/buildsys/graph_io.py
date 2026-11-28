"""Stable JSON and DOT representations for build dependency graphs."""

from __future__ import annotations

import json
from collections.abc import Mapping
from typing import Any

from .core import BuildAction
from .dep_graph import DependencyGraph
from .target import Target


def _dot_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)


def to_dict(graph: DependencyGraph) -> dict[str, Any]:
    if not isinstance(graph, DependencyGraph):
        raise TypeError("graph must be a DependencyGraph")
    return {
        "nodes": {
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
            for name, target in sorted(graph.nodes.items())
        }
    }


def from_dict(value: Mapping[str, Any]) -> DependencyGraph:
    if not isinstance(value, Mapping) or not isinstance(value.get("nodes"), Mapping):
        raise ValueError("graph data must contain a 'nodes' mapping")
    graph = DependencyGraph()
    for name, payload in value["nodes"].items():
        if not isinstance(payload, Mapping):
            raise ValueError(f"node {name!r} must be a mapping")
        actions = [
            BuildAction(
                command=action.get("command", action.get("argv")),
                cwd=action.get("cwd"),
                env=dict(action.get("env", {})),
                inputs=list(action.get("inputs", [])),
                outputs=list(action.get("outputs", [])),
                timeout=action.get("timeout"),
                priority=action.get("priority", 0),
            )
            for action in payload.get("actions", [])
        ]
        graph.add(
            Target(
                str(name),
                list(payload.get("deps", [])),
                actions,
                list(payload.get("outputs", [])),
                dict(payload.get("metadata", {})),
            )
        )
    return graph


def dumps(graph: DependencyGraph) -> str:
    return json.dumps(
        to_dict(graph),
        default=str,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    )


def loads(text: str) -> DependencyGraph:
    return from_dict(json.loads(text))


def to_dot(graph: DependencyGraph) -> str:
    if not isinstance(graph, DependencyGraph):
        raise TypeError("graph must be a DependencyGraph")
    lines = ["digraph build {"]
    for name in sorted(graph.nodes):
        lines.append(f"  {_dot_string(name)};")
    for dependent in sorted(graph.nodes):
        for dependency in sorted(graph.nodes[dependent].deps):
            lines.append(f"  {_dot_string(dependency)} -> {_dot_string(dependent)};")
    lines.append("}")
    return "\n".join(lines)


__all__ = ["dumps", "from_dict", "loads", "to_dict", "to_dot"]
