#!/usr/bin/env python3
"""C11 executable, rejecting frontends, injected failures and native isolation."""

import argparse
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tempfile


def run(*argv, cwd=None, error=None):
    result = subprocess.run(argv, cwd=cwd, text=True, capture_output=True, timeout=90)
    if error is None:
        assert result.returncode == 0, result
    else:
        assert result.returncode != 0 and error in result.stderr, result
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("on1xc", type=Path)
    parser.add_argument("cc")
    parser.add_argument("work", type=Path)
    parser.add_argument("--cflag", action="append", default=[])
    parser.add_argument("--native-only", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    kernel_headers = root.parent / "kernels"
    runtime = root / "stdlib/container/runtime.c"
    fixture = root / "tests/fixtures/container_native.on1x"
    on1xc = args.on1xc.resolve()
    if (sys.platform != "linux" or platform.libc_ver()[0] != "glibc" or
            not Path("/proc/self/fd").is_dir() or
            not re.fullmatch(r"x86_64(?:-[a-z0-9_]+)?-linux-gnu", run(args.cc, "-dumpmachine").strip())):
        print("UNSUPPORTED: container native test requires native Linux/glibc x86-64 with procfs")
        return 77

    args.work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="container-native-", dir=args.work.resolve()) as directory:
        work = Path(directory)
        (work / "bytes").write_bytes(b"\0\xff\x2a")
        (work / "link").symlink_to("bytes")
        os.mkfifo(work / "fifo")
        with (work / "sparse").open("wb") as file:
            file.seek(4294967303)
            file.write(b"\xad")

        cc = [args.cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
              "-I", kernel_headers, "-I", root / "stdlib", *args.cflag]
        # The harness, leaf bridge and implementation are distinct TUs;
        # only the last has the MuLIB implementation switch.
        kernels = work / "kernels.c"
        kernels.write_text('#define MULIB_IMPL_XTHREADS\n#include "mulib-xthreads.h"\n')
        native = work / "native"
        bridges = ["-DON1X_CONTAINER_TLS", "-DON1X_CONTAINER_FOREIGN"]
        run(*cc, *bridges, root / "tests/test_container_native.c", runtime, kernels, "-pthread",
            *(f"-Wl,--wrap={symbol}" for symbol in
              ["mulib_xtss_create", "mulib_xtss_set", "fstat64", "pread64", "close"]),
            "-o", native)
        run(native, cwd=work)
        run(native, "fail-key", cwd=work)
        if args.native_only:
            return 0

        driver = [sys.executable, root / "tools/on1x-build.py", "--on1xc", on1xc,
                  "--cc", args.cc, *(f"--cflag={flag}" for flag in args.cflag)]
        program, plan = work / "on1x-native", work / "on1x-native.json"
        run(*driver, fixture, "-o", program, "--mulib-plan", plan)
        assert run(program, cwd=work) == "container native ok\n"
        linked = json.loads(plan.read_text())
        assert linked["safe_export"] is False
        assert linked["plan"]["implementation_translation_units"] == 1
        assert linked["plan"]["implementation_macros"] == ["MULIB_IMPL_XTHREADS"]
        assert all(str(runtime.with_suffix(ext)) in linked["inputs"] for ext in (".c", ".h"))

        # Check the generated C declarations against the C/C++ bridge header
        # in the same TU, rather than relying on linker symbol names alone.
        generated = work / "generated.c"
        run(on1xc, "--emit-c", fixture, "-o", generated)
        run(*cc, "-I", root / "runtime", "-include", runtime.with_suffix(".h"),
            "-fsyntax-only", generated)

        def source(name, text):
            path = work / (name + ".on1x")
            path.write_text(text, encoding="ascii")
            return path

        tls = "import std.container.Tls as T;\n"
        foreign = "import std.container.Foreign as F;\n"
        for name, statement, expected_kernel in (
            ("tls", tls + "fn main(){ assert(T.basis_tls_i64_set(9)); }\n", True),
            ("file", foreign + 'fn main(){ F.basis_foreign_file_byte_at("bytes", 0); }\n', False),
        ):
            separate = source(name + "_only", statement)
            run(*driver, "--no-prelude", separate, "-o", program, "--mulib-plan", plan)
            run(program, cwd=work)
            recipe = json.loads(plan.read_text())["plan"]
            if expected_kernel:
                assert recipe["implementation_macros"] == ["MULIB_IMPL_XTHREADS"]
            else:
                assert recipe is None
        for name, statement in (
            ("tls", tls + "fn main(){ taskgroup{ let child = launch{ T.basis_tls_i64_clear(); 0 }; join child; } }\n"),
            ("file", foreign + 'fn main(){ taskgroup{ let child = launch{ F.basis_foreign_file_byte_at("bytes", 0); 0 }; join child; } }\n'),
        ):
            run(on1xc, "--check", source(name + "_concurrent", statement), error="raw foreign effects")
        run(on1xc, "--check", source("no_native_handle", foreign +
            'fn main(){ F.basis_foreign_file_byte_at(12, 0); }\n'), error="string")
        run(on1xc, "--check", source("no_tls_key", tls +
            "fn main(){ T.basis_tls_i64_get(12); }\n"), error="basis_tls_i64_get")
        run(on1xc, "--kernel-c11", "--emit-c", fixture, "-o", work / "denied.c",
            error="ON1X-KERNEL-EXTERN")
        run(on1xc, "--emit-ir", fixture, "-o", work / "denied.ir", error="ON1X-MIR-UNSUPPORTED")
        assert not (work / "denied.c").exists() and not (work / "denied.ir").exists()

        # The typed wrapper's failure action must not run the source body.
        fault = source("tls_failure", tls + """\
fn main() {
  match T.basis_tls_i64_with(7, fn()->i64{ assert(false); return 1; }) with {
    Some(_) => assert(false), None() => assert(true)
  };
  match T.basis_tls_i64_current() with {
    Some(_) => assert(false), None() => assert(true)
  };
}
""")
        fault_c = work / "fault.c"
        fault_c.write_text('''#include "mulib-xthreads.h"
int __wrap_mulib_xtss_create(mulib_xtss_t *key) {
    (void)key;
    return MULIB_XTHREAD_SYSTEM;
}
''')
        generated_fault = work / "generated_fault.c"
        run(on1xc, "--emit-c", fault, "-o", generated_fault)
        run(*cc, "-DON1X_CONTAINER_TLS", "-I", root / "runtime", "-pthread",
            generated_fault, root / "runtime/on1x_runtime.c", runtime, kernels, fault_c,
            "-Wl,--wrap=mulib_xtss_create", "-o", program)
        run(program, cwd=work)
    return 0


if __name__ == "__main__":
    sys.exit(main())
