#!/usr/bin/env python3
"""Compile small, representative PDFs and reject typography regressions."""
from pathlib import Path
import re
import shutil
import subprocess

from book_support import fragment

HERE = Path(__file__).resolve().parent
OUTPUT = HERE / "build/typography"


def run(*command):
    result = subprocess.run(command, cwd=HERE, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"{' '.join(command)} failed:\n{result.stdout[-12000:]}")
    return result.stdout


def main():
    for program in ("latexmk", "lualatex", "pdftotext"):
        if not shutil.which(program):
            raise SystemExit(f"Typography checks require {program}")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    table = "| Reference field | Contract |\n| --- | --- |\n"
    table += "".join(f"| Field {i} | A stable, documented interface for both host languages. |\n"
                     for i in range(1, 71))
    (OUTPUT / "table.tex").write_text(fragment(table))
    # Stress the same generated inline-code/path constructs as the full reference.
    prose = (
        "Normalization, inverse, look-at, projection and quaternion conversion/interpolation "
        "return **1 on success, 0 on failure**, preserving the output on failure. "
        "A NULL output fails. Passing a value and its original output variable is supported.\n\n"
        "- SETTINGS_ENABLE_PUSH (2), SETTINGS_ENABLE_CONNECT_PROTOCOL (8) and "
        "SETTINGS_NO_RFC7540_PRIORITIES (9) must be 0 or 1. INITIAL_WINDOW_SIZE (4) "
        "is at most 2^31-1; MAX_FRAME_SIZE (5) is in 16384..16777215.\n\n"
        "The following signature is a literal identifier, not executable typesetting.\n\n"
        "`mulib_linker_relocate(linker, name, field_offset, kind, addend)` records:\n"
    )
    (OUTPUT / "prose.tex").write_text(fragment(prose))
    (OUTPUT / "long.cpp").write_text("".join(
        f"int step_{i}(int value) {{ return value + {i}; }}\n" for i in range(1, 121)))
    for profile, prefix in (("screen", ""), ("legacy-print", r"\def\MuSmokeLegacy{1}")):
        source = OUTPUT / f"{profile}.tex"
        source.write_text(prefix + "\n" + r"\input{support/typography-smoke.ltx}" + "\n")
        run("latexmk", "-lualatex", "-shell-escape", "-interaction=nonstopmode",
            "-halt-on-error", "-outdir=build/typography", str(source.relative_to(HERE)))
        log = source.with_suffix(".log").read_text()
        problems = re.findall(
            r"[^\n]*(?:Overfull \\[hv]box|Missing character:|undefined references|"
            r"LaTeX Font Warning|Token not allowed in a PDF string|"
            r"destination with the same identifier|duplicate destination|"
            r"unreferenced destination|Package fancyhdr Warning)[^\n]*", log)
        if problems:
            raise RuntimeError(f"{profile} typography regressions:\n" + "\n".join(problems))
        text = run("pdftotext", "-layout", str(source.with_suffix(".pdf")), "-")
        if text.count("Reference field") < 2 or "Field 70" not in text:
            raise RuntimeError(f"{profile}: multipage table lost its repeated header or last row")
        if "step_120" not in text or "??" in text:
            raise RuntimeError(f"{profile}: missing listing text or unresolved cross-reference")
        if '#include "MuDSL.hpp"' not in text or '"and"' not in text:
            raise RuntimeError(f"{profile}: code quotes were changed by typographic ligatures")
        if '##include' in text:
            raise RuntimeError(f"{profile}: a literal API hash was doubled")
        print(f"{profile}: typography checks passed ({source.with_suffix('.pdf').relative_to(HERE)})")


if __name__ == "__main__":
    main()
