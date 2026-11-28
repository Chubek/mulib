from __future__ import annotations

import re
import shlex
from pathlib import Path

from .dep_graph import DependencyGraph
from .target import Target


def _calls(text: str):
    pattern = re.compile(r"\b(executable|library|shared_library|static_library)\s*\(", re.I)
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
            yield match.group(1).lower(), text[match.end() : index - 1]


def _tokens(body: str) -> list[str]:
    lexer = shlex.shlex(body, posix=True)
    lexer.whitespace_split = True
    lexer.commenters = "#"
    return [token for token in lexer if token != ","]


def parse(text):
    if not isinstance(text, str):
        raise TypeError("Meson input must be text")
    g = DependencyGraph()
    for kind, body in _calls(text):
        name_match = re.match(r"\s*['\"]([^'\"]+)['\"]", body)
        if name_match is None:
            continue
        name = name_match.group(1)
        dependencies: list[str] = []
        dependency_match = re.search(
            r"\b(?:dependencies|link_with|link_whole)\s*:\s*\[(.*?)\]",
            body,
            re.S,
        )
        if dependency_match:
            dependencies = re.findall(r"""['"]([^'"]+)['"]""", dependency_match.group(1))
        g.add(Target(name, dependencies, metadata={"kind": kind}))
    return g


def parse_meson(path: str) -> DependencyGraph:
    return parse(Path(path).read_text(encoding="utf-8"))


__all__ = ["parse", "parse_meson"]
