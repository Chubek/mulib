#!/usr/bin/env python3
"""Compile every backend/toolchain public header independently as C11/C++20."""

import pathlib
import subprocess
import sys


HEADERS = [
    "On1xSDK-AOT.h",
    "On1xSDK-Artifact.h",
    "On1xSDK-Backend.h",
    "On1xSDK-CallingConvention.h",
    "On1xSDK-DataLayout.h",
    "On1xSDK-JIT.h",
    "On1xSDK-MachineIR.h",
    "On1xSDK-ObjectABI.h",
    "On1xSDK-Target.h",
    "On1xSDK-TargetMachine.h",
    "On1xSDK-Toolchain.h",
    "On1xSDK-ToolchainResolver.h",
]


def compile_one(compiler, language, standard, include, source):
    command = [
        compiler,
        f"-std={standard}",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        "-fsyntax-only",
        "-x",
        language,
        "-I",
        str(include),
        str(source),
    ]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise SystemExit(
            f"{source.name} failed under {compiler}:\n"
            f"{result.stdout.decode(errors='replace')}"
            f"{result.stderr.decode(errors='replace')}"
        )


def main():
    if len(sys.argv) != 6:
        raise SystemExit("usage: check_backend_headers.py CC CXX INCLUDE WORK STAMP")
    cc, cxx, include_text, work_text, stamp_text = sys.argv[1:]
    include = pathlib.Path(include_text)
    work = pathlib.Path(work_text)
    work.mkdir(parents=True, exist_ok=True)
    for index, header in enumerate(HEADERS):
        c_source = work / f"header-{index}.c"
        cxx_source = work / f"header-{index}.cpp"
        text = f"#include <{header}>\nint main(void) {{ return 0; }}\n"
        c_source.write_text(text, encoding="utf-8")
        cxx_source.write_text(text, encoding="utf-8")
        compile_one(cc, "c", "c11", include, c_source)
        compile_one(cxx, "c++", "c++20", include, cxx_source)
    pathlib.Path(stamp_text).write_text("ok\n", encoding="utf-8")


if __name__ == "__main__":
    main()
