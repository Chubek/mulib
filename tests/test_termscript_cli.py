#!/usr/bin/env python3
"""Exercise the Termscript runner, native database and generated C11 together."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / "library" / "termscript"
TERM_SOURCES = ROOT / ".sot" / "term"


def run(args, data=None, want=None, fail=False, env=None):
    p = subprocess.run(args, input=data, capture_output=True, timeout=45, env=env, cwd=ROOT)
    if (p.returncode == 0) == fail or (want is not None and p.stdout != want):
        raise RuntimeError(f"{args}: exit={p.returncode}, stdout={p.stdout!r}, stderr={p.stderr!r}")
    return p.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("runner", "termdb-test", "compiler-test"):
        parser.add_argument(f"--{name}", type=lambda p: str(Path(p).resolve()), required=True)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--sanitize", choices=("0", "1"), default="0")
    a = parser.parse_args()
    env = dict(os.environ, TERM="xterm-fixture", TERMSCRIPT_LIBRARY_PATH="")
    run([a.runner, str(LIB / "examples" / "counter.term")], want=b"10\n", env=env)
    run([a.runner, "--ansi", "--library", str(LIB), "-"], b"""
#Terminfo { "stdlib.terminfo" import } bind seal;
#ImTUI { "stdlib.imtui" import } bind seal;
#State { "stdlib.state" import } bind seal;
`kf11` #Terminfo exec;
(3, 1) "Hi" `label` #ImTUI exec;
0 1 [(0, 1, 9)] `next` #State exec typeout;
%if-term: xterm*
"match" typeout;
%else:
"miss" typeout;
%end:
""", want=b"\x1b[23~\x1b[2;4HHi9match", env=env)
    run([a.runner, "-"], b'"\x1b[2J" typeout', want="�[2J".encode(), env=env)
    run([a.runner, "-"], b"`echo test-only` exec", want=b"", fail=True, env=env)
    run([a.runner, "-"], b"[[CUP]] drop", want=b"", env=env)
    run([a.runner, "-"], b"[[CUP]] emit", want=b"", fail=True, env=env)
    run([a.runner, "-"], b'"testlib.module" import drop', fail=True, env=env)
    run([a.runner, "-"], b"""
@a { 0 4 iota } set;
5 @a push; #last { @a pop } set;
@a len typeout #last typeout;
%r <k: "ok"> set; %r #k get typeout;
""", want=b"55ok", env=env)
    run([a.runner, "-"], b"@empty [] set @empty pop", fail=True, env=env)
    tmp = "/tmp/opencode" if Path("/tmp/opencode").is_dir() else None
    with tempfile.TemporaryDirectory(prefix="mulib-termscript-", dir=tmp) as work:
        work = Path(work)
        external = work / "external" / "testlib"
        external.mkdir(parents=True)
        (external / "module.term").write_text("%module: testlib.module\n#value 17 set;\n", encoding="utf-8")
        path = f"{work / 'missing'}::{external.parent}"
        run([a.runner, "-"], b'"testlib.module" import .value at typeout', want=b"17",
            env=dict(env, TERMSCRIPT_LIBRARY_PATH=path))
        db = work / "terminal.sqlite3"
        corpus = TERM_SOURCES if all((TERM_SOURCES / x).is_file() for x in
            ("ecma-48-code-sequence.txt", "kitty-escape-seq.txt")) else ROOT / "tests/fixtures/termdb"
        run([sys.executable, str(ROOT / "tools" / "mk-termdb.py"), str(db), "--source-dir", str(corpus)])
        run([a.termdb_test, "--meta", str(db), "schema_version"], want=b"1")
        run([a.termdb_test, "--emit", str(db), "xterm", "kf11"], want=b"\x1b[23~")
        expected = b"\x1b[3;5H"
        run([a.termdb_test, "--emit", str(db), "-", "CUP", "2", "4"], want=expected)
        run([a.runner, "--termdb", str(db), "-"], b"#pos [[CUP]] set; (2, 4) #pos emit",
            want=expected, env=env)
        run([a.runner, "--termdb", str(db), "-"], b"[[CUP]] emit", want=b"", fail=True, env=env)
        run([a.runner, "--termdb", str(db), "-"], b"[[paste-on]] emit", want=b"", fail=True, env=env)
        run([a.runner, "--termdb", str(db), "--profile", "xterm", "-"], b"[[paste-on]] emit",
            want=b"\x1b[?2004h", env=env)
        generated = work / "generated.c"
        generated.write_bytes(run([a.compiler_test, "--emit"]))
        headers = ["-std=c11", "-pedantic-errors", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "include")]
        flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if a.sanitize == "1" else []
        executable = work / "compiled"
        run(shlex.split(a.cc) + headers + flags + ["-DMULIB_TERMSCRIPT_TEST_GENERATED",
            str(ROOT / "tests" / "test_termscript_compiler.c"), str(generated), "-o", str(executable)])
        run([str(executable)], want=run([a.compiler_test, "--interpret"]))
        c11 = run([a.runner, "--compile", "compiled_script", "-"], b"[[CUP]] drop 4 5 +", env=env)
        if b"compiled_script(" not in c11 or b"MULIB_TERMSCRIPT_ESCAPE" not in c11:
            raise RuntimeError("CLI did not lower the escape literal to C11")
    print("Termscript runner, generated SQLite database, and C11 equivalence passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
