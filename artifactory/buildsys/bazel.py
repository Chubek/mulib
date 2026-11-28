from __future__ import annotations

import re
from pathlib import Path

from .dep_graph import DependencyGraph
from .target import Target


def _rule_calls(text: str):
    pattern = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(", re.M)
    for match in pattern.finditer(text):
        depth = 1
        quote = None
        escaped = False
        index = match.end()
        while index < len(text) and depth:
            char = text[index]
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif quote:
                if char == quote:
                    quote = None
            elif char in {"'", '"'}:
                quote = char
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
            index += 1
        if depth == 0:
            yield match.group(1), text[match.end() : index - 1]


def _attribute(body: str, name: str) -> list[str]:
    match = re.search(rf"\b{re.escape(name)}\s*=\s*\[(.*?)\]", body, re.S)
    if not match:
        return []
    return re.findall(r"""["']([^"']+)["']""", match.group(1))


def parse(text):
    if not isinstance(text, str):
        raise TypeError("Bazel input must be text")
    g = DependencyGraph()
    for kind, body in _rule_calls(text):
        name_match = re.search(r"\bname\s*=\s*[\"']([^\"']+)[\"']", body)
        if not name_match:
            continue
        dependencies = _attribute(body, "deps")
        dependencies.extend(_attribute(body, "srcs"))
        g.add(Target(name_match.group(1), dependencies, metadata={"kind": kind}))
    return g


def parse_build(path: str) -> DependencyGraph:
    return parse(Path(path).read_text(encoding="utf-8"))


__all__ = ["parse", "parse_build"]
