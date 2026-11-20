#!/usr/bin/env python3
"""Compile and execute CAPASM-emitted source as ISO C90."""
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--example", required=True, type=lambda s: Path(s).resolve())
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--sanitize", choices=("0", "1"), default="0")
    args = parser.parse_args()
    flags = ["-std=c90", "-pedantic-errors", "-Wall", "-Wextra", "-Werror"]
    if args.sanitize == "1":
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]

    with tempfile.TemporaryDirectory(prefix="mulib-capasm-") as directory:
        work = Path(directory)
        source, harness, executable = (work / name for name in ("out.c", "test.c", "test"))
        source.write_bytes(subprocess.check_output([args.example], timeout=15))
        harness.write_text(
            "extern unsigned long portable_answer(void);\n"
            "int main(void) { return portable_answer() == 42UL ? 0 : 1; }\n",
            encoding="ascii",
        )
        command = shlex.split(args.cc) + flags + [source, harness, "-o", executable]
        subprocess.run(command, check=True, timeout=45)
        subprocess.run([executable], check=True, timeout=15)
    print("CAPASM scalar output passed strict C90 compilation and execution.")


if __name__ == "__main__":
    main()
