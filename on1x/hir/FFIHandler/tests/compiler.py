#!/usr/bin/env python3
import hashlib
from pathlib import Path
import re
import runpy
import subprocess
import sys
import tempfile

root, parser, include, original, work = map(Path, sys.argv[1:])
source = original.read_text(encoding="ascii")
pin = hashlib.sha256((include / "mulib-clamp.h").read_bytes()).hexdigest()
generated_parser = runpy.run_path(str(root / "compile_recipes.py"))["load_parser"](parser)


def replace(text, old, new):
    if old not in text:
        raise AssertionError(f"test fixture no longer includes {old!r}")
    return text.replace(old, new, 1)


# The generated grammar itself, not only semantic review, rejects these.
generated_parser.parse(source)
for old, new in (
    ("(hint kernel-hot", "(hint external-code"),
    ("(hint kernel-hot", "(invent-directive kernel-hot"),
    ("(lt x lo)", "(arbitrary x lo)"),
    ("(lt x lo)", "(lt x lo hi)"),
    ("(hint private-code", "(hint private-code (speculative false)"),
    ("(hint private-code", "(hint private-code (target -bad)"),
):
    try:
        generated_parser.parse(replace(source, old, new))
    except generated_parser.LanguageError:
        pass
    else:
        raise AssertionError(f"LLSynth accepted unsupported syntax: {new}")


with tempfile.TemporaryDirectory(prefix="recipe-test-", dir=work) as temp:
    temp = Path(temp)
    spec = temp / "clamp.ffi"
    output = temp / "recipes.hpp"

    def generate(text, headers=include):
        spec.write_text(text, encoding="ascii")
        return subprocess.run([sys.executable, "-IB", root / "compile_recipes.py",
                               "--parser", parser, "--grammar", root / "FFIHandler.llsynth",
                               "--review", root / "reviewed.json",
                               "--include", headers, "--output", output, spec],
                              capture_output=True, text=True, timeout=10)

    initial = generate(source)
    assert initial.returncode == 0, initial.stderr
    generated = output.read_bytes()
    assert generate(source).returncode == 0 and output.read_bytes() == generated
    renamed = replace(source, "(params x lo hi)", "(params value lo hi)")
    renamed = replace(renamed, "(if (lt x lo) lo (if (gt x hi) hi x))",
                      "(if (lt value lo) lo (if (gt value hi) hi value))")
    assert generate("; equivalent input\n" + renamed).returncode == 0
    assert output.read_bytes().split(b"\n", 1)[1] == generated.split(b"\n", 1)[1]
    assert generate(source).returncode == 0 and output.read_bytes() == generated
    assert b"mulib_clamp_add_i64" in generated and b"(body" not in generated
    for old, new, message in (
        ("(ffispec 1", "(ffispec 2", "version 1"),
        (pin, "0" * 64, "unreviewed header or SHA-256"),
        ("mulib-clamp.h", "other.h", "unreviewed header"),
        ("mulib_clamp_i64", "mulib_clamp_fake", "unreviewed C symbol"),
        ("(guard (le lo hi))", "(guard true)", "unreviewed guard or body"),
        ("(lt x lo)", "(le x lo)", "unreviewed guard or body"),
        ("(lt x lo)", "(lt x (call injected))", "invalid standard FFI recipe"),
        ("(lt x lo)", "(lt x true)", "invalid i64 operand"),
        ("(lt x lo)", "(lt x 9223372036854775808)", "i64 literal out of range"),
        ("(params x lo hi)", "(params x lo lo)", "invalid parameters"),
        ("(params x lo hi)", "(params x lo hi) (params x lo hi)", "duplicate field"),
        ("(hint kernel-hot", "(hint external-code", "invalid standard FFI recipe"),
        ("(hint kernel-hot", "(hint kernel-hot (backend -bad)", "invalid standard FFI recipe"),
        ("(hint private-code", "(hint private-code (speculative false)", "invalid standard FFI recipe"),
        ("(hint private-code", "(hint private-code (speculative) (speculative)", "duplicate field"),
        ("(guard true)", '(guard "injected")', "invalid standard FFI recipe"),
    ):
        run = generate(replace(source, old, new))
        assert run.returncode != 0 and message in run.stderr, (old, new, run.stderr)
        assert output.read_bytes() == generated, "failed admission overwrote valid output"
    run = generate(source + " (ignored trailing source)")
    assert run.returncode != 0
    for marker, value in (("", b"false"), (" (speculative)", b"true")):
        variant = re.sub(r"\(rewrite fold-identity(?: \(speculative\))?\)",
                         "(rewrite fold-identity (backend archon) (target riscv64-linux-gnu)" + marker + ")",
                         source, count=1)
        assert variant != source
        run = generate(variant)
        assert run.returncode == 0, run.stderr
        assert b'{"archon","riscv64-linux-gnu","rewrite","fold-identity",' + value + b'}' in output.read_bytes()
    variant = replace(source, "(hint private-code", "(hint private-code (speculative)")
    run = generate(variant)
    assert run.returncode == 0, run.stderr
    assert b'{"mir-c11","","hint","private-code",true}' in output.read_bytes()
    assert b'{"mir-c11","","hint","private-code",false}' in output.read_bytes()
    stale = temp / "kernels"
    stale.mkdir()
    edited = (include / "mulib-clamp.h").read_bytes() + b"\n/* edited */\n"
    (stale / "mulib-clamp.h").write_bytes(edited)
    run = generate(source, stale)
    assert run.returncode != 0 and "pinned header SHA-256 mismatch" in run.stderr
    run = generate(replace(source, pin, hashlib.sha256(edited).hexdigest()), stale)
    assert run.returncode != 0 and "pinned header SHA-256 mismatch" in run.stderr
