"""GCC/Ninja-style depfile parsing.

Depfiles are deceptively easy to parse incorrectly: rules may span lines and
spaces, ``#``, and backslashes can be escaped.  This kernel intentionally
returns a plain target-to-prerequisites mapping so it composes with the
existing dependency graph and adapter APIs.
"""

from __future__ import annotations

from pathlib import Path


class DepfileError(ValueError):
    """Raised for malformed dependency-file syntax."""


def _logical_lines(text: str) -> list[str]:
    if not isinstance(text, str):
        raise TypeError("depfile input must be text")
    result: list[str] = []
    pending = ""
    for raw in text.splitlines():
        trailing = 0
        for char in reversed(raw):
            if char == "\\":
                trailing += 1
            else:
                break
        if trailing % 2 == 1:
            pending += raw[:-1] + " "
            continue
        result.append(pending + raw)
        pending = ""
    if pending:
        raise DepfileError("unterminated line continuation")
    return result


def _split_rule(line: str) -> tuple[str, str]:
    escaped = False
    for index, char in enumerate(line):
        if char == ":" and not escaped:
            # Do not mistake the colon in a Windows drive prefix for the
            # target/prerequisite separator.
            if index == 1 and line[0].isalpha():
                continue
            return line[:index], line[index + 1 :]
        escaped = not escaped if char == "\\" else False
    raise DepfileError(f"dependency rule has no ':' separator: {line!r}")


def _words(value: str) -> list[str]:
    words: list[str] = []
    current: list[str] = []
    escaped = False
    for char in value:
        if escaped:
            current.append(char)
            escaped = False
        elif char == "\\":
            escaped = True
        elif char.isspace():
            if current:
                words.append("".join(current))
                current = []
        else:
            current.append(char)
    if escaped:
        current.append("\\")
    if current:
        words.append("".join(current))
    return words


def parse(text: str) -> dict[str, list[str]]:
    """Parse one or more depfile rules and merge repeated targets."""
    rules: dict[str, list[str]] = {}
    for line in _logical_lines(text):
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        target_text, dependency_text = _split_rule(stripped)
        targets = _words(target_text)
        dependencies = _words(dependency_text)
        if not targets:
            raise DepfileError("dependency rule has no target")
        for target in targets:
            values = rules.setdefault(target, [])
            for dependency in dependencies:
                if dependency not in values:
                    values.append(dependency)
    return rules


def parse_depfile(path: str) -> dict[str, list[str]]:
    return parse(Path(path).read_text(encoding="utf-8"))


__all__ = ["DepfileError", "parse", "parse_depfile"]
