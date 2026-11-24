#!/usr/bin/env python3
"""Internal stage: checked On1x C11 --> freestanding DomIOS-v1 ELF, never a host executable."""
import argparse
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile


def compile_guest(clang, runtime_include, archvirt_include, generated, output, *, lld=None):
    if sys.platform != "linux" or platform.machine() != "x86_64":
        raise ValueError("the ArchVirt guest profile requires native x86-64 Linux")
    root = Path(__file__).resolve().parent
    if generated.resolve() == output.resolve() or (
        output.exists() and generated.samefile(output)
    ):
        raise ValueError("guest output must be separate from checked C11 input")
    for header in (runtime_include / "on1x_runtime.h", archvirt_include / "archvirt_boot.h"):
        if not header.is_file():
            raise ValueError(f"missing required guest ABI header: {header}")
    lld = lld or shutil.which("ld.lld")
    if not lld:
        raise ValueError("ld.lld is required for the DomIOS guest")
    env = dict(os.environ)
    for name in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
                 "LIBRARY_PATH", "COMPILER_PATH", "GCC_EXEC_PREFIX"):
        env.pop(name, None)
    with tempfile.TemporaryDirectory(prefix=".on1x-guest-", dir=output.parent) as temporary:
        work = Path(temporary).resolve()
        result = work / "guest.elf"
        env["TMPDIR"] = str(work)
        cc = [clang, "--no-default-config", "--target=x86_64-none-elf", "-c"]
        cflags = [
            "-std=c11", "-O1", "-ffreestanding", "-fno-builtin", "-nostdlibinc",
            "-fno-pic", "-fno-pie", "-mno-red-zone", "-mgeneral-regs-only",
            "-fno-stack-protector", "-fno-vectorize", "-fno-slp-vectorize",
            "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
            "-Werror=implicit-function-declaration",
            "-I", str(root / "include"), "-I", str(runtime_include.resolve()),
            "-I", str(archvirt_include.resolve()),
            "-x", "c",
        ]
        start, runtime, program = (work / name for name in ("start.o", "runtime.o", "program.o"))
        for cmd in (
            [*cc, root / "start.S", "-o", start],
            [*cc, *cflags, root / "guest.c", "-o", runtime],
            [*cc, *cflags, generated.resolve(), "-o", program],
            [lld, "-m", "elf_x86_64", "-static", "--build-id=none", "--no-undefined",
             "--fatal-warnings", "-z", "max-page-size=4096", "-T", root / "guest.ld",
             start, runtime, program, "-o", result],
        ):
            subprocess.run(cmd, check=True, env=env, timeout=60)
        if not 0 < result.stat().st_size <= 8 * 1024 * 1024:
            raise ValueError("DomIOS guest ELF must fit within 8 MiB")
        result.chmod(0o644)
        os.replace(result, output)


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--clang", required=True)
    cli.add_argument("--lld", help="path to ld.lld (defaults to ld.lld found on PATH)")
    cli.add_argument("--runtime-include", required=True, type=Path)
    cli.add_argument("--archvirt-include", required=True, type=Path)
    cli.add_argument("checked_c11", type=Path)
    cli.add_argument("-o", "--output", required=True, type=Path)
    args = cli.parse_args()
    try:
        compile_guest(args.clang, args.runtime_include, args.archvirt_include,
                      args.checked_c11, args.output, lld=args.lld)
    except subprocess.CalledProcessError as exc:
        print(f"On1x ArchVirt: guest C11 compilation/link failed "
              f"(host runtime and native linkage are unavailable; "
              f"{Path(exc.cmd[0]).name} exited with status {exc.returncode})", file=sys.stderr)
        return 1
    except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
        print(f"On1x ArchVirt: guest C11 compilation/link failed "
              f"(host runtime and native linkage are unavailable): {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
