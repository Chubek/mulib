#!/usr/bin/env python3
"""Relocate an On1x installation and qualify operators without source-tree paths."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def run(env, cwd, *argv, status=0, stdout=None, source=None):
    result = subprocess.run(argv, env=env, cwd=cwd, capture_output=True,
                            text=True, input=source, timeout=90)
    if result.returncode != status or (stdout is not None and result.stdout != stdout):
        raise RuntimeError(
            f"{argv!r}: expected exit {status}, got {result.returncode}\n"
            f"stdout: {result.stdout!r}\nstderr: {result.stderr}"
        )
    return result.stdout


def qualify(args, work):
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("ON1X_", "MULIB_"))}
    env.update(CC=args.clang, CXX=args.clangxx)
    if "clang" not in run(env, work, args.clang, "--version").splitlines()[0].lower():
        raise RuntimeError("--clang must name a Clang C compiler")

    original, moved = work / "original", work / "relocated on1x"
    run(env, work, args.cmake, "--install", args.build, "--prefix", original)
    original.rename(moved)
    compiler = moved / "bin/on1xc"
    stdlib = moved / "share/on1x/stdlib"
    runtime = moved / "share/on1x/runtime"
    library = moved / "lib/libon1x_runtime.a"
    if not library.is_file():
        library = runtime / "on1x_runtime.c"
    output = run(env, work, compiler, "--print-stdlib")
    if Path(output.strip()).resolve() != stdlib:
        raise RuntimeError(f"relocated executable did not use installed stdlib: {output!r}")

    def native(source, mode, extra=()):
        artifact = work / (source.stem + (".s" if extra else ".c"))
        exe = work / (source.stem + ".exe")
        run(env, work, compiler, *mode, source, "-o", artifact)
        run(env, work, args.clang, "-std=c11", "-I", runtime, artifact, *extra,
            library, "-lm", "-o", exe)
        run(env, work, exe, stdout="42\n")

    pipeline = work / "pervasive.on1x"
    pipeline.write_text(
        "fn plus_one(x: i64) -> i64 { return x + 1; }\n"
        "fn main() { println(41 |> plus_one); }\n", encoding="utf-8")
    native(pipeline, ("--emit-c",))
    paths = [Path(line).resolve()
             for line in run(env, work, compiler, "--deps", pipeline).splitlines()]
    if not any(path.is_relative_to(stdlib) and
               ("operators" in path.parts or path.name == "operators.on1x")
               for path in paths):
        raise RuntimeError("installed std.operators missing from dependency graph")
    run(env, work, compiler, "--no-prelude", "--check", pipeline, status=1)
    installed_surface = json.loads(run(env, work, compiler, "--source-only",
                                       "--no-prelude", "--operator-info",
                                       stdlib / "operators/__init__.on1x"))
    standard = installed_surface["declarations"]
    if ({item["lexeme"] for item in standard} !=
        {"|>", "<|", "<<.", ">>.", ">>=", "=<<", "<$>", "<&>", ">=>", "<=<"}
        or not all(item["signature"] for item in standard)):
        raise RuntimeError(f"installed operator module surface is incomplete: {standard!r}")

    override = work / "override.on1x"
    override.write_text(
        'operator (lexeme: ">>=", arity: "binary", tightness: "2", '
        'binds: "left", precedence: 4, formals: p1, p2) { return p2(p1); }\n'
        "fn plus_one(x: i64) -> i64 { return x + 1; }\n"
        "fn main() { println(41 >>= plus_one); }\n", encoding="utf-8")
    native(override, ("--emit-c",))

    scalar = work / "scalar.on1x"
    scalar.write_text(
        'operator (lexeme: "%%", arity: "binary", tightness: 2, binds: "left", '
        'precedence: 4, formals: x: i64, y: i64) -> i64 { return x * 10 + y; }\n'
        "fn main() { println(4 %% 2); }\n", encoding="utf-8")
    inspected = json.loads(run(env, work, compiler, "--no-prelude", "--operator-info", scalar))
    declaration = inspected["declarations"][0]
    use = inspected["uses"][0]
    if (inspected["version"], declaration["lexeme"], declaration["arity"],
        declaration["tightness"], declaration["signature"], use["declaration"],
        scalar.read_bytes()[use["offset"]:use["offset"] + use["length"]]) != (
            1, "%%", 2, 2, "fn(i64, i64) -> i64", 0, b"%%"):
        raise RuntimeError(f"unexpected checked operator inspection: {inspected!r}")
    report = json.loads(run(env, work, compiler, "--no-prelude", "--disable-opt", "all",
                            "--dump-processor", scalar))
    desugar = report["source_desugar"]
    if not (desugar["verified"] and desugar["definitions"] == 1 and desugar["rewrites"] == 2
            and {rule["name"] for rule in desugar["rules"]} ==
            {"operator_binary", "operator_definition"}):
        raise RuntimeError(f"missing verified On1xMETA source desugaring: {desugar!r}")
    for mode in ("--run-ir", "--run-vm"):
        run(env, work, compiler, "--no-prelude", mode, scalar, stdout="42\n")
    mir = work / "scalar.mir"
    run(env, work, compiler, "--no-prelude", "--emit-ir", scalar, "-o", mir)
    for mode in ("--run-ir", "--run-vm"):
        run(env, work, compiler, "--ir-input", mode, mir, stdout="42\n")
    for mode in ("--emit-c", "--emit-ir-c"):
        native(scalar, ("--no-prelude", mode))

    processors = moved / "libexec/on1x"
    fold = processors / "on1x-ppx-fold"
    if fold.is_file():
        folding = work / "folding.on1x"
        folding.write_text(scalar.read_text(encoding="utf-8").replace(
            "println(4 %% 2)", "println((2 + 2) %% 2)"), encoding="utf-8")
        wire = run(env, work, compiler, "--no-prelude", "--emit-ast", folding)
        transformed = run({**env, "ON1X_PPX_LABEL": "fold", "ON1X_PPX_PROTOCOL": "1",
                           "ON1X_PPX_SCHEMA": str(processors / "category.schema.json")},
                          work, fold, source=wire)
        if ("(Binary + " not in wire or transformed.count("(Binary + ") !=
            wire.count("(Binary + ") - 1 or "(OperatorBinary " not in transformed):
            raise RuntimeError("installed native PPX did not fold the custom operator's operand")
        native(folding, ("--no-prelude", "--use-ppx", "fold", "--emit-c"))
        print("Installed native PPX fold inside custom operator application: PASS")

    if args.archon_target:
        native(scalar, ("--no-prelude", "--sdk-emit", "mir-archon",
                        "--sdk-target", args.archon_target),
               (moved / "lib/libon1x_archon_scalar_abi.a",))
    print("Installed operator prelude, C11, MIR evaluator, VMWeave and saved MIR: PASS")
    if args.archon_target:
        print(f"Installed Archon scalar operator ({args.archon_target}): PASS")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--clang", default="clang")
    parser.add_argument("--clangxx", default="clang++")
    parser.add_argument("--archon-target", help="add the configured Archon SDK target")
    args = parser.parse_args()
    args.build = args.build.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="installed-operators-", dir=args.build) as work:
        qualify(args, Path(work))


if __name__ == "__main__":
    main()
