#!/usr/bin/env python3
"""PrettyPrint CLI and grammar compiler tests."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def run(*args, input=None, ok=True):
    proc = subprocess.run(args, input=input, capture_output=True, timeout=120, cwd=ROOT)
    if (proc.returncode == 0) != ok:
        raise AssertionError((args, proc.returncode, proc.stdout, proc.stderr))
    return proc.stdout if ok else proc.stderr


def read(path):
    return json.loads(path.read_text())


def cli(binary, sample):
    def render(*args, input=None, ok=True):
        return run(binary, *args, input=input, ok=ok)

    # The CLI does not append a newline to plain text.
    assert render(sample) == sample.with_suffix(".txt").read_bytes().rstrip(b"\n")
    assert b"<mn>42</mn>" in render("--target", "html", sample)
    assert render("--target", "latex", sample) == sample.with_suffix(".tex").read_bytes()
    assert render("--target", "troff", sample) == sample.with_suffix(".roff").read_bytes()
    group = b'(group (cat (text "abc") (cat (soft) (text "def"))))'
    assert render("--width", "10", "--ribbon", "6", input=group) == b"abc\ndef"
    assert render("--width", "10", "--ribbon", "0", input=group) == b"abc def"
    unicode_group = '(group (cat (text "界") (cat (soft) (annot "identifier" (text "é")))))'.encode()
    utf8 = "界 é".encode()
    assert render("--width", "4", input=unicode_group) == utf8
    assert render("--width", "3", input=unicode_group) == "界\né".encode()
    assert '界 <span class="tok-identifier">é</span>'.encode() in render(
        "--width", "4", "--target", "html", input=unicode_group)
    for invalid in ("-1", "+1", " 3", "4097", "999999999999999999999999"):
        render("--ribbon", invalid, input=group, ok=False)
    text = b'(text "x")'
    assert render(input=text.ljust(2 * 1024 * 1024)) == b"x"
    render(input=text.ljust(2 * 1024 * 1024 + 1), ok=False)
    render(input=text + b"\0", ok=False)
    render(input=text + b" (soft)", ok=False)
    render(input=b'(cat (text "trailing\\', ok=False)
    render(input=b'(source 0 4 2 (text "x"))', ok=False)
    assert render("--paragraph", "knuth-plass", "--width", "6",
                  input='(fill "aa ab\u00adcdef")'.encode()) == b"aa ab-\ncdef"
    render("--paragraph", "knuth-plass", "--width", "1", input=b'(fill "wide")', ok=False)
    with tempfile.TemporaryDirectory() as work:
        path = Path(work) / "map.json"
        assert render("--map", path, "--target", "html",
                      input=b'(source 9 13 15 (text "<&"))').count(b"&lt;&amp;") == 1
        assert read(path) == {"coordinate_space": "plain-utf8", "mappings": [[0, 2, 9, 13, 15]]}
    print("PrettyPrint CLI targets and input limits passed")


def ppg(binary):
    compiler = ROOT / "examples/ppgc.py"
    spec = ROOT / "examples/arithmetic.ppg"
    ast = ROOT / "examples/arithmetic.ast.json"
    source = run(sys.executable, compiler, spec)
    assert source == run(sys.executable, compiler, spec)
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        program = work / "format.py"
        program.write_bytes(source)
        ppdoc = run(sys.executable, program, ast)
        assert ppdoc == run(sys.executable, program, input=ast.read_bytes())
        assert run(binary, input=ppdoc) == b"(x)/(sqrt(2))"
        assert b"<msqrt>" in run(binary, "--target", "html", input=ppdoc)
        path = work / "origin.json"
        run(binary, "--map", path, input=ppdoc)
        assert read(path)["mappings"] == [[1, 1, 0, 0, 1], [10, 1, 0, 7, 8]]
        run(sys.executable, program, input=b'["unknown"]', ok=False)
        run(sys.executable, program, input=b'["atom",[0,0,1],123]', ok=False)
        run(sys.executable, compiler, "-", input=b'{"version":1,"version":1,"rules":[]}', ok=False)
        invalid = {"version": 1, "rules": [{"pattern": ["a"], "emit": {"render": "missing"}}]}
        run(sys.executable, compiler, "-", input=json.dumps(invalid).encode(), ok=False)
        matrix = b'(matrix (row (math-atom "a") (root (math-atom "b"))) (row (math-atom "c") (math-atom "d")))'
        if eqn := shutil.which("eqn"):
            markup = run(binary, "--target", "troff", input=matrix)
            proc = subprocess.run([eqn], input=markup, capture_output=True, timeout=30, check=True)
            assert b"syntax error" not in proc.stderr and proc.stdout
        if latex := shutil.which("pdflatex"):
            tex = work / "test.tex"
            tex.write_bytes(run(binary, "--target", "latex", input=matrix))
            run(latex, "-halt-on-error", "-interaction=nonstopmode", "-output-directory", work, tex)
            assert (work / "test.pdf").stat().st_size > 0
    print("PrettyPrint .ppg compiler and semantic math backends passed")


def main():
    mode, *args = sys.argv[1:] or [None]
    if mode == "cli" and len(args) == 2:
        cli(*(Path(arg).resolve() for arg in args))
    elif mode == "ppg" and len(args) == 1:
        ppg(Path(args[0]).resolve())
    else:
        raise SystemExit(f"usage: {sys.argv[0]} cli BINARY SAMPLE | ppg BINARY")


if __name__ == "__main__":
    main()
