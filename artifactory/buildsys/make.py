from pathlib import Path

from ._adapter import parse_lines


def parse(text):
    return parse_lines(text)


def parse_makefile(path: str):
    return parse(Path(path).read_text(encoding="utf-8"))


def dependencies(text):
    return parse(text)


__all__ = ["dependencies", "parse", "parse_makefile"]
