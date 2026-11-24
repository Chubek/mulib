#!/usr/bin/env python3
"""Check the typed scalar emitter after a real generated registry build.

Direct C11 emission only, not a native driver admission/review test.
"""
import argparse
from pathlib import Path
import subprocess


ON1X = Path(__file__).resolve().parents[3]
FIXTURES = Path(__file__).resolve().parent / "fixtures"
CLAMP_OUTPUT = (
    "42\n9223372036854775807\n-9223372036854775808\n9223372036854775807\n"
    "-9223372036854775808\n6\n0\n10\n4\n-7\n"
)
COUNTER_OUTPUT = "2\n40\n42\n18446744073709551615\n18446744073709551614\n0\n"


def run(command, *, expected=0, diagnostic=None):
    argv = list(map(str, command))
    p = subprocess.run(argv, capture_output=True, text=True, timeout=60)
    passed = p.returncode != 0 if expected is None else p.returncode == expected
    assert passed and (diagnostic is None or diagnostic in p.stderr), (argv, p)
    return p


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--on1xc", required=True, type=Path)
    parser.add_argument("--cc", required=True)
    parser.add_argument("--implementation-cc")
    parser.add_argument("--work", required=True, type=Path)
    args = parser.parse_args()
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    h = ["-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
         "-I", ON1X.parent / "include", "-I", ON1X / "runtime"]
    host = [args.cc, *h]
    build = [*host, "-DON1X_KERNEL_ABI_V1=1"]
    runtime = ON1X / "runtime/on1x_runtime.c"

    def emit(src, dest, *options):
        path = work / (dest + ".c")
        run([args.on1xc, "--emit-c", "--no-prelude", "--kernel-c11", *options, src, "-o", path])
        return path

    # The independent C caller and generated caller use the same actual
    # implementation object. It may be built with the other host compiler.
    impl = work / "implementation.c"
    impl.write_text("#define MULIB_IMPL_CLAMP\n#include <mulib-clamp.h>\n")
    obj = work / "implementation.o"
    run([args.implementation_cc or args.cc, *h, "-c", impl, "-o", obj])
    clamp = emit(ON1X / "examples/kernel/clamp.on1x", "clamp")
    contents = clamp.read_text()
    assert contents.count("#include <mulib-clamp.h>") == 1
    assert "_Generic(&mulib_clamp_i64, long (*)(long, long, long)" in contents
    executable = work / "clamp-run"
    run([*build, clamp, runtime, obj, "-o", executable])
    oracle = work / "clamp-oracle"
    run([*host, ON1X / "tests/fixtures/kernel_clamp_oracle.c", obj, "-o", oracle])
    assert run([executable]).stdout == run([oracle]).stdout == CLAMP_OUTPUT

    # ABI negative: same width/result signedness, *different* C identity.
    # Keep the canonical header untouched and alter only this negative
    # fixture's expected prototype; compilers must reject the assertion.
    wrong = work / "wrong-primitive.c"
    expected = "_Generic(&mulib_clamp_add_i64, long (*)(long, long)"
    assert contents.count(expected) == 1
    wrong.write_text(contents.replace(expected, expected.replace(", long (*)", ", long long (*)", 1)))
    run([*build, "-c", wrong, "-o", work/"wrong.o"], expected=None, diagnostic="prototype drift")
    run([*host, "-c", clamp, "-o", work/"wrong-target.o"], expected=None,
        diagnostic="verified native Linux x86-64 build profile")

    guard = emit(FIXTURES / "foreign_guard_order.on1x", "guard", "--no-optimizations")
    text = guard.read_text()
    assert text.rindex("on1x_assert(") < text.rindex("on1x_ffi_local_mulib_clamp_i64(")
    executable = work / "guard-run"
    run([*build, guard, runtime, obj, "-o", executable])
    assert run([executable], expected=70, diagnostic="On1x assertion failed").stdout == "8\n2\n1\n"

    # The inline caller has no implementation object or switch at all.
    counter = emit(FIXTURES / "container_counter.on1x", "inline")
    text = counter.read_text()
    assert text.count("#include <mulib-container-core.h>") == 1
    assert "_Generic(&mulib_ct_counter_add, unsigned long (*)(unsigned long, unsigned long)" in text
    executable = work / "inline-run"
    run([*build, counter, runtime, "-o", executable])
    oracle = work / "inline-oracle"
    run([*host, FIXTURES / "container_counter_oracle.c", "-o", oracle])
    assert run([executable]).stdout == run([oracle]).stdout == COUNTER_OUTPUT

    staged = work / "staged.on1x"
    staged.write_text("import kernel mulib.clamp as Clamp;\n"
                      "fn main(){println(Clamp.add_i64(40, 2));}\n")
    link_only = emit(staged, "staged", "--kernel-stage")
    assert "#include <mulib-clamp.h>" not in link_only.read_text()
    run([*host, link_only, runtime, obj, "-o", work/"unresolved"], expected=None,
        diagnostic="on1x_kernel_scalar_v1_mulib_clamp_add_i64")
    print("PASS: real clamp and unsigned inline oracles, guard ordering and fatal exit, "
          "same-width primitive and target negatives, unresolved link-only staging")


if __name__ == "__main__":
    main()
