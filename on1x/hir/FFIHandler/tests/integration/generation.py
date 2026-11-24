#!/usr/bin/env python3
"""Check real clamp and inline generation transactions without changing either checkout."""

import argparse
from hashlib import sha256
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys


ANALYSIS_REVISION = "6a7d52e866db8eb441bb7a4f86ce91253732e740"


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("work", type=Path, help="new directory for all outputs and test mutations")
    cli.add_argument("--mulib-root", type=Path, required=True)
    cli.add_argument("--other-mulib-root", type=Path, required=True,
                     help="independent checkout of the same commit")
    cli.add_argument("--cephyr", type=Path, required=True)
    cli.add_argument("--libstone-root", type=Path, required=True)
    cli.add_argument("--parser", type=Path, required=True, help="genuine generated LLSynth parser")
    cli.add_argument("--other-parser", type=Path,
                     help="independently generated genuine parser for the second checkout")
    cli.add_argument("--tool-input", type=Path, action="append", required=True)
    a = cli.parse_args()
    repo, other = (p.resolve(strict=True) for p in (a.mulib_root, a.other_mulib_root))
    if repo == other:
        cli.error("checkout location must actually differ")
    work = a.work.resolve()
    work.mkdir(parents=True)
    tmp = work / "tmp"
    tmp.mkdir()
    env = {**os.environ, "LC_ALL": "C", "TMPDIR": str(tmp), "PYTHONDONTWRITEBYTECODE": "1"}
    pinned = a.libstone_root.resolve(strict=True)

    def git(path, *args):
        return subprocess.check_output(["git", "-C", path, *args], text=True,
                                       timeout=15, env=env).strip()

    version = git(repo, "rev-parse", "HEAD")
    if version != git(other, "rev-parse", "HEAD"):
        cli.error("checkout revisions must match")
    revision = git(pinned, "rev-parse", "HEAD")
    (work / "identities.log").write_text(
        f"MuLIB {version}  LibStone {revision}\n"
        f"{repo}\n{git(repo, 'status', '--short')}\n"
        f"{other}\n{git(other, 'status', '--short')}\n"
        + "".join(f"LLSynth parser {sha256(p.read_bytes()).hexdigest()}  {p.resolve()}\n"
                  for p in (a.parser, a.other_parser or a.parser)))
    include = pinned / "cornerstone/Cephyr/stdlib/include"
    controlled = work / "cephyr-headers"
    shutil.copytree(include, controlled)
    stdint = controlled / "stdint.h"
    if not stdint.is_file():
        cli.error("no stdint.h to serve as the real clamp transitive include control")

    def generate(tag, root, output, *flags, kernel="clamp", fails_with=None):
        generator = root / "on1x/hir/FFIHandler/tools/ffgen.py"
        if not generator.is_file():
            raise RuntimeError(f"{tag}: no integrated generator in {root}")
        generated_parser = a.other_parser if root == other and a.other_parser else a.parser
        argv = [sys.executable, "-B", str(generator), "--mulib-root", str(root),
                "--cephyr", str(a.cephyr.resolve(strict=True)), "--libstone-revision", revision,
                "--target", "x86_64-linux-gnu", "--parser", str(generated_parser.resolve(strict=True)),
                "--scratch-root", str(tmp), "--system-include-dir", str(controlled),
                "--kernel", kernel, "--output", str(output), *flags]
        for path in a.tool_input:
            argv += ["--tool-input", str(path.resolve(strict=True))]
        with (work / "commands.log").open("a") as log:
            print(f"{tag}: {shlex.join(argv)}", file=log)
        p = subprocess.run(argv, capture_output=True, timeout=180, env=env, cwd=work)
        (work / (tag + ".stdout")).write_bytes(p.stdout)
        (work / (tag + ".stderr")).write_bytes(p.stderr)
        if p.returncode != (0 if fails_with is None else 1) or (
                fails_with is not None and fails_with not in p.stderr):
            raise RuntimeError(f"{tag}: exit {p.returncode}, see {work / (tag + '.stderr')}")

    def snapshot(path):
        return {f.name: (f.read_bytes(), f.stat().st_mtime_ns) for f in path.iterdir()}

    output, relocated, changed = (work / p for p in ("first", "relocated", "changed"))
    generate("initial", repo, output)
    before = snapshot(output)
    if set(before) != {"clamp.ffi", "index.json"}:
        raise RuntimeError("not an exact single-module output")
    index = json.loads(before["index.json"][0])
    row, = index["modules"]
    if (row["key"] != "clamp" or row["projection_sha256"] is None
            or row["ffi_sha256"] != sha256(before["clamp.ffi"][0]).hexdigest()):
        raise RuntimeError("clamp projection and generated DSL identities missing")
    if revision == ANALYSIS_REVISION:
        if (row["projection"].get("digest_kind") != "on1x.cephyr-c11-interface-analysis/v1"
                or row["c11_analysis"]["source"] != "cephyr_c11_analysis_v1"
                or b"analysis cephyr_c11_analysis_v1" not in before["clamp.ffi"][0]
                or any(str(root).encode() in value[0] for root in (repo, other) for value in before.values())):
            raise RuntimeError("extended analysis report or relocatable identity missing")
    interval = next(s for s in row["symbols"] if s["symbol"] == "mulib_clamp_i64")
    if interval["status"] != "representable" or interval["guards"] != [[1, 2]]:
        raise RuntimeError("clamp lost the position-bound override")
    generate("fresh", repo, output, "--check")
    if snapshot(output) != before:
        raise RuntimeError("successful --check wrote to its destination")
    generate("independent-location", other, relocated)
    if {k: val[0] for k, val in before.items()} != {
            k: val[0] for k, val in snapshot(relocated).items()}:
        raise RuntimeError("generation was not byte-identical across checkout locations")
    generate("independent-location-check", other, output, "--check")
    if snapshot(output) != before:
        raise RuntimeError("relocated --check wrote to the original destination")

    first_package, other_package = (work / name for name in ("package", "relocated-package"))
    shutil.copytree(output, first_package)
    shutil.copytree(relocated, other_package)
    for root, dest in ((repo, first_package), (other, other_package)):
        generate(f"{dest.name}-add-inline", root, dest, kernel="container-core")
        saved = snapshot(dest)
        if set(saved) != {"clamp.ffi", "container-core.ffi", "index.json"}:
            raise RuntimeError("incremental publication did not retain exactly the two modules")
        row = next(m for m in json.loads(saved["index.json"][0])["modules"] if m["key"] == "container-core")
        counter = next(s for s in row["symbols"] if s["symbol"] == "mulib_ct_counter_add")
        if (counter["status"] != "representable" or not row["projection_sha256"]
                or row["ffi_sha256"] != sha256(saved["container-core.ffi"][0]).hexdigest()
                or saved["clamp.ffi"][0] != before["clamp.ffi"][0]):
            raise RuntimeError("inline projection, generated DSL or retained clamp identity does not match")
        for kernel in ("clamp", "container-core"):
            generate(f"{dest.name}-{kernel}-fresh", root, dest, "--check", kernel=kernel)
        if snapshot(dest) != saved:
            raise RuntimeError("incremental --check wrote to its destination")
    if {name: value[0] for name, value in snapshot(first_package).items()} != {
            name: value[0] for name, value in snapshot(other_package).items()}:
        raise RuntimeError("two-module package differs between checkout locations")

    # An unexpected generated file is not allowed to disappear during --check.
    (relocated / "unregistered.ffi").write_bytes(before["clamp.ffi"][0])
    stale = snapshot(relocated)
    generate("unexpected-file", other, relocated, "--check", fails_with=b"unregistered.ffi")
    if snapshot(relocated) != stale:
        raise RuntimeError("failed --check rewrote its destination")

    overrides = work / "private-overrides"
    overrides.mkdir()
    no_selection = work / "no-selection"
    generate("without-origin-selection", repo, no_selection, "--overrides", str(overrides))
    row_without, = json.loads((no_selection / "index.json").read_bytes())["modules"]
    symbols = [s for s in row_without["symbols"] if s["symbol"].startswith("mulib_clamp_")]
    if len(symbols) != 3 or any(s["reason"] != "ambiguous_export" for s in symbols):
        raise RuntimeError("prefix alone was treated as ownership")
    authored = (repo / "on1x/stdffi/overrides/mulib/clamp.ffi").read_bytes()
    bad, n = re.subn(rb'(?<=fingerprint ")[a-f0-9]{64}(?=")', b"0" * 64, authored)
    if n != 1 or bad == authored:
        raise RuntimeError("missing unique native signature in authored override")
    (overrides / "clamp.ffi").write_bytes(bad)
    generate("stale-override", repo, output, "--overrides", str(overrides),
             fails_with=b"stale native signature")
    if snapshot(output) != before:
        raise RuntimeError("failed override modified previously published output")

    # This mutation is confined to a private copy of the actual bundled header.
    # A declaration-neutral change must still invalidate the recorded inputs.
    with stdint.open("ab") as out:
        out.write(b"\n/* W11 transitive-provenance negative control */\n")
    generate("transitive-drift-check", repo, output, "--check", fails_with=b"index.json")
    if snapshot(output) != before:
        raise RuntimeError("drift --check rewrote known-good output")
    generate("transitive-regenerate", repo, changed)
    after = snapshot(changed)
    if (after["index.json"][0] == before["index.json"][0] or
            after["clamp.ffi"][0] != before["clamp.ffi"][0]):
        raise RuntimeError("a declaration-neutral header change must change provenance, not the DSL")
    print("PASS actual clamp and inline generated with LLSynth validation, relocation, and read-only drift checks")


if __name__ == "__main__":
    main()
