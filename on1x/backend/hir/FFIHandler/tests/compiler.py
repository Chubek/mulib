#!/usr/bin/env python3
import hashlib
import json
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile


RECIPE = "recipes/ffifixture.ffi"
BODY = "(if (lt x 9223372036854775807) (add x 1) x)"


def write_fixture(root, output):
    header = (root / "tests/mulib-ffifixture.h").resolve(strict=True)
    spec = output / RECIPE
    spec.parent.mkdir(exist_ok=True, parents=True)
    spec.write_bytes((root / "tests/directives.ffi").read_bytes())
    record = {"file": RECIPE, "header": str(header)}
    (output / "index.json").write_text(json.dumps({"version": 1, "recipes": [record]}), encoding="utf-8")
    return record


def replace(text, old, new):
    if old not in text:
        raise AssertionError(f"test fixture no longer includes {old!r}")
    return text.replace(old, new, 1)


def test_compiler(root, parser, work):
    source = (root / "tests/directives.ffi").read_text(encoding="ascii")
    header = (root / "tests/mulib-ffifixture.h").resolve()
    pin = hashlib.sha256(header.read_bytes()).hexdigest()
    compiler = runpy.run_path(str(root / "compile_recipes.py"))
    generated_parser = compiler["load_parser"](parser)

    # LLSynth, not just table admission, rejects unsupported source syntax.
    generated_parser.parse(source)
    for old, new in (
        ("(hint kernel-hot", "(hint external-code"),
        ("(rewrite memgraph-local", "(rewrite unknown-bank"),
        ("(rewrite meta-canonical", "(invent-directive meta-canonical"),
        ("(lt x 0)", "(arbitrary x 0)"),
        ("(lt x 0)", "(lt x 0 1)"),
        ("(hint private-code", "(hint private-code (speculative false)"),
        ("(hint private-code", "(hint private-code (target -bad)"),
    ):
        try:
            generated_parser.parse(replace(source, old, new))
        except generated_parser.LanguageError:
            pass
        else:
            raise AssertionError(f"LLSynth accepted unsupported syntax: {new}")

    with tempfile.TemporaryDirectory(prefix="recipe-test-", dir=work) as directory:
        temp = Path(directory)
        record = write_fixture(root, temp)
        index, spec, output = temp / "index.json", temp / RECIPE, temp / "recipes.hpp"

        def compile_tables():
            return subprocess.run([sys.executable, "-IB", root / "compile_recipes.py",
                                   "--parser", parser, "--generated-index", index, "--output", output],
                                  capture_output=True, text=True, timeout=10)

        def generate(text=source, records=None):
            spec.write_text(text, encoding="ascii")
            index.write_text(json.dumps({"version": 1, "recipes": [record] if records is None else records,
                                         "catalogue": [{"file": "not-loaded.ffi"}]}), encoding="utf-8")
            return compile_tables()

        initial = generate()
        assert initial.returncode == 0, initial.stderr
        generated = output.read_bytes()
        initial_mtime = output.stat().st_mtime_ns
        assert b"mulib_ffifixture_bump" in generated and b"(body" not in generated
        assert b'{"mir-c11","x86_64-unknown-linux-gnu","rewrite","memgraph-local",false}' in generated
        assert b'{"mir-vm","","rewrite","meta-canonical",true}' in generated
        assert generate().returncode == 0 and output.read_bytes() == generated
        renamed = replace(source, "(params x)", "(params input)")
        renamed = replace(renamed, BODY, BODY.replace(" x", " input"))
        assert generate("; equivalent input\n" + renamed).returncode == 0
        assert output.read_bytes() == generated
        assert output.stat().st_mtime_ns == initial_mtime, "equivalent registry forced a rebuild"

        for old, new, message in (
            ("(ffispec 1", "(ffispec 2", "version 1"),
            (pin, "0" * 64, "header SHA-256 mismatch"),
            ("mulib-ffifixture.h", "other.h", "differs from the indexed header"),
            ("(kernel ffifixture)", "(kernel other)", "kernel differs"),
            ("mulib_ffifixture_floor", "mulib_ffifixture_bump", "duplicate export member or C symbol"),
            ("(lt x 0)", "(lt x true)", "invalid i64 operand"),
            ("(lt x 0)", "(lt x 9223372036854775808)", "i64 literal out of range"),
            ("(body 0)", "(body true)", "invalid i64 operand"),
            ("(guard true)", "(guard (add x 1))", "wrong arity, type, or position"),
            ("(params)", "", "invalid signed i64 signature"),
            ("(params x)", "(params x x)", "invalid signed i64 signature"),
            ("(params x)", "(params x) (params x)", "duplicate field"),
            ("(hint kernel-hot", "(hint kernel-hot (backend -bad)", "invalid standard FFI recipe"),
            ("(hint private-code", "(hint private-code (speculative false)", "invalid standard FFI recipe"),
            ("(hint private-code", "(hint private-code (speculative) (speculative)", "duplicate field"),
            ("(hint no-inline)", "(hint no-inline (backend *))", "requires effective backend mir-c11"),
            ("(hint no-inline)", "(hint no-inline (backend mir-vm))", "requires effective backend mir-c11"),
            ("(rewrite memgraph-local)", "(rewrite memgraph-local) (rewrite memgraph-local (backend mir-c11))",
             "duplicate FFI directive"),
            ("(ffispec 1", "(ffi-catalogue 1", "invalid standard FFI recipe"),
            (BODY, "(add x 1)", "cannot prove signed i64 add does not overflow"),
            (BODY, "(if (lt x 9223372036854775807) x (add x 1))",
             "cannot prove signed i64 add does not overflow"),
            (f"(guard true)\n    (body {BODY})",
             "(guard (lt x 9223372036854775807))\n    (body (add x 1))",
             "cannot prove signed i64 add does not overflow"),
        ):
            run = generate(replace(source, old, new))
            assert run.returncode != 0 and message in run.stderr, (old, new, run.stderr)
            assert output.read_bytes() == generated, "failed admission overwrote valid output"

        run = generate(source + " (ignored trailing source)")
        assert run.returncode != 0
        oversized = "(lt x 0)"
        for _ in range(8):
            oversized = f"(and {oversized} {oversized})"
        run = generate(replace(source, "(guard true)", f"(guard {oversized})"))
        assert run.returncode != 0 and "expression depth or node limit" in run.stderr
        assert output.read_bytes() == generated

        edited = temp / header.name
        edited.write_bytes(header.read_bytes() + b"\n/* edited */\n")
        alias = temp / "alias"
        alias.mkdir()
        alias_header = alias / header.name
        alias_header.symlink_to(header)
        for invalid, message in (
            ([record, record], "duplicate generated recipe"),
            ([{**record, "file": "../ffifixture.ffi"}], "invalid recipe path"),
            ([{**record, "file": "catalogue/mulib-ffifixture.ffi"}], "invalid recipe path"),
            ([{**record, "header": header.name}], "not a canonical header path"),
            ([{**record, "header": str(temp / "../" / temp.name / header.name)}], "not a canonical header path"),
            ([{**record, "header": str(alias_header)}], "not a canonical header path"),
            ([{**record, "header": str(edited)}], "header SHA-256 mismatch"),
            ([{**record, "sha256": "0" * 64}], "header SHA-256 mismatch"),
        ):
            run = generate(records=invalid)
            assert run.returncode != 0 and message in run.stderr, run.stderr
            assert output.read_bytes() == generated
        for invalid, message in (
            ('{"version":true,"recipes":[]}', "expected generated recipe index version 1"),
            ('{"version":1,"recipes":{} }', "expected bounded recipes array"),
            ('{"version":1,"recipes":[],"recipes":[]}', "duplicate JSON field"),
            ('{"version":1,"headers":[]}', "expected bounded recipes array"),
        ):
            index.write_text(invalid, encoding="utf-8")
            run = compile_tables()
            assert run.returncode != 0 and message in run.stderr, run.stderr
            assert output.read_bytes() == generated
        assert generate().returncode == 0
        recipe_alias = alias / spec.name
        spec.rename(recipe_alias)
        spec.symlink_to(recipe_alias)
        run = compile_tables()
        assert run.returncode != 0 and "not a canonical recipe path" in run.stderr, run.stderr
        assert output.read_bytes() == generated
        spec.unlink()
        run = generate(records=[])
        assert run.returncode == 0, run.stderr
        assert b"std::array<Export,0>" in output.read_bytes()

    # Exact normalized linear forms and short-circuit facts prove bounded
    # saturating arithmetic without relying on a guard or a rewrite directive.
    prove = compiler["prove_i64_body"]
    lo, hi = -(1 << 63), (1 << 63) - 1
    for add, op, limit_op, high_sign, low_sign in (
        (True, "add", "sub", "gt", "lt"),
        (False, "sub", "add", "lt", "gt"),
    ):
        a, b = "$0", "$1"
        neg = (low_sign, b, 0)
        pos = (high_sign, b, 0)
        below = ("lt", a, (limit_op, lo, b))
        above = ("gt", a, (limit_op, hi, b))
        prove(("if", ("and", pos, above), hi, ("if", ("and", neg, below), lo, (op, a, b))))
        try:
            prove(("if", ("and", above, pos), hi, ("if", ("and", neg, below), lo, (op, a, b))))
        except ValueError:
            pass
        else:
            raise AssertionError(f"unsafe evaluation order admitted for {'addition' if add else 'subtraction'}")


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "--fixture-index":
        write_fixture(*map(Path, sys.argv[2:]))
    else:
        test_compiler(*map(Path, sys.argv[1:]))
