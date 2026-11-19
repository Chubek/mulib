#!/usr/bin/env python3
import pathlib
import subprocess
import sys


def run(command):
    return subprocess.run(command, check=True, text=True, capture_output=True)


def main():
    test, cc, runtime_dir, output_dir = sys.argv[1:]
    output = pathlib.Path(output_dir)
    output.mkdir(parents=True, exist_ok=True)
    before_c = output / "before.c"
    after_c = output / "after.c"
    run([test, "--emit-c", str(before_c), str(after_c)])
    executables = []
    for name, source in (("before", before_c), ("after", after_c)):
        executable = output / name
        run([
            cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
            "-I", runtime_dir, str(source),
            str(pathlib.Path(runtime_dir) / "on1x_runtime.c"),
            "-o", str(executable),
        ])
        executables.append(executable)
    before = run([str(executables[0])])
    after = run([str(executables[1])])
    if before.returncode != after.returncode or before.stdout != after.stdout:
        raise SystemExit("C11 before/after observations differ")
    expected = "2\n4\n6\n7\n14\n"
    if before.stdout != expected:
        raise SystemExit(f"unexpected C11 trace: {before.stdout!r}")
    print("SDK pass C11 before/after equivalence PASS")


if __name__ == "__main__":
    main()
