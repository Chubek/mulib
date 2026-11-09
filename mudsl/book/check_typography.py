#!/usr/bin/env python3
"""Compile small, representative PDFs and reject typography regressions."""
from pathlib import Path
import importlib.util
import re
import shutil
import struct
import subprocess
import zlib

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
    # A miniature manual collection exercises the real generator and its scoped
    # reference style, rather than a separately maintained imitation.
    manuals = OUTPUT / "manual-sources"
    manuals.mkdir(exist_ok=True)
    # A small original raster also catches confusion between TeX's working
    # directory and the PDF output directory when rebasing Markdown images.
    def png_chunk(kind, payload):
        return (struct.pack('>I', len(payload)) + kind + payload
                + struct.pack('>I', zlib.crc32(kind + payload)))
    raster = b''.join(b'\x00' + b'\x32\x68\x7a' * 160 for _ in range(8))
    (manuals / 'reference_strip.png').write_bytes(
        b'\x89PNG\r\n\x1a\n'
        + png_chunk(b'IHDR', struct.pack('>IIBBBBB', 160, 8, 8, 2, 0, 0, 0))
        + png_chunk(b'IDAT', zlib.compress(raster)) + png_chunk(b'IEND', b''))
    (manuals / "README.md").write_text(
        '# Manual guide\n\n[Contract](mulib-specimen.md#contract) and '
        '[repeated heading](mulib-specimen.md#contract-1) are linked inside this PDF.\n\n'
        '![Reference strip](reference_strip.png)\n')
    (manuals / "mulib-specimen.md").write_text(
        '# MuLIB specimen\n\n[Manuals](README.md)\n\n## Contract\n\n'
        'Identifiers such as `mulib_averylongidentifierwithmanyletters_suffix` '
        'prefer their natural punctuation boundaries.\n\n'
        '<!-- generated marker, not prose -->\n<details>\n'
        '<summary>Types, constants, and declarations</summary>\n\n'
        '```c\nconst char *literal = "<details> <!-- still code -->";\n```\n\n'
        '</details>\n\n## Long signatures\n\n'
        'Zero initialize `mulib_cmachine_t`. Call '
        '`mulib_cmachine_lower(program, arguments, argument_count, record_limit, code_limit, &machine, diagnostic)`. '
        'Output must be empty. The entire MIR is validated before allocation; every opcode '
        'in `manual/mulib-mir.md` is supported. The supplied argument count must exactly '
        'match `parameter_count`. A nonempty argument array cannot be NULL. Every BOOL PARAM '
        'argument is preflight checked for 0/1 before any VM arithmetic. Invalid arguments '
        'return INVALID/BAD_TYPE.\n\n'
        '- `mulib_minicurl_send(url, method, content_type, body, length, options, transport, user, response)` '
        'sends an explicit method and binary body.\n\n'
        '`mulib_minincat_relay(read_fn, reader, write_fn, writer, buffer, capacity, max_bytes, transferred)` '
        'borrows all pointers. A non-NULL scratch buffer, positive capacity, both functions '
        'and non-NULL `transferred` are required. Reader and writer contexts can be NULL '
        'if the callbacks permit it.\n\n'
        '- `SELECT [DISTINCT] expressions-or-* [FROM one-table] [WHERE expression] '
        '[ORDER BY expressions [ASC|DESC], ...] [LIMIT integer [OFFSET integer]]`. '
        'Result aliases use AS. ORDER BY accepts aliases and one-based output ordinals.\n\n'
        '## Contract\n\n' + table)
    spec = importlib.util.spec_from_file_location("manual_generator", HERE / "typeset-manuals.py")
    generator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(generator)
    generator.generate(manuals, OUTPUT / "manual-reference.tex")
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
        if 'MuLIB specimen' not in text or 'generated marker, not prose' in text:
            raise RuntimeError(f"{profile}: manual title/disclosure rendering regressed")
        if not re.search(r'APPENDIX\s+A', text):
            raise RuntimeError(f"{profile}: the manual style leaked into source appendices")
        print(f"{profile}: typography checks passed ({source.with_suffix('.pdf').relative_to(HERE)})")


if __name__ == "__main__":
    main()
