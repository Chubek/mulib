#!/usr/bin/env python3
"""Strict separate-TU tests of ScalarABI against the real On1x runtime.

No generated Archon installation is needed for runtime tests. --native-tool
additionally executes native/C and native/native argument/result boundaries.
"""
import argparse
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys

BACKEND = Path(__file__).resolve().parents[1]
ON1X = BACKEND.parents[1]
MASK = (1 << 64) - 1


def run(command, **kwargs):
    result = subprocess.run([str(x) for x in command], text=True,
                            capture_output=True, timeout=120, **kwargs)
    if result.returncode:
        raise AssertionError(f"{' '.join(map(str, command))}\n"
                             f"status {result.returncode}\n{result.stdout}{result.stderr}")
    return result


def load_generator():
    spec = importlib.util.spec_from_file_location("scalar_abi_generator",
                                                BACKEND / "generate_scalar_abi.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    assert module.generate() == (BACKEND / "ScalarABI.c").read_text()
    assert module.generate() == module.generate()
    return module


def generator_checks(generator, build):
    directory = build / "generator"
    directory.mkdir(exist_ok=True)
    script = directory / "generate_scalar_abi.py"
    output = directory / "ScalarABI.c"
    script.write_bytes((BACKEND / script.name).read_bytes())
    output.write_text("stale generated file\n")
    stale = subprocess.run([sys.executable, str(script), "--check"],
                           capture_output=True, text=True, timeout=10)
    assert stale.returncode != 0 and "out of date" in stale.stderr
    assert output.read_text() == "stale generated file\n"
    for seed in ("1", "71"):
        run([sys.executable, script], env={**os.environ, "PYTHONHASHSEED": seed})
        assert output.read_text() == generator.generate()
        run([sys.executable, script, "--check"])
    print("generator: byte-identical across hash seeds, stale --check rejects without writing")


def coverage(generator, build, cxx):
    exe = build / "coverage"
    run([cxx, "-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
         "-I", ON1X / "include", BACKEND / "tests/TestScalarABICoverage.cpp",
         ON1X / "src/type.cpp", ON1X / "src/primitives.cpp", "-o", exe])
    # Harvest operation/type fragments from the real descriptor sources as well
    # as testing complete names. A new operation string is therefore queried
    # even when the wrapper generator has not yet been updated.
    tokens = set()
    for source in ("src/backend/numeric.hpp", "src/primitives.cpp", "src/type.cpp"):
        tokens.update(re.findall(r'"([a-z0-9_]+)"', (ON1X / source).read_text()))
    candidates = set(generator.rows)
    candidates.update(tokens)
    for left in tokens:
        for right in tokens:
            candidates.add(left + "_" + right)
    for target in ("i64", "bool", "f32", "f64", "u8", "u16", "u32", "u64"):
        for token in tokens:
            candidates.add(target + "_from_" + token)
    result = run([exe], input="\n".join(sorted(candidates)) + "\n")
    signatures = {}
    for line in result.stdout.splitlines():
        name, result_type, *args = line.split()
        signatures[name] = (args, result_type)
    direct = {f"{op}_{t}" for op in ("eq", "ne", "lt", "le", "gt", "ge")
              for t in ("u8", "u16", "u32", "u64")}
    # f64 arithmetic/comparison has dedicated MIR opcodes rather than Numeric.
    dedicated = {f"{op}_f64" for op in
                 ("add", "sub", "mul", "div", "neg", "eq", "ne", "lt", "le", "gt", "ge")}
    assert set(signatures) - direct == set(generator.rows) - dedicated, (
        "MIR numeric/runtime adapter coverage drift",
        (set(signatures) - direct) ^ (set(generator.rows) - dedicated))
    for name, signature in signatures.items():
        if name not in direct:
            assert signature == generator.rows[name], (name, signature, generator.rows[name])
    print(f"coverage: {len(signatures)} Numeric signatures, "
          f"{len(direct)} direct comparisons, {len(generator.rows)} adapters")


def literal(value):
    return f"UINT64_C(0x{value & MASK:016x})"


def call(name, *values):
    return f"on1x_archon_{name}({', '.join(literal(v) for v in values)})"


def runtime_source(generator):
    lines = [
        '#include <on1x_runtime.h>', '#include <stdint.h>', '#include <stdio.h>',
        '#include <stdlib.h>', '#include <string.h>', '#include <xmmintrin.h>',
        '#if !defined(ON1X_F64_ABI_VERSION) || !defined(ON1X_F32_ABI_VERSION)',
        '#error "This executable qualifies Linux AMD64 binary32/binary64 only"',
        '#endif',
        'static uint64_t bits_i64(int64_t v) { uint64_t b; memcpy(&b,&v,8); return b; }',
        'static uint64_t bits_f64(double v) { uint64_t b; memcpy(&b,&v,8); return b; }',
        'static uint64_t bits_f32(float v) { uint32_t b; memcpy(&b,&v,4); return b; }',
        'static void check(uint64_t got,uint64_t expected,const char *name) {',
        ' if(got!=expected) { fprintf(stderr,"%s: %llx != %llx\\n",name,',
        ' (unsigned long long)got,(unsigned long long)expected); exit(1); } }',
    ]
    for name, (args, _) in sorted(generator.rows.items()):
        lines.append(f"extern uint64_t on1x_archon_{name}("
                     + ", ".join("uint64_t" for _ in args) + ");")
    lines += ['extern void on1x_archon_print_f32(uint64_t);',
              'extern void on1x_archon_print_f64(uint64_t);']
    checks = []
    traps = []

    def expect(name, values, expected):
        checks.append(f'check({call(name, *values)},{literal(expected)},"{name}");')

    def trap(name, values, message):
        traps.append((call(name, *values) + ";", message))

    # Exercise every exported adapter across independent C declarations and
    # separately compiled TUs, comparing to its actual typed runtime entrypoint.
    for name, (args, result) in sorted(generator.rows.items()):
        for a, b in ((3, 1), (1, 1), (7, 2)):
            values = [a, b][:len(args)]
            typed = [f"({generator.TYPES[t]}){v}" for t, v in zip(args, values)]
            carriers = [
                f"bits_{t}({v})" if t in ("f32", "f64", "i64") else literal(v)
                for t, v in zip(args, values)]
            reference = f"on1x_{name}({', '.join(typed)})"
            reference = (f"bits_{result}({reference})"
                         if result in ("i64", "f32", "f64")
                         else f"(uint64_t)({reference})")
            checks.append(f'check(on1x_archon_{name}({", ".join(carriers)}),'
                          f'{reference},"typed {name}");')

    for width in (8, 16, 32, 64):
        t = f"u{width}"
        maximum = (1 << width) - 1
        for name, values, expected in (
                ("from_bits", [MASK], maximum), ("to_bits", [maximum], maximum),
                ("wrapping_add", [maximum, 1], 0),
                ("wrapping_sub", [0, 1], maximum),
                ("wrapping_mul", [maximum, maximum], 1),
                ("and", [maximum, 0x55], 0x55),
                ("or", [0x80, 0x55], 0xd5), ("xor", [maximum, maximum], 0),
                ("not", [0], maximum), ("shr", [maximum, width - 1], 1),
                ("shl", [1, width - 1], 1 << (width - 1))):
            expect(f"{t}_{name}", values, expected)
        for op, values, message in (
                ("add", [maximum, 1], "unsigned addition overflow"),
                ("sub", [0, 1], "unsigned subtraction underflow"),
                ("mul", [maximum, 2], "unsigned multiplication overflow"),
                ("div", [1, 0], "unsigned division by zero"),
                ("mod", [1, 0], "unsigned modulo by zero")):
            trap(f"{op}_{t}", values, message)
        trap(f"{t}_from_i64", [MASK], "negative unsigned conversion")
        for count in (MASK, width):
            for op in ("shl", "shr"):
                trap(f"{t}_{op}", [1, count], "unsigned shift count out of range")
        trap(f"{t}_shl", [maximum, 1], "unsigned left shift overflow")
        if width < 64:
            trap(f"{t}_from_u64", [maximum + 1], "unsigned conversion out of range")
            trap(f"{t}_from_i64", [maximum + 1], "unsigned conversion out of range")
        for f, bound in (("f32", (127 + width) << 23),
                         ("f64", (1023 + width) << 52)):
            trap(f"{t}_from_{f}", [bound], "float to unsigned out of range")
    trap("u64_to_i64", [1 << 63], "unsigned to i64 out of range")
    for f, sign, one, two, inf, nan, snan, negnan, maxfinite, limit in (
            ("f32", 1 << 31, 0x3f800000, 0x40000000, 0x7f800000,
             0x7fc00000, 0x7f800001, 0xffc12345, 0x7f7fffff, 0x5f000000),
            ("f64", 1 << 63, 0x3ff0000000000000, 0x4000000000000000,
             0x7ff0000000000000, 0x7ff8000000000000, 0x7ff0000000000001,
             0xfff8123456789abc, 0x7fefffffffffffff, 0x43e0000000000000)):
        for bits in (0, sign, 1, sign | 1, one, inf, sign | inf, maxfinite):
            expect(f"{f}_from_bits", [bits], bits)
            expect(f"{f}_to_bits", [bits], bits)
        for bits in (snan, negnan, nan):
            expect(f"{f}_from_bits", [bits], nan)
            expect(f"{f}_to_bits", [bits], nan)
            expect(f"{f}_is_nan", [bits], 1)
            expect(f"{f}_is_finite", [bits], 0)
            expect(f"{f}_signbit", [bits], 0)  # Canonical NaNs are positive.
        expect(f"neg_{f}", [0], sign)
        expect(f"neg_{f}", [sign], 0)
        expect(f"add_{f}", [sign, sign], sign)
        expect(f"mul_{f}", [sign, one], sign)
        expect(f"div_{f}", [one, sign], sign | inf)
        expect(f"div_{f}", [0, 0], nan)
        expect(f"mul_{f}", [maxfinite, two], inf)
        expect(f"add_{f}", [1, 1], 2)  # Gradual underflow, not FTZ.
        expect(f"{f}_to_i64", [limit | sign], 1 << 63)
        expect(f"{f}_from_i64", [1 << 63], limit | sign)
        expect(f"{f}_signbit", [sign], 1)
        for op, expected in (("eq", 1), ("ne", 0), ("lt", 0),
                             ("le", 1), ("gt", 0), ("ge", 1)):
            expect(f"{op}_{f}", [0, sign], expected)
            expect(f"{op}_{f}", [nan, one], int(op == "ne"))
        for bits in (limit, inf, sign | inf, nan):
            trap(f"{f}_to_i64", [bits], f"{f} to i64 out of range")
        for t in ("u8", "u16", "u32", "u64"):
            for bits in (sign | one, inf, nan):
                trap(f"{t}_from_{f}", [bits], "float to unsigned out of range")
        # Every operation that promises environment validation must reject a
        # foreign round mode, FTZ, DAZ, or an unmasked SSE exception.
        for mode in (0x2000, 0x4000, 0x6000, 0x8000, 0x40, -0x80):
            for name, values in ((f"add_{f}", [one, one]),
                                 (f"eq_{f}", [one, one]),
                                 (f"{f}_from_i64", [1]),
                                 (f"{f}_to_i64", [one]),
                                 (f"neg_{f}", [one]),
                                 (f"u64_from_{f}", [one]),
                                 (f"{f}_from_u64", [1])):
                traps.append((f"_mm_setcsr(0x1f80u + ({mode})); "
                              + call(name, *values) + ";",
                              "unsupported f64 environment"))
    trap("f32_from_bits", [MASK], "f32 bit pattern out of range")
    trap("f32_from_bits", [1 << 32], "f32 bit pattern out of range")
    expect("f32_from_f64", [0x8000000000000000], 0x80000000)
    expect("f64_from_f32", [0x80000000], 0x8000000000000000)
    expect("f32_from_f64", [0x7ff0000000000001], 0x7fc00000)
    expect("f64_from_f32", [0x7f800001], 0x7ff8000000000000)
    # RNE narrowing overflow and exact rounding midpoint.
    expect("f32_from_f64", [0x47efffffe0000000], 0x7f7fffff)
    expect("f32_from_f64", [0x47effffff0000000], 0x7f800000)
    expect("f32_from_f64", [0x3ff0000010000000], 0x3f800000)
    expect("f32_from_f64", [0x3ff0000010000001], 0x3f800001)
    expect("f64_from_u64", [MASK], 0x43f0000000000000)
    expect("f32_from_u64", [MASK], 0x5f800000)
    expect("u64_from_f64", [0x43efffffffffffff], 0xfffffffffffff800)
    expect("u64_from_f32", [0x5f7fffff], 0xffffff0000000000)
    for name, value in (("f32_from_f64", 0x3ff0000000000000),
                        ("f64_from_f32", 0x3f800000)):
        traps.append(("_mm_setcsr(0x3f80u); " + call(name, value) + ";",
                      "unsupported f64 environment"))
    for op, a, b, message in (
            ("add", "INT64_MAX", "1", "addition overflow"),
            ("sub", "INT64_MIN", "1", "subtraction overflow"),
            ("mul", "INT64_MIN", "-1", "multiplication overflow"),
            ("div", "INT64_MIN", "-1", "invalid division"),
            ("mod", "INT64_MIN", "-1", "invalid modulo"),
            ("div", "1", "0", "invalid division"),
            ("mod", "1", "0", "invalid modulo")):
        traps.append((f"on1x_{op}_i64({a},{b});", message))
    traps.append(("on1x_neg_i64(INT64_MIN);", "negation overflow"))
    lines += ["int main(int argc,char **argv) {", "_mm_setcsr(0x1f80u);",
              'if(argc==2 && strcmp(argv[1],"print")==0) {',
              'on1x_archon_print_f32(UINT64_C(0x80000000));',
              'on1x_archon_print_f64(UINT64_C(0x8000000000000000));',
              'on1x_archon_print_f32(UINT64_C(0x7f800001));',
              'on1x_archon_print_f64(UINT64_C(0xfff0000000000000)); return 0; }',
              "if(argc==2) { switch(atoi(argv[1])) {"]
    for index, (statement, _) in enumerate(traps):
        lines.append(f"case {index}: {statement} break;")
    lines += ['default: return 2;', '} return 3; }'] + checks
    # Bit/classification operations intentionally remain valid in a foreign
    # environment. Preserve runtime semantics rather than invent extra traps.
    lines += ['_mm_setcsr(0xbfc0u);',
              f'check({call("f64_from_bits", 1 << 63)},{literal(1 << 63)},"bits environment");',
              f'check({call("f32_is_nan", 0x7f800001)},1,"classify environment");',
              '_mm_setcsr(0x1f80u);', 'return 0;', '}']
    return "\n".join(lines) + "\n", traps, len(checks)


def exercise(exe, traps):
    run([exe])
    printed = run([exe, "print"])
    assert printed.stdout == "-0x0.000000p+0\n-0x0.0000000000000p+0\nnan\n-inf\n"
    for index, (_, message) in enumerate(traps):
        result = subprocess.run([str(exe), str(index)], text=True,
                                capture_output=True, timeout=10)
        assert result.returncode == 70, (index, result.returncode, result.stderr)
        assert result.stderr == f"On1x arithmetic error: {message}\n", (
            index, result.stderr, message)
        assert not result.stdout, (index, result.stdout)


def compile_runtime(generator, build, cc, sanitize):
    flags = ["-std=c11", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
             "-I", ON1X / "runtime"]
    if sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                  "-fno-omit-frame-pointer", "-fno-pie"]
    objects = []
    source, traps, count = runtime_source(generator)
    (build / "caller.c").write_text(source)
    for name, source in (("runtime", ON1X / "runtime/on1x_runtime.c"),
                         ("scalar", BACKEND / "ScalarABI.c"),
                         ("caller", build / "caller.c")):
        obj = build / (name + ".o")
        run([cc, *flags, "-c", source, "-o", obj])
        objects.append(obj)
    exe = build / "scalar-test"
    linkflags = ["-no-pie"] if sanitize else []
    run([cc, *flags, *linkflags, *objects, "-lm", "-o", exe])
    exercise(exe, traps)
    # Object-level absence of floating wrappers on an unqualified target.
    for profile, options in (
            ("no-float", ["-U__linux__"]),
            ("f64-only", ["-include", build / "f64-only.h"])):
        (build / "f64-only.h").write_text(
            '#include <on1x_runtime.h>\n#undef ON1X_F32_ABI_VERSION\n')
        obj = build / (profile + ".o")
        run([cc, *flags, *options, "-c", BACKEND / "ScalarABI.c", "-o", obj])
        symbols = run(["nm", "-g", "--defined-only", obj]).stdout
        assert "on1x_archon_add_u64" in symbols
        assert "on1x_archon_f32" not in symbols
        assert "on1x_archon_u64_from_f64" not in symbols
        assert ("on1x_archon_add_f64" in symbols) == (profile == "f64-only")
    for option in ("-ffast-math", "-ffinite-math-only"):
        result = subprocess.run(
            [str(cc), *map(str, flags), option, "-c", str(BACKEND / "ScalarABI.c"),
             "-o", str(build / "forbidden.o")], text=True, capture_output=True)
        assert result.returncode != 0 and "forbid relaxed" in result.stderr, result.stderr
    print(f"runtime: {count} value comparisons, {len(traps)} exact traps, "
          "print/feature/fast-math checks")
    return flags, objects[:2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--cxx", default="c++")
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--native-tool", type=Path)
    options = parser.parse_args()
    options.build_dir.mkdir(parents=True, exist_ok=True)
    generator = load_generator()
    generator_checks(generator, options.build_dir)
    coverage(generator, options.build_dir, options.cxx)
    flags, objects = compile_runtime(generator, options.build_dir, options.cc,
                                     options.sanitize)
    if options.native_tool:
        from scalar_abi_native import native_boundaries
        native_boundaries(options.native_tool.resolve(), options.build_dir,
                          options.cc, flags, objects, options.sanitize, generator)


if __name__ == "__main__":
    main()
