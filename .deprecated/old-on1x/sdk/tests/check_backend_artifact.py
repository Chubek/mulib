#!/usr/bin/env python3
import pathlib
import subprocess
import sys


def run(command, expected=0):
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != expected:
        raise SystemExit(
            f"command returned {result.returncode}, expected {expected}: {command!r}\n"
            f"stdout:\n{result.stdout.decode(errors='replace')}\n"
            f"stderr:\n{result.stderr.decode(errors='replace')}"
        )


def main():
    if len(sys.argv) != 5:
        raise SystemExit("usage: check_backend_artifact.py EMITTER CC ON1X_ROOT WORK")
    emitter, cc, root_text, work_text = sys.argv[1:]
    root = pathlib.Path(root_text)
    work = pathlib.Path(work_text)
    work.mkdir(parents=True, exist_ok=True)
    first = work / "first.c"
    second = work / "second.c"
    executable = work / "artifact-program"
    run([emitter, str(first)])
    run([emitter, str(second)])
    if first.read_bytes() != second.read_bytes():
        raise SystemExit("deterministic mir-c11 backend produced different bytes")
    run([cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
         "-I", str(root / "runtime"), str(first),
         str(root / "runtime" / "on1x_runtime.c"), "-o", str(executable)])
    run([str(executable)], expected=42)


if __name__ == "__main__":
    main()
