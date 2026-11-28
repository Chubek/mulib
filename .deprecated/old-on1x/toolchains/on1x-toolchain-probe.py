#!/usr/bin/env python3
"""Probe explicit trusted tools for the On1x Linux AMD64 host build profile."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import signal
import stat
import struct
import subprocess
import sys
import tempfile
import time

if sys.platform == "linux":
    import resource


TARGET = "x86_64-unknown-linux-gnu"
CC_TARGETS = {TARGET, "x86_64-linux-gnu", "x86_64-pc-linux-gnu"}
MAX_TOOL_BYTES = 512 * 1024 * 1024
MAX_STAGE_FILE_BYTES = 8 * 1024 * 1024
C_SOURCE = """\
#include <features.h>
#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if !defined(__linux__) || !defined(__x86_64__) || !defined(__GLIBC__)
#error expected Linux AMD64/glibc
#endif
_Static_assert(CHAR_BIT == 8 && CHAR_MIN == -128 && sizeof(short) == 2 &&
               sizeof(int) == 4 && sizeof(long) == 8 && sizeof(long long) == 8 &&
               sizeof(void *) == 8 && sizeof(size_t) == 8 && sizeof(ptrdiff_t) == 8 &&
               sizeof(int64_t) == 8 && sizeof(_Bool) == 1, "expected signed-char LP64");
_Static_assert(FLT_RADIX == 2 && sizeof(float) == 4 && FLT_MANT_DIG == 24 &&
               FLT_MAX_EXP == 128 && sizeof(double) == 8 && DBL_MANT_DIG == 53 &&
               DBL_MAX_EXP == 1024, "expected binary32 and binary64");
struct layout { char tag; void *data; double value; };
_Static_assert(sizeof(struct layout) == 24 && _Alignof(struct layout) == 8 &&
               offsetof(struct layout, data) == 8 && offsetof(struct layout, value) == 16,
               "unexpected record layout");

extern double on1x_probe_mix(long, double);
int main(void) {
    if (on1x_probe_mix(40L, 2.5) != 42.5)
        return 1;
    return puts("on1x-toolchain-ok") < 0;
}
"""
ASM_SOURCE = """\
.text
.globl on1x_probe_mix
.type on1x_probe_mix, @function
on1x_probe_mix:
    cvtsi2sdq %rdi, %xmm1
    addsd %xmm1, %xmm0
    ret
.size on1x_probe_mix, .-on1x_probe_mix
.section .note.GNU-stack,"",@progbits
"""


class ProbeError(Exception):
    pass


def tool_path(role, value):
    path = Path(value)
    if not path.is_absolute():
        raise ProbeError(f"{role}: supply an absolute trusted executable path")
    try:
        path = path.resolve(strict=True)
        if not path.is_file() or not os.access(path, os.X_OK):
            raise ProbeError(f"{role}: not a regular executable: {path}")
    except (OSError, RuntimeError) as error:
        raise ProbeError(f"{role}: cannot resolve executable: {error}") from error
    return path


def digest_tool(role, path):
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or not 0 < info.st_size <= MAX_TOOL_BYTES:
            raise ProbeError(f"{role}: empty, non-regular or oversized executable")
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            size += len(chunk)
            if size > info.st_size:
                raise ProbeError(f"{role}: executable changed while reading")
            digest.update(chunk)
        if size != info.st_size:
            raise ProbeError(f"{role}: executable changed while reading")
    return digest.hexdigest()


def stage_limits():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_FSIZE, (MAX_STAGE_FILE_BYTES, MAX_STAGE_FILE_BYTES))


class Probe:
    def __init__(self, directory):
        self.directory = directory
        self.deadline = time.monotonic() + 30
        # Driver subprocesses may use the system toolchain and its standard
        # search paths. Do not inherit PATH, specs, libraries, or flag overrides.
        self.env = {"PATH": os.defpath, "LC_ALL": "C", "TMPDIR": str(directory)}

    def run(self, stage, *argv):
        timeout = self.deadline - time.monotonic()
        if timeout <= 0:
            raise ProbeError(f"{stage}: probe deadline exceeded")
        with tempfile.TemporaryFile(dir=self.directory) as output:
            try:
                child = subprocess.Popen(argv, cwd=self.directory, env=self.env,
                                         stdin=subprocess.DEVNULL, stdout=output,
                                         stderr=subprocess.STDOUT, start_new_session=True,
                                         preexec_fn=stage_limits)
            except (OSError, subprocess.SubprocessError) as error:
                raise ProbeError(f"{stage}: could not start: {error}") from error
            try:
                status = child.wait(timeout=timeout)
            except BaseException as error:
                try:
                    os.killpg(child.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                child.wait()
                if isinstance(error, subprocess.TimeoutExpired):
                    raise ProbeError(f"{stage}: probe deadline exceeded") from error
                raise
            output.seek(0)
            text = output.read(65536).decode("utf-8", errors="replace")
        if status:
            raise ProbeError(f"{stage}: failed (exit {status})\n{text.rstrip()}")
        return text

    def check_elf(self, stage, filename, kind):
        path = self.directory / filename
        if path.is_file():
            with path.open("rb") as stream:
                header = stream.read(64)
            if (len(header) == 64 and header[:7] == b"\x7fELF\x02\x01\x01"
                    and header[7:9] in (b"\x00\x00", b"\x03\x00")
                    and struct.unpack_from("<HHI", header, 16) == (kind, 62, 1)
                    and struct.unpack_from("<IH", header, 48) == (0, 64)):
                return
        expected = "relocatable" if kind == 1 else "executable"
        raise ProbeError(f"{stage}: expected an AMD64 ELF64 {expected}: {filename}")

    def build(self, cc, assembler, linker):
        (self.directory / "probe.c").write_text(C_SOURCE, encoding="ascii")
        (self.directory / "sysv.s").write_text(ASM_SOURCE, encoding="ascii")
        self.run("C11 ABI compile", cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
                 "-Werror", "-fno-lto", "-fno-pie", "-c", "probe.c", "-o", "probe.o")
        self.check_elf("C11 ABI compile", "probe.o", 1)
        self.run("assemble SysV fixture", assembler, "--64", "sysv.s", "-o", "sysv.o")
        self.check_elf("assemble SysV fixture", "sysv.o", 1)
        # Invoke the precise linker, not just a driver reporting its name.
        self.run("relocatable link", linker, "-m", "elf_x86_64", "-r",
                 "probe.o", "sysv.o", "-o", "mixed.o")
        self.check_elf("relocatable link", "mixed.o", 1)
        # Use the same -B / -fuse-ld=bfd selection and non-PIE recipe as the
        # native build helper. The driver supplies its usual startup/libc files.
        tools = self.directory / "tools"
        tools.mkdir()
        for name in ("ld", "ld.bfd"):
            (tools / name).symlink_to(linker)
        self.run("hosted link", cc, f"-B{tools}/", "-fuse-ld=bfd", "-no-pie",
                 "mixed.o", "-Wl,-z,noexecstack", "-o", "probe")
        self.check_elf("hosted link", "probe", 2)
        if self.run("execute ABI fixture", self.directory / "probe") != "on1x-toolchain-ok\n":
            raise ProbeError("execute ABI fixture: unexpected output")


def qualify(target, paths):
    if target != TARGET or sys.platform != "linux" or platform.machine() != "x86_64":
        raise ProbeError(f"only native Linux {TARGET} is supported")
    tools = {role: tool_path(role, path) for role, path in paths.items()}
    hashes = {role: digest_tool(role, path) for role, path in tools.items()}
    with tempfile.TemporaryDirectory(prefix="on1x-toolchain-") as directory:
        probe = Probe(Path(directory))
        cc_target = probe.run("compiler target", tools["c_compiler"], "-dumpmachine").strip()
        if cc_target not in CC_TARGETS:
            raise ProbeError(f"C compiler target mismatch: {cc_target!r}")
        versions = {}
        for role, path in tools.items():
            version = probe.run(f"{role} version", path, "--version").partition("\n")[0].strip()
            if not version or not version.isascii() or not version.isprintable() or len(version) > 256:
                raise ProbeError(f"{role}: missing or invalid version banner")
            versions[role] = version
        probe.build(tools["c_compiler"], tools["assembler"], tools["linker"])
    for role, path in tools.items():
        if tool_path(role, paths[role]) != path or digest_tool(role, path) != hashes[role]:
            raise ProbeError(f"{role}: executable changed during probe")
    return {
        "schema": "on1x.toolchain-probe/1",
        "target": target,
        "abi": "SysVAMD64",
        "object_format": "ELF64",
        "profile": "linux-amd64-glibc-nonpie",
        "cc_target": cc_target,
        "tools": {
            role: {"path": str(path), "sha256": hashes[role], "version": versions[role]}
            for role, path in tools.items()
        },
    }


def main():
    cli = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    cli.add_argument("--target", required=True, help=f"exact supported target: {TARGET}")
    for option in ("cc", "assembler", "linker"):
        cli.add_argument(f"--{option}", required=True, help="absolute trusted executable path")
    args = cli.parse_args()
    try:
        manifest = qualify(args.target, {"c_compiler": args.cc,
                                         "assembler": args.assembler, "linker": args.linker})
        print(json.dumps(manifest, sort_keys=True, separators=(",", ":")))
        return 0
    except (ProbeError, OSError, ValueError) as error:
        print(f"on1x-toolchain-probe: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("on1x-toolchain-probe: cancelled", file=sys.stderr)
        return 130


def interrupted(signum, frame):
    raise KeyboardInterrupt


if __name__ == "__main__":
    signal.signal(signal.SIGTERM, interrupted)
    sys.exit(main())
