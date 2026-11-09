#!/usr/bin/env python3
"""Stage a local Pygments plugin, without modifying the Python installation."""
from pathlib import Path
from book_support import write_output

HERE = Path(__file__).resolve().parent


def main():
    target = HERE / "build/python"
    write_output(target / "pygments_mull/__init__.py", (HERE / "pygments_mull/__init__.py").read_text())
    info = target / "pygments_mull-1.0.dist-info"
    write_output(info / "METADATA", "Metadata-Version: 2.1\nName: pygments-mull\nVersion: 1.0\n")
    write_output(info / "entry_points.txt", "[pygments.lexers]\nmull = pygments_mull:MuLLLexer\n")


if __name__ == "__main__":
    main()
