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


def guest_tools(clang, runtime_include, archvirt_include):
    if sys.platform != "linux" or platform.machine() != "x86_64":
        raise ValueError("the ArchVirt guest profile requires native x86-64 Linux")
    root = Path(__file__).resolve().parent
    for header in (runtime_include / "on1x_runtime.h", archvirt_include / "archvirt_boot.h"):
        if not header.is_file():
            raise ValueError(f"missing required guest ABI header: {header}")
    env = dict(os.environ)
    for name in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
                 "LIBRARY_PATH", "COMPILER_PATH", "GCC_EXEC_PREFIX"):
        env.pop(name, None)
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
    return root, env, cc, cflags


def invoke(command, env):
    subprocess.run(command, check=True, env=env, timeout=60)


def compile_runtime(root, work, env, cc, cflags):
    start, runtime = work / "start.o", work / "runtime.o"
    invoke([*cc, root / "start.S", "-o", start], env)
    invoke([*cc, *cflags, root / "guest.c", "-o", runtime], env)
    return start, runtime


def build_archive(clang, runtime_include, archvirt_include, output, ar):
    root, env, cc, cflags = guest_tools(clang, runtime_include, archvirt_include)
    with tempfile.TemporaryDirectory(prefix=".on1x-guest-", dir=output.parent) as temporary:
        work = Path(temporary).resolve()
        env["TMPDIR"] = str(work)
        objects = compile_runtime(root, work, env, cc, cflags)
        result = work / "guest.a"
        invoke([ar, "rcsD", result, *objects], env)
        os.replace(result, output)


def compile_guest(clang, runtime_include, archvirt_include, generated, output, *, lld=None,
                  runtime_archive=None):
    root, env, cc, cflags = guest_tools(clang, runtime_include, archvirt_include)
    if generated.resolve() == output.resolve() or (
        output.exists() and generated.samefile(output)
    ):
        raise ValueError("guest output must be separate from checked C11 input")
    lld = lld or shutil.which("ld.lld")
    if not lld:
        raise ValueError("ld.lld is required for the DomIOS guest")
    with tempfile.TemporaryDirectory(prefix=".on1x-guest-", dir=output.parent) as temporary:
        work = Path(temporary).resolve()
        result, program = work / "guest.elf", work / "program.o"
        env["TMPDIR"] = str(work)
        objects = (["--whole-archive", runtime_archive.resolve(), "--no-whole-archive"]
                   if runtime_archive else compile_runtime(root, work, env, cc, cflags))
        invoke([*cc, *cflags, "-include", "on1x_archvirt_guest.h",
                generated.resolve(), "-o", program], env)
        invoke([lld, "-m", "elf_x86_64", "-static", "--build-id=none", "--no-undefined",
                "--fatal-warnings", "-z", "max-page-size=4096", "-T", root / "guest.ld",
                *objects, program, "-o", result], env)
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
    cli.add_argument("--build-runtime", metavar="AR")
    cli.add_argument("--runtime-archive", type=Path)
    cli.add_argument("checked_c11", nargs="?", type=Path)
    cli.add_argument("-o", "--output", required=True, type=Path)
    args = cli.parse_args()
    try:
        if args.build_runtime:
            if args.checked_c11 or args.runtime_archive or args.lld:
                cli.error("--build-runtime cannot be combined with program inputs")
            build_archive(args.clang, args.runtime_include, args.archvirt_include,
                          args.output, args.build_runtime)
        else:
            if not args.checked_c11:
                cli.error("checked C11 input is required")
            compile_guest(args.clang, args.runtime_include, args.archvirt_include,
                          args.checked_c11, args.output, lld=args.lld,
                          runtime_archive=args.runtime_archive)
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
