#!/usr/bin/env python3
"""Compile every SDK header independently and the C umbrella as one unit."""

from pathlib import Path
import subprocess
import sys


STRICT = ("-Wall", "-Wextra", "-Wpedantic", "-Werror")


def run(arguments: list[str]) -> None:
    subprocess.run(arguments, check=True, timeout=60)


def compile_header(
    compiler: str,
    standard: str,
    language: str,
    include: Path,
    source: Path,
    header: str,
) -> None:
    run([
        compiler,
        f"-std={standard}",
        *STRICT,
        f"-I{include}",
        "-include",
        header,
        "-x",
        language,
        "-fsyntax-only",
        str(source),
    ])


def main() -> int:
    if len(sys.argv) != 6:
        print(
            "usage: check_aggregate_headers.py CC CXX SDK_INCLUDE RT_INCLUDE WORK_DIR",
            file=sys.stderr,
        )
        return 2
    cc, cxx = sys.argv[1:3]
    include = Path(sys.argv[3]).resolve(strict=True)
    runtime_include = Path(sys.argv[4]).resolve(strict=True)
    work = Path(sys.argv[5]).resolve()
    work.mkdir(parents=True, exist_ok=True)
    empty_c = work / "empty.c"
    empty_cpp = work / "empty.cpp"
    empty_c.write_text("int on1x_sdk_header_c_probe(void) { return 0; }\n",
                       encoding="utf-8")
    empty_cpp.write_text("int on1x_sdk_header_cpp_probe() { return 0; }\n",
                         encoding="utf-8")

    headers = sorted(path.name for path in include.glob("*.h"))
    expected = set(headers) - {"On1xSDK.h"}
    umbrella = (include / "On1xSDK.h").read_text(encoding="utf-8")
    included = {
        line.split('"')[1]
        for line in umbrella.splitlines()
        if line.startswith('#include "On1xSDK-')
    }
    if included != expected:
        missing = ", ".join(sorted(expected - included))
        extra = ", ".join(sorted(included - expected))
        raise RuntimeError(
            f"On1xSDK.h inventory mismatch; missing=[{missing}], extra=[{extra}]"
        )

    runtime_headers = sorted(path.name for path in runtime_include.glob("*.h"))
    if "On1xRT.h" not in runtime_headers:
        raise RuntimeError("missing On1xRT.h")
    for directory, inventory in ((include, headers), (runtime_include, runtime_headers)):
        for header in inventory:
            compile_header(cc, "c11", "c", directory, empty_c, header)
            compile_header(cxx, "c++17", "c++", directory, empty_cpp, header)
    for header in sorted(path.name for path in include.glob("*.hpp")):
        compile_header(cxx, "c++17", "c++", include, empty_cpp, header)

    print(
        f"{len(headers)} SDK and {len(runtime_headers)} runtime C headers "
        "passed independent C11/C++17 compilation; "
        "the aggregate umbrella includes the complete inventory"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"aggregate header check failed: {error}", file=sys.stderr)
        raise SystemExit(1)
