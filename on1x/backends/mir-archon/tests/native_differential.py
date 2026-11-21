#!/usr/bin/env python3
"""Real Archon assembly -> host assembler/linker -> native execution differential.

All subprocesses are explicit, bounded, and recorded. C11 is an independent
reference executable; it is never substituted for a failed native compilation.
"""
import argparse
import json
from pathlib import Path
import random
import subprocess
import sys


def run(command, *, success=True):
    command = [str(x) for x in command]
    result = subprocess.run(command, capture_output=True, timeout=30)
    if success and result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {command!r}\n"
                           + result.stderr.decode(errors="replace"))
    return result


def check(ok, message):
    if not ok:
        raise RuntimeError(message)


def generated_sources():
    """Cover every admitted integer width and numeric intrinsic family."""
    for width in (8, 16, 32, 64):
        t = f"u{width}"
        maximum = (1 << width) - 1
        statements = [f"println({maximum}{t});"]
        for op in ("+", "-", "*", "/", "%", "==", "!=", "<", "<=", ">", ">="):
            statements.append(f"println(7{t}{op}3{t});")
        statements += [f"println({maximum}{t}>1{t});",
                       f"println(0{t}<{maximum}{t});"]
        for op in ("wrapping_add", "wrapping_sub", "wrapping_mul", "and", "or", "xor"):
            statements.append(f"println({t}_{op}({maximum}{t},2{t}));")
        statements += [f"println({t}_shl(1{t},{width-1}));",
                       f"println({t}_shr({maximum}{t},{width-1}));"]
        statements += [f"println({t}_not(0{t}));", f"println({t}_to_bits({t}_from_bits(-1)));",
                       f"println({t}_from_i64(42));", f"println({t}_to_i64(42{t}));"]
        for other in (8, 16, 32, 64):
            statements.append(f"println({t}_from_u{other}(42u{other}));")
        for fp in ("f32", "f64"):
            statements += [f"println({fp}_from_{t}(42{t}));",
                           f"println({t}_from_{fp}(42.75{fp}));"]
        yield t, "fn main(){" + "".join(statements) + "}"
        for name, expr in (
            ("overflow", f"{maximum}{t}+1{t}"), ("underflow", f"0{t}-1{t}"),
            ("multiply", f"{maximum}{t}*2{t}"), ("divide_zero", f"1{t}/0{t}"),
            ("remainder_zero", f"1{t}%0{t}"), ("shift_width", f"{t}_shl(1{t},{width})"),
            ("shift_overflow", f"{t}_shl({maximum}{t},1)"),
            ("negative_cast", f"{t}_from_i64(-1)"),
        ):
            yield f"trap_{t}_{name}", f"fn main(){{println(123);println({expr});println(999);}}"
    for fp, bits in (("f32", (0, 2147483648, 1, 8388607, 8388608, 2139095039,
                             2139095040, 4286578688, 2143289344)),
                     ("f64", (0, -9223372036854775808, 1, 4503599627370495,
                              4503599627370496, 9218868437227405311,
                              9218868437227405312, -4503599627370496, 9221120237041090560))):
        statements = []
        # INT64_MIN is represented as an expression accepted by the canonical lexer.
        for index, bit in enumerate(bits):
            literal = "(-9223372036854775807-1)" if bit == -(1 << 63) else str(bit)
            v = f"x{index}"
            statements.append(f"let {v}={fp}_from_bits({literal});")
            for expr in (v, f"{fp}_to_bits({v})", f"{fp}_is_nan({v})",
                         f"{fp}_is_finite({v})", f"{fp}_signbit({v})",
                         f"{v}==0.0{fp}", f"{v}<0.0{fp}", f"{v}!=0.0{fp}",
                         f"{v}<=0.0{fp}", f"{v}>0.0{fp}", f"{v}>=0.0{fp}",
                         f"{fp}_to_bits({v}+0.0{fp})", f"{fp}_to_bits(-{v})"):
                statements.append(f"println({expr});")
        for op in ("+", "-", "*", "/", "==", "!=", "<", "<=", ">", ">="):
            statements.append(f"println(1.5{fp}{op}2.0{fp});")
        statements += [f"println({fp}_to_bits(0.0{fp}/0.0{fp}));",
                       f"println({fp}_to_bits(1.0{fp}/(-0.0{fp})));",
                       f"println({fp}_from_i64(42));", f"println({fp}_to_i64(-42.75{fp}));"]
        yield fp + "_special", "fn main(){" + "".join(statements) + "}"
        yield f"trap_{fp}_nan_cast", f"fn main(){{println(123);println({fp}_to_i64(0.0{fp}/0.0{fp}));}}"
    yield "float_width_conversion", "fn main(){println(f32_from_f64(1.5));println(f64_from_f32(-0.0f32));}"
    for name, expr in (
        ("division_min", "(-9223372036854775807-1)/(-1)"),
        ("remainder_min", "(-9223372036854775807-1)%(-1)"),
        ("division_zero", "1/0"), ("remainder_zero", "1%0"),
        ("add_overflow", "9223372036854775807+1"),
        ("subtract_overflow", "(-9223372036854775807-1)-1"),
        ("multiply_overflow", "9223372036854775807*2"),
        ("negate_min", "-(-9223372036854775807-1)"),
    ):
        yield "trap_i64_" + name, f"fn main(){{println(123);println({expr});println(999);}}"
    yield "trap_assert", "fn main(){println(123);assert(false);println(999);}"


def unsigned_optimization_oracle():
    """Worker 2's 588 boundary/seeded results, independently computed in Python."""
    rng = random.Random(0x415243484F4E)
    functions, expected = [], []
    for width in (8, 16, 32, 64):
        mask, t = (1 << width) - 1, f"u{width}"
        pairs = [(0, 0), (0, 1), (mask, 1), (mask, mask), (1 << (width - 1), mask)]
        pairs += [(rng.getrandbits(width), rng.getrandbits(width)) for _ in range(16)]
        body = []
        for a, b in pairs:
            for op, value in (
                ("wrapping_add", a + b), ("wrapping_sub", a - b),
                ("wrapping_mul", a * b), ("and", a & b), ("or", a | b), ("xor", a ^ b),
            ):
                body.append(f"println({t}_{op}({a}{t},{b}{t}));")
                expected.append(str(value & mask))
            body.append(f"println({t}_not({a}{t}));")
            expected.append(str((~a) & mask))
        functions.append(f"fn test{width}(){{" + "".join(body) + "}")
    return ("".join(functions) + "fn main(){test8();test16();test32();test64();}",
            ("\n".join(expected) + "\n").encode())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("fixture-tool", "archon-tool", "on1xc", "runtime", "scalar-abi",
                 "runtime-include", "work"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--sdk-target",
                        help="also execute SDK 2 native production, with and without MIR optimization")
    parser.add_argument("--sanitize", action="store_true",
                        help="compile runtime, scalar ABI, and reference/native drivers with ASan/UBSan")
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    args.work = args.work.resolve()
    here = Path(__file__).resolve().parent
    flags = ["-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
             "-I", args.runtime_include]
    link_flags = ["-lm"]
    runtime, scalar = args.runtime, args.scalar_abi
    if args.sanitize:
        san = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        flags += san
        link_flags += ["-fsanitize=address,undefined"]
        runtime, scalar = args.work / "runtime.o", args.work / "scalar.o"
        for source, output in ((args.runtime_include / "on1x_runtime.c", runtime),
                               (here.parent / "ScalarABI.c", scalar)):
            run([args.cc, *flags, "-c", source, "-o", output])
    sources = list(sorted((here / "fixtures").glob("*.on1x")))
    for name, source in generated_sources():
        path = args.work / (name + ".on1x")
        path.write_text(source)
        sources.append(path)
    oracle_source, oracle_output = unsigned_optimization_oracle()
    oracle_path = args.work / "unsigned_optimization.on1x"
    oracle_path.write_text(oracle_source)
    sources.append(oracle_path)
    results, failures = [], []

    def exercise(name, source=None):
        base = args.work / name
        run([args.fixture_tool, "--source" if source else "--phi", source or name, base])
        if source:
            # Exercise the actual public source CLI as well as in-process lowering.
            run([args.on1xc, "--emit-ir", source, "-o", str(base) + ".cli.mir"])
            run([args.archon_tool, "--ir-input", str(base) + ".cli.mir",
                 "-o", str(base) + ".cli.s"])
            run([args.on1xc, "--emit-c", source, "-o", str(base) + ".hir.c"])
            run([args.on1xc, "--emit-ir-c", "--optimize-ir", source,
                 "-o", str(base) + ".opt.c"])
            if args.sdk_target:
                for optimize, suffix in (([], ".sdk.s"), (["--optimize-ir"], ".sdk-opt.s")):
                    run([args.on1xc, "--sdk-emit", "mir-archon", "--sdk-target", args.sdk_target,
                         *optimize, source, "-o", str(base) + suffix])
        # The standalone tool must match the same verified serialized module.
        run([args.archon_tool, "--ir-input", str(base) + ".mir", "-o", str(base) + ".tool.s"])
        check(Path(str(base) + ".s").read_bytes() == Path(str(base) + ".tool.s").read_bytes(),
              name + ": bridge/tool assembly mismatch")
        expected = Path(str(base) + ".stdout").read_bytes()
        if name == "unsigned_optimization":
            check(expected == oracle_output, "unsigned evaluator disagrees with independent Python oracle")
        status = Path(str(base) + ".status").read_text().strip()
        check((status == "trap") == name.startswith("trap_"),
              f"{name}: fixture unexpectedly {'trapped' if status == 'trap' else 'returned'}")
        diagnostic = Path(str(base) + ".diagnostic").read_bytes()
        outcomes = {}
        paths = [("native", ".s"), ("mir_c", ".c")]
        if source:
            paths += [("cli_native", ".cli.s"), ("hir_c", ".hir.c"), ("optimized_c", ".opt.c")]
            if args.sdk_target:
                paths += [("sdk_native", ".sdk.s"), ("sdk_optimized", ".sdk-opt.s")]
        for kind, suffix in paths:
            exe = Path(str(base) + "." + kind)
            native = suffix.endswith(".s")
            run([args.cc, *([] if native else flags), str(base) + suffix,
                 *([scalar] if native else []), runtime, *link_flags, "-o", exe])
            execution = run([exe], success=False)
            outcomes[kind] = execution
            check(b"Sanitizer" not in execution.stderr and b"runtime error:" not in execution.stderr,
                  f"{name}/{kind}: sanitizer finding: {execution.stderr!r}")
            check(execution.stdout == expected,
                  f"{name}/{kind}: stdout {execution.stdout!r} != evaluator {expected!r}")
            if status != "trap":
                check(execution.returncode == int(status),
                      f"{name}/{kind}: status {execution.returncode} != evaluator {status}")
                check(not execution.stderr, f"{name}/{kind}: unexpected stderr {execution.stderr!r}")
            else:
                check(execution.returncode > 0 and execution.stderr,
                      f"{name}/{kind}: missing controlled runtime trap")
                check(diagnostic and diagnostic in execution.stderr,
                      f"{name}/{kind}: trap differs from evaluator {diagnostic!r}: {execution.stderr!r}")
        reference = outcomes["mir_c"]
        for kind, execution in outcomes.items():
            check((execution.returncode, execution.stderr) == (reference.returncode, reference.stderr),
                  f"{name}/{kind}: exit/trap diagnostic differs from C11: {execution.stderr!r}")
        return {"name": name, "status": status, "stdout_bytes": len(expected),
                "target": Path(str(base) + ".target").read_text()}

    for source in sources:
        try:
            result = exercise(source.stem, source)
            results.append(result)
            print("PASS", result["name"], flush=True)
        except (RuntimeError, subprocess.TimeoutExpired) as error:
            failures.append({"name": source.stem, "error": str(error)})
            print("FAIL", source.stem, error, file=sys.stderr, flush=True)
    for name in ("phi_pair", "phi_pair_false", "phi_nested", "phi_nested_false"):
        try:
            result = exercise(name)
            results.append(result)
            print("PASS", name, flush=True)
        except (RuntimeError, subprocess.TimeoutExpired) as error:
            failures.append({"name": name, "error": str(error)})
            print("FAIL", name, error, file=sys.stderr, flush=True)

    # Malformed input, missing output directory, and rename failure must retain
    # existing output bytes and remove every temporary publication file.
    publication = args.work / "publication.s"
    publication.write_bytes(b"existing-artifact\n")
    malformed = args.work / "malformed.mir"
    for text in ("", "(on1x.mir 999 1 ())", "(on1x.mir 1 -1 ())",
                 "(on1x.mir 1 1 ()) trailing", "(" * 2000):
        malformed.write_text(text)
        result = run([args.archon_tool, "--ir-input", malformed, "-o", publication], success=False)
        check(result.returncode != 0 and not result.stdout, "malformed input published output")
        check(publication.read_bytes() == b"existing-artifact\n", "failed compile clobbered artifact")
    good = args.work / "phi_pair.mir"
    # A positive publication first prevents an unrelated tool/configuration
    # failure from making all negative publication checks spuriously pass.
    run([args.archon_tool, "--ir-input", good, "-o", args.work / "publication-good.s"])
    directory = args.work / "rename-directory"
    directory.mkdir(exist_ok=True)
    for output in (directory, args.work / "missing-parent" / "out.s"):
        result = run([args.archon_tool, "--ir-input", good, "-o", output], success=False)
        check(result.returncode != 0, "invalid publication unexpectedly succeeded")
    check(not list(args.work.glob("*.tmp.*")), "failed publication leaked temporary file")
    negative_source = args.work / "rejected.on1x"
    negative_output = args.work / "rejected.mir"
    for text in ("fn main(){let x=();}", "fn f(x:i8){}", "fn main(){println(256u8);}",
                 "fn main(){println(missing);}", "fn main(){let x=(1,true;", "fn main(){if 1{}}"):
        negative_source.write_text(text)
        negative_output.write_bytes(b"existing-mir\n")
        result = run([args.on1xc, "--emit-ir", negative_source, "-o", negative_output], success=False)
        check(result.returncode != 0 and result.stderr, "malformed/unsupported source accepted")
        check(negative_output.read_bytes() == b"existing-mir\n", "bad source clobbered output")
    report = {"passed": results, "failed": failures, "publication_checks": "passed",
              "sanitized_runtime": args.sanitize, "cc": args.cc, "sdk_target": args.sdk_target,
              "independent_unsigned_oracle_results": 588}
    (args.work / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{len(results)} differential cases passed; {len(failures)} failed; publication checks passed")
    return bool(failures)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
