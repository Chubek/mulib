#!/usr/bin/env python3
"""Compile and execute emitted owner MIR C11 and an independent C11 reference."""
import argparse
import os
from pathlib import Path
import subprocess


def run(*command):
    return subprocess.run(command, capture_output=True, timeout=40, check=False)


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def successful(*command):
    result = run(*command)
    check(result.returncode == 0,
          f"{result.args!r}: {result.returncode}\n{result.stdout!r}\n{result.stderr!r}")
    return result


def outcome(result):
    return result.returncode, result.stdout, result.stderr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("cc")
    parser.add_argument("runtime", type=Path)
    parser.add_argument("work", type=Path)
    parser.add_argument("--cflag", action="append", default=[])
    args = parser.parse_args()
    fixtures = Path(__file__).resolve().parent
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    tmp = work / "tmp"
    tmp.mkdir(exist_ok=True)
    os.environ["TMPDIR"] = str(tmp)

    result = successful(args.fixture.resolve(), work)
    print(result.stdout.decode(), end="")
    options = (args.cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
               "-pedantic-errors", *args.cflag)
    reference = work / "reference"
    successful(*options, fixtures / "mir_nominal_owner_reference.c", "-o", reference)
    for name in ("plain", "traced", "trapped"):
        successful(*options, "-I", fixtures.parent / "runtime", work / f"{name}.c",
                   args.runtime.resolve(), "-o", work / name)

    expected = outcome(run(reference))
    check(expected[:2] == (34, b"42\n34\n"), f"reference outcome drift: {expected!r}")
    check(expected == (int((work / "eval.status").read_bytes()),
                       (work / "eval.stdout").read_bytes(), (work / "eval.trace").read_bytes()),
          "evaluator result or lifecycle trace differs from the direct C11 oracle")
    actual_traced = outcome(run(work / "traced"))
    check(actual_traced == expected, f"traced C11 differs from oracle: {actual_traced!r}")
    actual_plain = outcome(run(work / "plain"))
    check(actual_plain == (*expected[:2], b""), f"plain C11 differs from oracle: {actual_plain!r}")

    trapped = outcome(run(reference, "trap"))
    check(trapped == (70, (work / "trap-eval.stdout").read_bytes(),
                      (work / "trap-eval.trace").read_bytes() + b"On1x assertion failed\n"),
          f"reference post-drop trap drift: {trapped!r}")
    actual_trapped = outcome(run(work / "trapped"))
    check(actual_trapped == trapped, f"emitted C11 post-drop trap differs from oracle: {actual_trapped!r}")
    print("computed stdout (both normal/trap):", repr(expected[1]))
    print("normal return: 34; post-drop trap: 70; untraced native stderr: empty")
    print(expected[2].decode(), end="")


if __name__ == "__main__":
    main()
