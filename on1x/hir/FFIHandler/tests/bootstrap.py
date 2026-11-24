#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys
import tempfile

root, include, spec, work = map(Path, sys.argv[1:5])
parser = Path(sys.argv[5]) if len(sys.argv) > 5 else None
bootstrap = root / "generated/bootstrap.hpp"
grammar = root / "FFIHandler.llsynth"
review = root / "reviewed.json"

with tempfile.TemporaryDirectory(prefix="bootstrap-test-", dir=work) as path:
    temp = Path(path)
    output = temp / "tables.hpp"

    def compile(mode="--bootstrap", asset=bootstrap, grammar=grammar,
                review=review, include=include, spec=spec):
        return subprocess.run([sys.executable, "-IB", root / "compile_recipes.py",
                               mode, asset, "--grammar", grammar, "--review", review,
                               "--include", include, "--output", output, spec],
                              text=True, capture_output=True, timeout=10)

    run = compile()
    assert run.returncode == 0, run.stderr
    published = output.read_bytes()
    if parser:
        run = compile("--parser", parser)
        assert run.returncode == 0 and published == output.read_bytes(), run.stderr

    for argument, original in (("asset", bootstrap), ("grammar", grammar),
                               ("review", review), ("spec", spec)):
        edited = temp / original.name
        edited.write_bytes(original.read_bytes() + b"\n")
        run = compile(**{argument: edited})
        assert run.returncode != 0 and "stale FFIHandler bootstrap" in run.stderr, run.stderr
        assert output.read_bytes() == published

    changed = temp / "kernels"
    changed.mkdir()
    header = include / "mulib-clamp.h"
    (changed / header.name).write_bytes(header.read_bytes() + b"\n")
    run = compile(include=changed)
    assert run.returncode != 0 and "pinned header SHA-256 mismatch" in run.stderr, run.stderr
    assert output.read_bytes() == published
