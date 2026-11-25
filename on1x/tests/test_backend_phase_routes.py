"""Check explicit product selection, source admission, and serialized re-entry."""

import argparse
from pathlib import Path
import shutil
import subprocess


parser = argparse.ArgumentParser(description=__doc__)
for name in ("compiler", "runner", "root", "work", "cc"):
    parser.add_argument(name, type=Path)
parser.add_argument("--cflag", action="append", default=[])
args = parser.parse_args()
compiler, runner, root, work, cc = args.compiler, args.runner, args.root, args.work, args.cc
work.mkdir(parents=True, exist_ok=True)
source = root / "examples/mir.on1x"
expected = b"1\n1\n2\n6\n24\n120\nfalse\n"
mir_modes = ("--emit-ir", "--ir-cfg", "--emit-ir-c", "--run-ir", "--run-vm", "--emit-vm")


def run(*args, status=0):
    result = subprocess.run(args, capture_output=True, timeout=30)
    assert result.returncode == status, (args, result.returncode, result.stderr)
    return result


def execute_c(output, artifact):
    cfile = work / (output + ".c")
    executable = work / output
    cfile.write_bytes(artifact)
    runtime = root / "runtime"
    run(cc, "-std=c11", "-I", runtime, cfile, runtime / "on1x_runtime.c",
        "-lm", *args.cflag, "-o", executable)
    assert run(executable).stdout == expected


preserved = work / "preserved"
marker = b"previous artifact\n"
for first, second in [
    ("--run-ir", "--emit-vm"),
    ("--emit-c", "--emit-ir-c"),
    ("--ir-cfg", "--emit-hir"),
    ("--emit-ir", "--print-stdlib"),
]:
    for modes in [(first, second), (second, first)]:
        preserved.write_bytes(marker)
        result = run(compiler, *modes, source, "-o", preserved, status=2)
        assert not result.stdout and b"conflicting compiler modes" in result.stderr, result
        assert all(mode.encode() in result.stderr for mode in modes), result.stderr
        assert preserved.read_bytes() == marker

hir = run(compiler, "--emit-hir", source).stdout
assert hir.startswith(b"(on1x.hir ") and b'"factorial"' in hir and b"(While" in hir
execute_c("direct-c", run(compiler, "--emit-c", source).stdout)

invalid_source = work / "ill-typed.on1x"
invalid_source.write_text("fn main() -> i64 { return true; }\n", encoding="utf-8")
diagnostic = run(compiler, "--check", invalid_source, status=1).stderr
assert b"ill-typed.on1x:1:" in diagnostic
for mode in ("--emit-c", *mir_modes):
    preserved.write_bytes(marker)
    result = run(compiler, mode, invalid_source, "-o", preserved, status=1)
    assert result.stderr == diagnostic and not result.stdout, result
    assert preserved.read_bytes() == marker

wire = work / "scalar.mir"
wire.write_bytes(run(compiler, "--emit-ir", source).stdout)
assert wire.read_bytes().startswith(b"(on1x.mir ")
assert str(source.resolve()).encode() in wire.read_bytes()
assert run(compiler, "--emit-ir", "--ir-input", wire).stdout == wire.read_bytes()
for input_args in [(source,), ("--ir-input", wire)]:
    assert run(compiler, "--run-ir", *input_args).stdout == expected
    assert run(compiler, "--run-vm", *input_args).stdout == expected
    image = work / "scalar.o1vm"
    run(compiler, "--emit-vm", *input_args, "-o", image)
    assert image.read_bytes().startswith(b"O1VM\x01\r\n")
    run(runner, "--verify", image)
    assert run(runner, image).stdout == expected
    cfg = run(compiler, "--ir-cfg", *input_args).stdout
    assert cfg.startswith(b"digraph on1x_mir") and b" -> " in cfg
    if dot := shutil.which("dot"):
        graph = work / "scalar.dot"
        graph.write_bytes(cfg)
        run(dot, "-Tdot", graph)
    execute_c("mir-c", run(compiler, "--emit-ir-c", *input_args).stdout)

# Exercise both the decoder's version gate and the semantic verifier reached
# after successful decoding. Neither failure may reach product publication.
tag, version, _, body = wire.read_bytes().split(maxsplit=3)
for bad_version, bad_id, message in [
    (b"999", b"1", b"unsupported format version"),
    (version, b"0", b"zero/duplicate identity"),
]:
    bad = work / "bad.mir"
    bad.write_bytes(b" ".join((tag, bad_version, bad_id, body)))
    for mode in mir_modes:
        preserved.write_bytes(marker)
        result = run(compiler, mode, "--ir-input", bad, "-o", preserved, status=1)
        assert message in result.stderr, result.stderr
        assert not result.stdout and preserved.read_bytes() == marker
