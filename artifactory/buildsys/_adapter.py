from __future__ import annotations

import re

from .dep_graph import DependencyGraph
from .target import Target


def _logical_lines(text: str) -> list[str]:
    if not isinstance(text, str):
        raise TypeError("build definition must be text")
    lines: list[str] = []
    pending = ""
    for raw in text.splitlines():
        line = raw.rstrip()
        if line.endswith("\\") and not line.endswith("\\\\"):
            pending += line[:-1] + " "
            continue
        lines.append(pending + line)
        pending = ""
    if pending:
        lines.append(pending)
    return lines


def _strip_comment(line: str) -> str:
    escaped = False
    for index, char in enumerate(line):
        if char == "#" and not escaped:
            return line[:index]
        escaped = char == "\\" and not escaped
        if char != "\\":
            escaped = False
    return line


def _words(text: str) -> list[str]:
    # Make/Ninja rule names can contain escaped whitespace.
    words = re.findall(r"(?:\\.|[^\s])+", text)
    return [word.replace("\\ ", " ").replace("\\#", "#") for word in words]


def parse_lines(text: str) -> DependencyGraph:
    g = DependencyGraph()
    for raw_line in _logical_lines(text):
        if raw_line.startswith("\t"):
            continue
        line = _strip_comment(raw_line).strip()
        if not line or "=" in line.split(":", 1)[0]:
            continue
        m = re.match(r"(.+?)\s*:\s*(.*)$", line)
        if m:
            names, dependencies = m.groups()
            dependency_words = _words(dependencies)
            normal = []
            order_only = False
            for word in dependency_words:
                if word == "|":
                    order_only = True
                    continue
                normal.append(word)
            for name in _words(names):
                metadata = {"order_only": order_only} if order_only else {}
                g.add(Target(name, normal, metadata=metadata))
    return g


__all__ = ["parse_lines"]
