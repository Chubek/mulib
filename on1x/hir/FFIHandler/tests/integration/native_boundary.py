#!/usr/bin/env python3
"""Run independent AMD64 Linux C caller/callee probes, not On1x admission tests."""

import argparse
import hashlib
import math
import os
from pathlib import Path
import platform
import random
import shlex
import shutil
import subprocess


HERE = Path(__file__).resolve().parent
CLAMP_NAMES = ("mulib_clamp_add_i64", "mulib_clamp_sub_i64", "mulib_clamp_i64")


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("work", type=Path, help="new directory for artifacts/logs")
    cli.add_argument("--include", type=Path, default=HERE.parents[4] / "include")
    cli.add_argument("--cc", required=True, help="explicit native linker and caller compiler")
    cli.add_argument("--other-cc", help="optional second native C compiler")
    cli.add_argument("--cephyr", type=Path, help="also exercise Cephyr objects")
    cli.add_argument("--sanitizers", action="store_true", help="instrument both host TUs and caller")
    a = cli.parse_args()
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        cli.error("executable probes require native x86_64 Linux")
    if a.sanitizers and (a.cephyr or a.other_cc):
        cli.error("run the host sanitizers independently of mixed compiler probes")
    include = a.include.resolve(strict=True)
    headers = ("mulib-clamp.h", "mulib-media-timebase.h", "mulib-media-core.h",
               "mulib-container-core.h")
    for header in headers:
        if not (include / header).is_file():
            cli.error(f"actual MuLIB header is missing: {include / header}")
    if a.cephyr:
        a.cephyr = a.cephyr.resolve(strict=True)
    root = a.work.resolve()
    root.mkdir(parents=True)
    tmp = root / "tmp"
    tmp.mkdir()
    config = root / "Cephyr.toml"
    config.write_text("[blackstone]\nenabled = false\n")
    env = {key: val for key, val in os.environ.items()
           if not key.startswith(("CEPHYR_", "LD_")) and key not in
           {"CPATH", "C_INCLUDE_PATH", "LIBRARY_PATH"}}
    env.update(TMPDIR=str(tmp), LC_ALL="C", ASAN_OPTIONS="detect_leaks=1:halt_on_error=1",
               UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")

    def run(tag, command, *, input=None, expected=0, diagnostic=None):
        argv = list(map(str, command))
        with (root / "commands.log").open("a") as log:
            print(f"{tag}: {shlex.join(argv)}", file=log)
        p = subprocess.run(argv, input=input, capture_output=True, text=True, timeout=120,
                           env=env, cwd=root)
        (root / f"{tag}.stdout").write_text(p.stdout)
        (root / f"{tag}.stderr").write_text(p.stderr)
        if (not (p.returncode > 0 if expected is None else p.returncode == expected)
                or diagnostic and diagnostic not in p.stderr):
            raise RuntimeError(f"{tag} returned {p.returncode}, see {root}; {p.stderr}")
        return p.stdout

    def host(cc, *args):
        return [cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                "-I", include, *san, *args]

    def compile_host(cc, source, output):
        run(output.stem, host(cc, "-c", HERE / source, "-o", output))

    def compile_cephyr(source, output, *options):
        run(output.stem, [a.cephyr, "--config", config, "--target=x86_64-linux-gnu",
                         "--stdlib=cephyr", "--no-factir", "-std=c11", "-I", include,
                         *options, "-c", HERE / source, "-o", output])

    def digest(path):
        return hashlib.sha256(path.read_bytes()).hexdigest()

    san = (["-g", "-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-fno-sanitize-recover=all"] if a.sanitizers else [])
    tools = [("host", a.cc), ("nm", "nm"), ("readelf", "readelf")]
    if a.other_cc:
        tools.append(("other", a.other_cc))
    if a.cephyr:
        tools.append(("cephyr", str(a.cephyr)))
    with (root / "identities.log").open("w") as identities:
        for name, tool in tools:
            found = shutil.which(tool)
            if found is None:
                cli.error(f"tool is unavailable: {tool}")
            path = Path(found).resolve()
            print(name, path, digest(path), file=identities)
            run(f"version-{name}", [tool, "--version"])
        for header in headers:
            print(header, digest(include / header), file=identities)

    lo, hi = -(2**63), 2**63 - 1
    edges = [lo, lo + 1, -2, -1, 0, 1, 2, hi - 1, hi]
    rng = random.Random(74191)
    signed = [(a, b, c) for a in edges for b in edges for c in edges if b <= c]
    for _ in range(500):
        x, y, z = [rng.randint(lo, hi) for _ in range(3)]
        signed.append((x, min(y, z), max(y, z)))
    signed_input = "".join(f"{x} {y} {z}\n" for x, y, z in signed)

    def saturate(x):
        return max(lo, min(hi, x))

    signed_expected = "".join(f"{saturate(x+y)} {saturate(x-y)} {max(y,min(z,x))}\n"
                              for x, y, z in signed)
    (root / "clamp-input.txt").write_text(signed_input)
    (root / "clamp-expected.txt").write_text(signed_expected)
    passed = []

    def check_clamp(tag, binary):
        got = run(tag, [binary], input=signed_input)
        if got != signed_expected:
            raise RuntimeError(f"{tag}: independent integer oracle mismatch")
        passed.append(f"{tag}: {len(signed) * 3} integer comparisons")

    def probe(tag, caller, implementation):
        exe = root / tag
        run(f"link-{tag}", host(a.cc, driver, caller, implementation, "-o", exe))
        check_clamp(tag, exe)

    driver = root / "driver.o"
    adapter, kernel = root / "host-boundary.o", root / "host-kernel.o"
    compile_host(a.cc, "oracle_driver.c", driver)
    compile_host(a.cc, "clamp_boundary.c", adapter)
    compile_host(a.cc, "clamp_impl.c", kernel)
    probe("host-host", adapter, kernel)
    run("wrong-same-width", host(a.cc, "-DQUALIFY_WRONG_PRIMITIVE", "-c",
                               HERE / "clamp_boundary.c", "-o", "wrong.o"),
        expected=None, diagnostic="clamp native prototype")
    if (root / "wrong.o").exists():
        raise RuntimeError("rejected prototype produced an object")
    run("missing-link-symbol", host(a.cc, driver, adapter, "-o", "missing"),
        expected=None, diagnostic="mulib_clamp_add_i64")
    if (root / "missing").exists():
        raise RuntimeError("rejected link produced an executable")
    passed.extend(("same-width wrong primitive rejected by compiler", "missing C symbol rejected at link"))

    host_inline = root / "host-inline"
    run("compile-inline", host(a.cc, HERE / "inline_driver.c", "-o", host_inline))
    unsigned_edges = [0, 1, 2, 3, 2**32, 2**63, 2**64 - 2, 2**64 - 1]
    unsigned = [(a, b) for a in unsigned_edges for b in unsigned_edges]
    unsigned.extend((rng.getrandbits(64), rng.getrandbits(64)) for _ in range(500))
    got = run("inline", [host_inline], input="".join(f"{x} {y}\n" for x, y in unsigned))
    if got != "".join(f"{math.gcd(x, y)} {min(x + y, 2**64 - 1)}\n" for x, y in unsigned):
        raise RuntimeError("inline functions disagree with independent oracles")
    passed.append(f"native header-only gcd and counter-add: {len(unsigned) * 2} unsigned comparisons")

    shared = root / "libw11clamp.so"
    run("build-dso", host(a.cc, "-fPIC", "-shared", HERE / "clamp_impl.c",
                          "-Wl,-soname," + shared.name, "-o", shared))
    symbols = run("dso-symbols", ["nm", "-D", "--defined-only", shared])
    if not set(CLAMP_NAMES) <= {line.split()[-1] for line in symbols.splitlines() if line.split()}:
        raise RuntimeError("expected clamp DSO exports missing")
    dso_exe = root / "caller-dso"
    run("link-dso", host(a.cc, driver, adapter, "-L", root, "-lw11clamp",
                         "-Wl,-rpath,$ORIGIN", "-o", dso_exe))
    tags = run("dso-linkage", ["readelf", "-d", dso_exe])
    if f"[{shared.name}]" not in tags or "[$ORIGIN]" not in tags:
        raise RuntimeError("not linked to adjacent DSO")
    (root / "dso.sha256").write_text(f"{digest(shared)}  {shared.name}\n")
    check_clamp("host-dso", dso_exe)
    # Test a relocated copy of the same executable WITHOUT its library.
    broken = root / "missing-dso"
    broken.mkdir()
    shutil.copy2(dso_exe, broken)
    run("load-missing-dso", [broken / dso_exe.name], expected=None, diagnostic=shared.name)
    passed.append("dynamic loader rejected missing adjacent artifact")

    if a.other_cc:
        other_adapter, other_kernel = root / "other-boundary.o", root / "other-kernel.o"
        compile_host(a.other_cc, "clamp_boundary.c", other_adapter)
        compile_host(a.other_cc, "clamp_impl.c", other_kernel)
        probe("host-other", adapter, other_kernel)
        probe("other-host", other_adapter, kernel)
    if a.cephyr:
        cephyr_adapter, cephyr_kernel = root / "cephyr-boundary.o", root / "cephyr-kernel.o"
        compile_cephyr("clamp_boundary.c", cephyr_adapter)
        compile_cephyr("clamp_impl.c", cephyr_kernel)
        probe("host-cephyr", adapter, cephyr_kernel)
        probe("cephyr-host", cephyr_adapter, kernel)
        probe("cephyr-cephyr", cephyr_adapter, cephyr_kernel)

    summary = "\n".join(passed) + "\n"
    (root / "summary.log").write_text(summary)
    print(summary, end="")


if __name__ == "__main__":
    main()
