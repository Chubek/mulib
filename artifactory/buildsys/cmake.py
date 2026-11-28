from __future__ import annotations

import re
import shlex
from pathlib import Path

from .dep_graph import DependencyGraph
from .target import Target


def _without_comments(text: str) -> str:
    result: list[str] = []
    index = 0
    quote = False
    while index < len(text):
        char = text[index]
        if char == '"':
            quote = not quote
            result.append(char)
            index += 1
        elif char == "#" and not quote:
            newline = text.find("\n", index)
            if newline < 0:
                break
            result.append("\n")
            index = newline + 1
        else:
            result.append(char)
            index += 1
    return "".join(result)


def _commands(text: str):
    """Yield CMake command names and balanced argument bodies."""
    if not isinstance(text, str):
        raise TypeError("CMake input must be text")
    text = _without_comments(text)
    pattern = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(", re.M)
    for match in pattern.finditer(text):
        depth = 1
        quote = False
        escaped = False
        index = match.end()
        while index < len(text) and depth:
            char = text[index]
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quote = not quote
            elif not quote and char == "(":
                depth += 1
            elif not quote and char == ")":
                depth -= 1
            index += 1
        if depth == 0:
            yield match.group(1).lower(), text[match.end() : index - 1]


def _tokens(body: str) -> list[str]:
    lexer = shlex.shlex(body, posix=True)
    lexer.whitespace_split = True
    lexer.commenters = "#"
    return [token for token in lexer if token != ";"]


_LIBRARY_MODES = {
    "STATIC",
    "SHARED",
    "MODULE",
    "OBJECT",
    "UNKNOWN",
    "INTERFACE",
    "IMPORTED",
    "ALIAS",
    "EXCLUDE_FROM_ALL",
    "WIN32",
    "MACOSX_BUNDLE",
}


def parse(text):
    g = DependencyGraph()
    for command, body in _commands(text):
        tokens = _tokens(body)
        if not tokens:
            continue
        if command in {"add_executable", "add_library"}:
            name = tokens[0]
            dependencies = [token for token in tokens[1:] if token.upper() not in _LIBRARY_MODES]
            g.add(Target(name, dependencies, metadata={"kind": command[4:]}))
        elif command == "add_dependencies":
            for dependency in tokens[1:]:
                g.add_edge(dependency, tokens[0])
        elif command == "target_sources":
            target = tokens[0]
            dependencies = [
                token
                for token in tokens[1:]
                if token.upper() not in {"PRIVATE", "PUBLIC", "INTERFACE"}
            ]
            g.add(Target(target, dependencies))
    return g


def parse_cmake(path: str) -> DependencyGraph:
    return parse(Path(path).read_text(encoding="utf-8"))


__all__ = ["parse", "parse_cmake"]
