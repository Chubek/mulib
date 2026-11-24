#!/usr/bin/env python3
"""Run checked On1x through the embedded object backend and an independent C11 compiler."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("on1xc", type=Path)
    parser.add_argument("cc")
    args = parser.parse_args()
    compiler = args.on1xc.resolve()
    root = Path(__file__).resolve().parents[1]
    runtime = root / "runtime"
    compilers = {Path(shutil.which(args.cc) or args.cc).resolve()}
    compilers.update(Path(path).resolve() for name in ("gcc", "clang")
                     if (path := shutil.which(name)))
    with tempfile.TemporaryDirectory(prefix="on1x-cephyr-test-") as tmp:
        work = Path(tmp)
        marker = work / "cephyr-was-invoked"
        trap = work / "cephyr"
        trap.write_text('#!/bin/sh\n: > "$ON1X_CEPHYR_TEST_MARKER"\nexit 99\n')
        trap.chmod(0o755)
        env = dict(os.environ, PATH=str(work) + os.pathsep + os.environ["PATH"],
                   ON1X_CEPHYR_TEST_MARKER=str(marker))

        def run(command, check=True):
            return subprocess.run(command, capture_output=True, check=check, env=env,
                                  timeout=120)

        def outcome(command):
            p = run(command, check=False)
            return p.returncode, p.stdout, p.stderr

        target = "x86_64-linux-gnu"

        def emit(source, output, triple=target, check=True, options=()):
            return run([compiler, "--emit-cephyr-object", "--cephyr-target", triple,
                        *options, source, "-o", output], check=check)

        tests = [(root / "examples/hello.on1x", (0, b"21\n", b""))]
        for name, expr, message in (
                ("overflow", "9223372036854775807 + n", "addition overflow"),
                ("division", "n / (n - 1)", "invalid division")):
            source = work / f"{name}.on1x"
            source.write_text(f"fn checked(n: i64) -> i64 {{ return {expr}; }}\n"
                              "fn main() { println(7); println(checked(1)); }\n")
            tests.append((source, (70, b"7\n", f"On1x arithmetic error: {message}\n".encode())))

        for source, expected in tests:
            obj, cref, exe = (work / (source.stem + ext) for ext in (".o", ".c", ".exe"))
            emit(source, obj)
            shortened = obj.with_suffix(".short.o")
            emit(source, shortened, options=("--cephyr-sandslab", "on"))
            optimized = obj.with_suffix(".blackstone.o")
            emit(source, optimized, options=("--cephyr-blackstone", "on"))
            elf = obj.read_bytes()
            assert elf[:6] == b"\x7fELF\x02\x01" and elf[16:20] == b"\x01\0\x3e\0", source
            run([compiler, "--emit-c", source, "-o", cref])
            for cc in sorted(compilers):
                for program in (cref, obj, shortened, optimized):
                    run([cc, "-std=c11", "-I", runtime, program, runtime / "on1x_runtime.c",
                         "-o", exe])
                    actual = outcome([exe])
                    assert actual == expected, (source, cc, program, actual, expected)

        sentinel = b"preserve existing output\n"
        obj.write_bytes(sentinel)
        bad = emit(tests[0][0], obj, triple="unsupported-test-target", check=False)
        assert bad.returncode and b"target" in bad.stderr.lower() and obj.read_bytes() == sentinel
        bad = emit(root / "examples/f64.on1x", obj, check=False)
        assert bad.returncode and b"unsupported On1x f64 target or runtime ABI" in bad.stderr, bad.stderr
        assert b"f64.on1x.c:" in bad.stderr and obj.read_bytes() == sentinel, bad.stderr
        bad = emit(root / "examples/async_timer.on1x", obj, check=False)
        assert bad.returncode and b"stackless timer frames require the direct C11 runtime profile" in bad.stderr, bad.stderr
        assert obj.read_bytes() == sentinel
        output_dir = work / "nonempty"
        output_dir.mkdir()
        retained = output_dir / "file"
        retained.write_bytes(sentinel)
        bad = emit(tests[0][0], output_dir, check=False)
        assert bad.returncode and bad.stderr and retained.read_bytes() == sentinel
        assert not marker.exists(), "the adapter invoked the Cephyr executable on PATH"


if __name__ == "__main__":
    main()
