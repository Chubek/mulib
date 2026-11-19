#!/usr/bin/env python3
"""Compile the Linux notebook desktop headers and link a device-free C11 test."""

import argparse
from collections import Counter
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
FIXTURES = ROOT / "tests" / "notebook-desktop-headers"
PROFILE = "notebook-desktop"
PROFILE_SIZE = 83
INCLUDES = re.compile(r'^\s*#\s*include\s*["<]mulib-([^">]+)\.h[">]', re.M)
SWITCH = re.compile(r'^#\s*if(?:def\s+|\s+defined\s*\(\s*)MULIB_IMPL_', re.M)


def inventory(include):
    manifest = (ROOT / "MANIFEST.yaml").read_text().split("\nproject_files:\n", 1)[0]
    headers = []
    paths = Counter(re.findall(r"^  header: (.+)$", manifest, re.M))
    for block in re.split(r"^- name: ", manifest, flags=re.M)[1:]:
        name = block.splitlines()[0]
        fields = dict(re.findall(r"^  (\w+): (.+)$", block, re.M))
        if fields.get("profile") != PROFILE:
            continue
        filename = f"mulib-{name}.h"
        if (not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", name)
                or fields.get("header") != f"include/{filename}"
                or paths[fields["header"]] != 1):
            raise ValueError(f"invalid or duplicate desktop module: {name}")
        if fields.get("status") != "bounded" or fields.get("implementation") != "static-inline":
            raise ValueError(f"{filename}: expected bounded static-inline metadata")
        for key in ("manual", "example"):
            if not fields.get(key) or not (ROOT / fields[key]).is_file():
                raise ValueError(f"{filename}: invalid {key} path in manifest")
        text = (include / filename).read_text()
        if SWITCH.search(text):
            raise ValueError(f"{filename}: a header-only module cannot enable an ABI switch")
        dependencies = sorted(set(INCLUDES.findall(text)))
        if fields.get("dependencies") != f"[{', '.join(dependencies)}]":
            raise ValueError(f"{filename}: dependencies differ from header includes")
        headers.append(filename)
    if len(headers) != PROFILE_SIZE:
        raise ValueError(f"expected {PROFILE_SIZE} desktop headers, found {len(headers)}")
    return sorted(headers)


def check_compiler(compiler, include, headers, build):
    base = shlex.split(compiler) + [
        "-std=c11", "-D_GNU_SOURCE", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
        "-Wmisleading-indentation", "-I", str(include),
    ]
    standalone = build / "independent.c"
    aggregate = build / "combined.h"
    for name in headers:
        # A normal C TU, not a PCH: repeat inclusion to test public guards.
        standalone.write_text(f'#include "{name}"\n' * 2 + "int main(void) { return 0; }\n")
        subprocess.run(base + ["-fsyntax-only", str(standalone)], check=True)

    aggregate.write_text("".join(f'#include "{name}"\n' for name in headers))
    # Force every desktop header into both TUs and pass an actual owned
    # notebook between them so this is more than a declaration-only link.
    executable = build / "notebook-desktop-link"
    link = ["-lm", "-ldl"]
    if os.environ.get("SANITIZE") == "1":
        base += [
            "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
            "-fno-omit-frame-pointer",
        ]
    objects = []
    for unit in ("consumer", "peer"):
        obj = str(build / (unit + ".o"))
        flags = ["-DMULIB_IMPL_UTF8"] if unit == "consumer" else []
        subprocess.run(base + flags + [
            "-include", str(aggregate), "-DMULIB_NOTEBOOK_DESKTOP_TEST_PEER",
            "-c", str(FIXTURES / (unit + ".c")), "-o", obj,
        ], check=True)
        objects.append(obj)
    subprocess.run(base + objects + link + ["-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
    for name in ("test-jupyter-envelope", "test-notebook-pty"):
        executable = build / name
        subprocess.run(base + [str(ROOT / "tests" / (name + ".c"))] + link +
                       ["-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
    print(f"PASS: {compiler}: {len(headers)} independent C11 headers and aggregate two-TU link/run; "
          "Jupyter envelope and PTY regressions ran")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", action="append", help="C compiler command (default: gcc and clang)")
    parser.add_argument("--include", type=Path, default=ROOT / "include",
                        help="header directory (can be a staged install)")
    args = parser.parse_args()
    try:
        include = args.include.resolve()
        headers = inventory(include)
        parent = os.environ.get("TMPDIR")
        if not parent and Path("/tmp/opencode").is_dir():
            parent = "/tmp/opencode"
        with tempfile.TemporaryDirectory(prefix="mulib-notebook-headers-", dir=parent) as tmp:
            for compiler in args.cc or ["gcc", "clang"]:
                check_compiler(compiler, include, headers, Path(tmp))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Notebook desktop header gate: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
