#!/usr/bin/env python3
"""File-unit, rewriting and MetaGC regressions, run against either dialect."""
import argparse
import os
from pathlib import Path
import subprocess
import struct
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--frontend")
    args = parser.parse_args()
    binary = str(Path(args.binary).resolve())
    env = dict(os.environ, METAML_OPTML=str(ROOT / "optml"))
    temporary = "/tmp/opencode" if Path("/tmp/opencode").is_dir() else None
    with tempfile.TemporaryDirectory(prefix="metaml-units-", dir=temporary) as tmp:
        root = Path(tmp)

        def write(name, source):
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(source)
            return path

        def run(source, *, name="test.mlls", expected=None, error=None, engine="tree", rules=None):
            path = write(name, source)
            result = subprocess.run([binary, "--no-stdlib", "--engine", engine, str(path)],
                                    cwd=ROOT, env=env if rules is None else dict(env, METAML_OPTML=str(root / rules)),
                                    capture_output=True, text=True, timeout=30)
            if error:
                assert result.returncode != 0 and error in result.stderr, (source, result)
            else:
                assert result.returncode == 0, (source, result.stdout, result.stderr)
                if expected is not None:
                    assert result.stdout == expected, (source, result.stdout, expected)

        write("Foo/Bar.mllm", "module Foo::Bar\nfn add a b := a + b\n")
        write("Foo/Buzz.mllm", "module Foo::Buzz\nfn answer := 42\n")
        write("Foo/Use.mllm", "module Foo::Use\nimport Foo::Bar\nfn answer := Foo.Bar.add 20 22\n")
        write("Foo/Again.mllm", "module Foo::Again\nimport Foo::Bar\nfn answer := Foo.Bar.add 40 2\n")
        write("Foo.mllm", "module Foo\nfn root := 0\n")
        for engine in ("tree", "xvm", "portable"):
            run('program Main\nimport Foo::{Bar, Buzz}\nfn () := IO.println (Core.show (Foo.Bar.add 20 22))\n',
                name="main.mllp", expected="42\n", engine=engine)
            run('script Main\nimport qualified Foo::*\nIO.println (Core.show Foo.Buzz.answer)\n',
                expected="42\n", engine=engine)
            run('script Main\nfn add := $1 + $2\nIO.println (Core.show (add 20 22))\n', expected="42\n", engine=engine)
            run('script Main\nfn outer x := fn y -> if true then x + y + 0 else 0\nIO.println (Core.show (outer 20 22))\n',
                expected="42\n", engine=engine)

        run('script Main\nimport Foo::{Use, Again}\nimport Foo\nIO.println (Core.show (Foo.Use.answer + Foo.Again.answer + Foo.root))\n',
            expected="84\n")

        write("helper.mlls", "script Helper\nfn answer := 42\n")
        write("helper.mlli", "this is not an interface and must be ignored by scripts\n")
        if args.frontend:
            for source, name, success in [
                ('program Check\nimport Foo::*\nfn () := IO.println "MUST NOT RUN"\n', "check.mllp", True),
                ('script Check\ninclude "helper.mlls"\nimport Foo::{Use, Again}\nIO.println "MUST NOT RUN"\n', "check.mlls", True),
                ('script Check\nimport "helper.mlls"\n', "check.mlls", False),
            ]:
                path = write(name, source)
                checked = subprocess.run([str(Path(args.frontend).resolve()), "--no-stdlib", "--check", str(path)],
                                         cwd=ROOT, env=env, capture_output=True, text=True, timeout=30)
                assert (checked.returncode == 0) == success, (source, checked)
                assert "MUST NOT RUN" not in checked.stdout, checked
        run('script Main\ninclude "helper.mlls"\nIO.println (Core.show answer)\n', expected="42\n")
        run('script Main\nimport "helper.mlls"\n', error="M0507")
        run('program Main\ninclude "helper.mlls"\nfn () := 0\n', name="main.mllp", error="M0507")
        run('script Main\ninclude Foo.Bar\n', error="M0507")
        write("cycle.mlls", 'script Cycle\ninclude "cycle.mlls"\n')
        run('script Main\ninclude "cycle.mlls"\n', error="M0508")
        run('program Main\nfn x := 1\n', name="main.mllp", error="M0507")
        run('script Main\nfn () := 0\n', error="M0507")
        run('fn foo := 1\n', error="M0507")
        run('program Foo::Bar\nfn () := 0\n', name="main.mllp", error="M0507")
        write("Wrong.mllm", "module Other\nfn x := 1\n")
        run('script Main\nimport Wrong\n', error="M0507")
        write("Foo/Bar.mlli", "interface Foo::Bar\nval add : Int -> Int -> Int\n")
        run('script Main\nimport Foo::Bar\nIO.println (Core.show (Foo.Bar.add 20 22))\n', expected="42\n")
        write("Foo/Bar.mlli", "interface Different\nval add : Int -> Int -> Int\n")
        run('script Main\nimport Foo::Bar\n', error="M0506")
        write("Foo/Bar.mlli", "interface Foo::Bar\nval add : String\n")
        run('script Main\nimport Foo::Bar\n', error="M0502")
        run('script Main\nfn r := Rule.read (SExpr.read "(rewrite zero (add ?x 0) ?x)")\n'
            'fn term := SExpr.read "(add (add a 0) 0)"\n'
            'IO.println (SExpr.show (Rule.saturate [r] term))\n'
            'IO.println (SExpr.show (Rule.rewrite_if (fn _ -> true) r term))\n'
            'IO.println (SExpr.show (Rule.apply r (SExpr.read "(add b 0)")))\n'
            'IO.println (SExpr.show (SExpr.decode (SExpr.encode (SExpr.read "(foo \\\"?x\\\")"))))\n'
            'IO.println (match r with | RewriteRule _ -> "rule")\n',
            expected='a\na\nb\n(foo "?x")\nrule\n')
        run('script Main\nSExpr.decode [77, 83, 88, 1, 2, 255, 255, 255, 255]\n', error="M0409")
        run('script Main\nRule.read (SExpr.read "(rewrite bad a ?missing)")\n', error="M0409")
        write("Cycle/A.mllm", "module Cycle::A\nimport Cycle::B\nfn x := 1\n")
        write("Cycle/B.mllm", "module Cycle::B\nimport Cycle::A\nfn x := 1\n")
        run('script Main\nimport Cycle::A\n', error="M0508")
        write("Dup.mllm", "module Dup\nfn x := 1\n")
        write("Dup/mod.mllm", "module Dup\nfn x := 2\n")
        run('script Main\nimport Dup\n', error="M0509")
        run('script Main\n1\n', rules="missing", error="M0701")
        write("bad-rules/core.sx", "(rewrite invalid (int ?n) (none))\n")
        run('script Main\n1\n', rules="bad-rules", error="M0701")

        def encode(node):
            if isinstance(node, str):
                text = node.encode()
                return b"\0" + struct.pack("<I", len(text)) + text
            return b"\2" + struct.pack("<I", len(node)) + b"".join(map(encode, node))

        binary_rules = root / "binary-rules"
        binary_rules.mkdir()
        (binary_rules / "core.msxb").write_bytes(b"MSX\1" + encode([
            "rules", ["rewrite", "fold", ["bin", "+", ["int", "1"], ["int", "2"]], ["int", "3"]]
        ]))
        run('script Main\nIO.println (Core.show (1 + 2))\n', rules="binary-rules", expected="3\n")
    print("language units, rewriting and MetaGC: OK")


if __name__ == "__main__":
    main()
