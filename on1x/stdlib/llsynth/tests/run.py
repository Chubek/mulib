"""Exercise native std.llsynth against pinned LibStone parser artifacts."""

import argparse
import os
from pathlib import Path
import runpy
import struct
import subprocess
import sys
import tempfile


HERE = Path(__file__).resolve().parent
STD = HERE.parents[1]
ON1X = STD.parent
PACKER = HERE.parent / "pack_tables.py"
DRIVER = ON1X / "third_party/libstone/cornerstone/LLSynth/scripts/llsynth.pl"


def run(*args, **kwargs):
    return subprocess.run(args, capture_output=True, timeout=90, **kwargs)


def checked(*args, **kwargs):
    result = run(*args, **kwargs)
    assert result.returncode == 0, (args, result.stdout, result.stderr)
    return result.stdout


def literal(data):
    return 'b!"' + "".join(f"\\x{x:02x}" for x in data) + '"'


def words(*values):
    return literal(struct.pack(f"<{len(values)}I", *values))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("on1xc", type=Path)
    parser.add_argument("cc", nargs="?", default="cc")
    parser.add_argument("--perl", default="perl")
    parser.add_argument("--llsynth", type=Path, default=DRIVER)
    parser.add_argument("--cflag", action="append", default=[])
    args = parser.parse_args()
    assert checked(args.perl, args.llsynth, "--version") == b"llsynth.pl 0.7.0\n"

    with tempfile.TemporaryDirectory(prefix="on1x-llsynth-") as tmp:
        work = Path(tmp)
        compiler = args.on1xc.resolve()
        runtime = ON1X / "runtime"

        def generate(name, grammar, algorithm):
            upstream = work / (name + ".py")
            checked(args.perl, args.llsynth, "--mode=analyze", f"--parser={algorithm}",
                    "-T", "python3", "--driver-mode=library", "-o", upstream, grammar)
            image = checked(sys.executable, PACKER, upstream)
            (work / (name + ".on1x")).write_bytes(image)
            return upstream, image

        lr, image = generate("lr", HERE / "syntax.llsynth", "lr1")
        generate("lalr", HERE / "syntax.llsynth", "lalr1")
        mir, _ = generate("mir", ON1X / "mir/MIR.llsynth", "lalr1")
        meta, _ = generate("meta", ON1X / "hir/On1xMETA/On1xMETA.llsynth", "lalr1")

        # Packing only reads the SPEC envelope; it does not evaluate Python.
        envelope = next(line for line in lr.read_bytes().splitlines() if line.startswith(b"SPEC = "))
        poisoned = work / "not_executable.py"
        poisoned.write_bytes(b"raise RuntimeError('must not execute')\n" + envelope + b"\n")
        assert checked(sys.executable, PACKER, poisoned) == image
        for data in (b"", envelope + b"\n" + envelope + b"\n",
                     b'SPEC = json.loads(base64.b64decode("e30="))\n'):
            poisoned.write_bytes(data)
            rejected = run(sys.executable, PACKER, poisoned)
            assert rejected.returncode == 1 and not rejected.stdout, rejected
            assert rejected.stderr.startswith(b"pack_tables:"), rejected

        fixtures = [
            (b"", 0, 0), (b"if x;   \r\n", 0, 10), (b"ifx;", 0, 4),
            (b"if;", 2, 2), (b"a", 2, 1), (b"A;", 1, 0), (b"x; y @", 1, 5),
            (b"@@a\n;", 0, 5), (b"x;\n@@abc\n;", 0, 10), (b" @@a\n;", 1, 1),
            (b"@@a;", 1, 0), (b"\x00;", 0, 2), (b"\x00", 2, 1),
            ("é;".encode(), 0, 3), ("é é;".encode(), 2, 3), (b"\xc3;", 1, 0),
            (b"[1<2];", 0, 6), (b"[1<2<3];", 2, 4),
        ]

        def compare(upstream, cases):
            # Fixtures are authored locally or produced by our pinned compiler.
            spec = runpy.run_path(str(upstream))
            for source, status, _ in cases:
                try:
                    spec["parse"](source.decode("utf-8"))
                except UnicodeDecodeError:
                    assert status == 1, source
                except spec["LanguageError"] as exc:
                    assert exc.code == {1: "LEX", 2: "PARSE"}.get(status), (source, exc)
                else:
                    assert status == 0, source

        compare(lr, fixtures)
        mir_src = work / "ir_input.on1x"
        mir_src.write_text("fn main(){let n=42; println(n);}\n", encoding="utf-8")
        wire = checked(compiler, "--emit-ir", mir_src)
        assert wire.startswith(b"(on1x.mir ")
        truncated = wire.rstrip()[:-1]
        mir_cases = [(wire, 0, len(wire)), (truncated, 2, len(truncated))]
        meta_src = b"meta 1; budget steps 50 nodes 20 depth 2;\n"
        meta_bad = meta_src.rstrip()[:-1]
        meta_cases = [(meta_src, 0, len(meta_src)), (meta_bad, 2, len(meta_bad))]
        compare(mir, mir_cases)
        compare(meta, meta_cases)

        def statements(table, cases):
            return "\n".join(
                f"  expect({table}, {literal(source)}, {status}, {offset});"
                for source, status, offset in cases)

        def build(name, source, *flags):
            on1x = work / (name + ".on1x")
            c11 = work / (name + ".c")
            executable = work / name
            on1x.write_text(source, encoding="utf-8")
            checked(compiler, "--emit-c", "--stdlib", STD, on1x, "-o", c11)
            checked(args.cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                    "-I", runtime, *args.cflag, *flags, c11, runtime / "on1x_runtime.c",
                    "-o", executable)
            return executable

        # One lex row (empty), one LR row (EOF reduces an empty rule, goto itself).
        cycle = words(22236236, 1, 1, 1, 2, 3, 0, 11, 12, 12, 16, 0, 1, 1, 3, 3)
        bad_row = words(22236236, 2, 1, 1, 2, 3, 0, 12, 0xffffffff, 14, 14, 14, 0, 0)
        bad_header = words(22236236, 0xffffffff, 0xffffffff, 0xffffffff, 2)
        large_number = b"9223372036854775808;"
        program = f"""
import std.llsynth as L;
from .lr import basis_llsynth_tables as lr_tables;
from .lalr import basis_llsynth_tables as lalr_tables;
from .mir import basis_llsynth_tables as mir_tables;
from .meta import basis_llsynth_tables as meta_tables;

fn expect(table: buffer, source: buffer, status: i64, offset: i64)
{{
  let before = data_live_nodes();
  let (s, at) = L.recognize(table, source);
  assert(s == status && at == offset);
  assert(data_live_nodes() == before);
}}

fn main()
{{
  {{
    let lr = lr_tables();
    let lalr = lalr_tables();
    let mir = mir_tables();
    let meta = meta_tables();
{statements("lr", fixtures)}
{statements("lalr", fixtures)}
{statements("mir", mir_cases)}
{statements("meta", meta_cases)}
    expect(lr, {literal(large_number)}, 0, {len(large_number)});
    expect(b!"", b!"", 3, 0);
    expect({bad_header}, b!"", 3, 0);
    expect({bad_row}, b!"x", 3, 0);
    expect({cycle}, b!"", 4, 0);
    let source = buffer_new();
    for _ in 0 .. 1048577 {{ buffer_push(source, 120u8); }}
    expect(lr, source, 4, 0);
    expect(source, b!"", 4, 0);
    let (status, at) = L.basis_llsynth_recognize(lr, b!"");
    assert(status == 0 && at == 0);
  }}
  assert(data_live_nodes() == 0);
}}
"""
        assert checked(build("test", program), env={**os.environ, "PATH": ""}) == b""

        # Empty input allocates no backing bytes; the tiny grammar table is
        # >64 bytes, so this targets the recognizer's initial scratch reserve.
        oom = build("oom", """
import std.llsynth as L;
from .lr import basis_llsynth_tables;
fn main() { L.recognize(basis_llsynth_tables(), b!""); }
""", "-DON1X_DATA_MALLOC(n)=((n)==64?(void*)0:malloc(n))")
        result = run(oom, env={**os.environ, "LSAN_OPTIONS": "detect_leaks=0"})
        assert result.returncode == 73 and b"buffer allocation failure" in result.stderr, result

    print("LLSynth LR/DFA recognition, malformed tables, lifetime and allocation failure: PASS")


if __name__ == "__main__":
    main()
