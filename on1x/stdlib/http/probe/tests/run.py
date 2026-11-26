#!/usr/bin/env python3
"""Check Probe's C11 callback flow, optional adapter, and real process dispatch."""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile


TESTS = Path(__file__).resolve().parent
PROBE = TESTS.parent
STDLIB = PROBE.parents[1]
ON1X = STDLIB.parent
ROOT = ON1X.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--on1xc", type=Path, required=True)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--work-root", type=Path, default=ROOT / "build")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    compiler = args.on1xc.resolve()
    args.work_root.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="probe-test-", dir=args.work_root.resolve()) as tmp:
        work = Path(tmp)
        environment = dict(os.environ, TMPDIR=str(work))

        def run(*argv, capture=False):
            return subprocess.run(argv, check=True, text=True, capture_output=capture,
                                  env=environment, timeout=60)

        def build(name, inputs, *, strict=True, includes=(), libraries=()):
            binary = work / name
            flags = ["-std=c11", "-O1", "-g", "-UNDEBUG"]
            if strict:
                flags += ["-Wall", "-Wextra", "-Wpedantic", "-Werror"]
            if args.sanitize:
                flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
            run(args.cc, *flags, *(f"-I{x}" for x in includes), *inputs,
                *libraries, "-o", binary)
            return binary

        def typed(name, *extra):
            c_path = work / f"{name}.c"
            run(compiler, "--emit-c", TESTS / f"{name}.on1x", "-o", c_path)
            return build(
                name,
                [c_path, ON1X / "runtime/on1x_runtime.c", STDLIB / "elvi/bridge.c",
                 STDLIB / "elvi/runtime.c", STDLIB / "html/runtime.c",
                 TESTS / "execution_kernels.c", *extra],
                strict=False, includes=(ON1X / "runtime", ROOT / "kernels", STDLIB / "elvi"),
                libraries=("-lm",),
            )

        context = build("context", [PROBE / "context.c", TESTS / "context.c"])
        run(context)

        supervised = build(
            "supervised",
            [PROBE / "context.c", PROBE / "supervised.c", TESTS / "supervised.c",
             TESTS / "job_kernels.c", STDLIB / "daemon/supervisor.c", STDLIB / "kron/runtime.c"],
            includes=(ROOT / "kernels",), libraries=("-pthread", "-ldl", "-lm"),
        )
        state_dir = work / "state"
        state_dir.mkdir(mode=0o700)
        run(supervised, state_dir)

        sequence = [1, 2, 3, 4] * 4 + [10, 11, 1]
        for case in range(12, 18):
            sequence.extend((case, 1, 2))
        sequence += [18, 1, 2, 3, 19, 1, 2]
        sequence += [20, 21, 5, 22, 5, 1, 2, 3, 6, 4, 23, 5, 1, 2, 3, 6, 24, 5, 1, 2, 3]
        sequence += [30, 31, 7, 1, 2, 3, 4, 7, 1, 32, 1, 2, 3, 4, 33, 1, 2, 3, 4]
        got = run(typed("execution"), capture=True).stdout.splitlines()
        if got != list(map(str, sequence)):
            raise AssertionError(f"unexpected Probe callback sequence: {got}")

        live = typed("live", STDLIB / "elvi/capture.c", STDLIB / "elvi/live.c",
                     STDLIB / "elvi/tests/live_host.c")
        run(live)
    print("Probe: canonical HTML, callback gates, native dispatch, and link-only live adapter PASS")


if __name__ == "__main__":
    main()
