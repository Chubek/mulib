from pathlib import Path

from ._adapter import _logical_lines, _strip_comment, _words
from .dep_graph import DependencyGraph
from .target import Target


def parse(text):
    if not isinstance(text, str):
        raise TypeError("Ninja input must be text")
    graph = DependencyGraph()
    for raw_line in _logical_lines(text):
        if raw_line[:1].isspace():
            continue
        line = _strip_comment(raw_line).strip()
        if not line:
            continue
        if not line.startswith("build "):
            continue
        parts = line[6:].split(":", 1)
        if len(parts) != 2:
            continue
        outputs = _words(parts[0])
        fields = _words(parts[1])
        if not outputs or not fields:
            continue
        rule = fields[0]
        dependencies = [field for field in fields[1:] if field not in {"|", "||", "|@"}]
        for output in outputs:
            graph.add(Target(output, dependencies, metadata={"rule": rule}))
    return graph


def parse_ninja(path: str):
    return parse(Path(path).read_text(encoding="utf-8"))


def execute(graph, target=None):
    if target is None:
        return graph
    if target not in graph.nodes:
        raise KeyError(f"unknown target: {target}")
    return graph.nodes[target]


__all__ = ["execute", "parse", "parse_ninja"]
