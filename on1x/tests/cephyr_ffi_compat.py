#!/usr/bin/env python3
"""Probe the real clamp declaration with an explicitly supplied libcephyr.

Run on Linux/AMD64 with CPython 3.10+:
    python3 on1x/tests/cephyr_ffi_compat.py /path/to/libcephyr.so

Exit nonzero on incompatibility. This checks a C compilation prerequisite,
not KernelBind admission or the public On1xRT module interface.
"""
import argparse
from pathlib import Path
import platform
import runpy
import subprocess
import sys
from tempfile import TemporaryDirectory


def main():
    on1x = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("library", type=Path)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--config", type=Path, help="On1x baseline TOML from the enabled build")
    parser.add_argument("--libstone-root", type=Path, default=on1x/"third_party/libstone")
    args = parser.parse_args()
    if sys.platform != "linux" or platform.machine() != "x86_64":
        parser.error("the executable control requires an AMD64 Linux host")
    root = args.libstone_root.resolve()/"cornerstone/Cephyr"
    sys.path.insert(0, str(root/"python"))
    import cephyr

    compiler = cephyr.Compiler(args.library.resolve(strict=True))
    target = "--target=x86_64-linux-gnu"
    fixture = on1x/"tests/fixtures"
    include = on1x.parent/"include"
    control = '''#include "contract_abi.h"
_Static_assert(_Generic(&contract_add, int (*)(int,int): 1, default: 0), "signature drift");
int main(void) { return contract_add(40,2) != 42; }
'''
    # Exercise the production emitter/oracle without minting review evidence.
    exo = runpy.run_path(str(on1x/"tools/on1x-exotract.py"))
    interface = {"exports": {s: {"c_name": s} for s in exo["CLAMP_EXPORTS"]}}
    clamp = exo["clamp_adapter"](interface)
    with TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        config = tmp/"Cephyr.toml"
        if args.config:
            config = args.config.resolve(strict=True)
        else:
            config.write_text("[blackstone]\nenabled = false\n")

        def embedded(source, headers):
            return compiler.compile(("--config", config, target, "--stdlib=cephyr",
                                     "--no-factir", "-I"+str(headers), "-x", "c", "-", "-c", "-o", "-"),
                                    source, resource_root=root, archon_root=root.parent/"Archon")

        def run_object(result, headers, *companions):
            obj = tmp/"probe.o"
            obj.write_bytes(result.output)
            exe = tmp/"probe"
            subprocess.run([args.cc, "-std=c11", "-I", headers, obj, *companions,
                            "-o", exe], check=True)
            subprocess.run([exe], check=True)

        bad = embedded(control.replace("(*)(int,int)", "(*)(int)"), fixture)
        if bad.status != cephyr.Status.COMPILE_ERROR or b"signature drift" not in bad.diagnostics:
            sys.exit(f"wrong prototype was not rejected by the assertion: {bad.diagnostics!r}")
        result = embedded(control, fixture)
        if not result.ok:
            sys.exit(f"scalar control failed: {result.diagnostics!r}")
        run_object(result, fixture, fixture/"contract_abi.c")

        implementation = tmp/"mulib.c"
        implementation.write_text("#define MULIB_IMPL_CLAMP\n#include <mulib-clamp.h>\n")
        adapter = tmp/"kernel-adapter.c"
        adapter.write_text(clamp)
        probe = tmp/"probe.c"
        probe.write_text(exo["clamp_probe"]())
        host = tmp/"host"
        subprocess.run([args.cc, "-std=c11", "-pedantic-errors", "-I", include, probe, adapter,
                        implementation, "-o", host], check=True)
        subprocess.run([host], check=True)
        print("PASS: embedded scalar control, wrong-prototype rejection, host clamp boundary",
              flush=True)
        result = embedded(clamp, include)
        if not result.ok:
            sys.stderr.buffer.write(result.diagnostics)
            return 1
        run_object(result, include, probe, implementation)
        print("PASS: embedded clamp adapter linked to host-compiled implementation")
        kernel = embedded(implementation.read_text(), include)
        if not kernel.ok:
            sys.stderr.buffer.write(kernel.diagnostics)
            return 1
        kernel_object = tmp/"mulib.o"
        kernel_object.write_bytes(kernel.output)
        run_object(result, include, probe, kernel_object)
        print("PASS: independent embedded adapter and kernel objects passed the clamp boundary")
    return 0


if __name__ == "__main__":
    sys.exit(main())
