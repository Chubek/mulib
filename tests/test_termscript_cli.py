#!/usr/bin/env python3
"""Exercise the Termscript runner, native database and CAPASM C90 output."""
import argparse
import os
from pathlib import Path
import shlex
import shutil
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
    env = dict(os.environ, TERM="xterm-fixture", TERMSCRIPT_LIBRARY_PATH="",
               XMACIR_REWRITE_PATH="")
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
    run([a.runner, "-"], b'[[ctrl, shift, f1]] { "chord" typeout } bind [[shift, ctrl, f1]] press',
        want=b"chord", env=env)
    run([a.runner, "-"], b'"testlib.module" import drop', fail=True, env=env)
    run([a.runner, "-"], b"""
@a { 0 4 iota } set;
5 @a push; #last { @a pop } set;
@a len typeout #last typeout;
%r <k: "ok"> set; %r #k get typeout;
""", want=b"55ok", env=env)
    run([a.runner, "-"], b"@empty [] set @empty pop", fail=True, env=env)
    with tempfile.TemporaryDirectory(prefix="mulib-termscript-") as work:
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
        executable, obj = work / "compiled", work / "generated.o"
        cc = shlex.split(a.cc)
        run(cc + ["-std=c90", "-pedantic-errors", "-Wall", "-Wextra", "-Werror"]
            + flags + ["-c", str(generated), "-o", str(obj)])
        run(cc + headers + flags + ["-DMULIB_TERMSCRIPT_TEST_GENERATED",
            str(ROOT / "tests" / "test_termscript_compiler.c"), str(obj), "-o", str(executable)])
        run([str(executable)], want=run([a.compiler_test, "--interpret"]))
        ansi = run([a.runner, "--compile", "compiled_script", "-"], b"[[CUP]] drop 4 5 +", env=env)
        generated.write_bytes(ansi)
        run(cc + ["-std=c90", "-pedantic-errors", "-Wall", "-Wextra", "-Werror",
                  "-c", str(generated), "-o", str(obj)])
        if b"compiled_script(" not in ansi or b"CAPASM" not in ansi:
            raise RuntimeError("CLI did not emit a CAPASM C90 translation unit")
        with_headers = run([a.runner, "--compile", "compiled_script", "--kernel", "money",
                            "--kernel", "termscript-runtime", "--kernel", "money", "-"],
                           b"42", env=env)
        if with_headers.count(b'#include "mulib-money.h"') != 1 or b"MULIB_IMPL_" in with_headers:
            raise RuntimeError("selected CAPASM headers must be declaration-only and deduplicated")
        generated.write_bytes(with_headers)
        run(cc + headers + ["-c", str(generated), "-o", str(obj)])
        for bad_kernel in ("capasm", "xmacir", "../money"):
            run([a.runner, "--compile", "compiled_script", "--kernel", bad_kernel, "-"],
                b"42", env=env, want=b"", fail=True)
        if shutil.which("m4"):
            plain = [a.runner, "--compile", "compiled_script"]
            run(plain + ["--rewrite", "repack", "--rewrite", "peephole", "-"],
                b"[[CUP]] drop 4 5 +", want=ansi, env=env)
            rewrite = work / "rewrite"
            rewrite.mkdir()
            # Without definitions m4 passes canonical XMacIR through unchanged.
            (rewrite / "fixture.xmtrs").write_text("", encoding="ascii")
            rewrite_env = dict(env, XMACIR_REWRITE_PATH=f"{work / 'missing'}::{rewrite}")
            run([a.runner, "--compile", "compiled_script", "--rewrite", "fixture", "-"],
                b"[[CUP]] drop 4 5 +", want=ansi, env=rewrite_env)
            run(plain + ["--m4", shutil.which("m4"), "--rewrite-path", str(rewrite),
                         "--rewrite", "fixture.xmtrs", "-"],
                b"[[CUP]] drop 4 5 +", want=ansi, env=env)
            run([a.runner, "--compile", "compiled_script", "--rewrite", "missing", "-"],
                b"1", want=b"", fail=True, env=rewrite_env)
            run([a.runner, "--compile", "compiled_script", "--rewrite", "../fixture", "-"],
                b"1", want=b"", fail=True, env=rewrite_env)
            (rewrite / "bad.xmtrs").write_text("not valid XMacIR\n", encoding="ascii")
            run([a.runner, "--compile", "compiled_script", "--rewrite", "bad", "-"],
                b"1", want=b"", fail=True, env=rewrite_env)
        run([a.runner, "--rewrite", "fixture", "-"], b"1", want=b"", fail=True, env=env)
        if sys.platform.startswith("linux") and shutil.which("as"):
            driver = cc[0]
            cli = [a.runner, "--compile", "compiled_script", "--cc", driver]
            # Clang's ELF .addrsig directives target its integrated assembler.
            if b"clang" in run(cc + ["--version"]).lower():
                cli += ["--cc-arg", "-fno-addrsig"]
            run([a.runner, "--compile", "compiled_script", "--emit", "preprocessed",
                 "--cpp", driver, "-"], b"42", env=env)
            preprocessed = run([a.runner, "--compile", "compiled_script", "--kernel", "money",
                                "--emit", "preprocessed", "--cpp", driver,
                                "--cpp-arg", f"-I{ROOT / 'include'}", "-"], b"42", env=env)
            if b"mulib_money_make" not in preprocessed:
                raise RuntimeError("selected CAPASM headers were not passed to preprocessing")
            assembly = run(cli + ["--emit", "asm", "--cpp", driver, "-"], b"42", env=env)
            if b"compiled_script" not in assembly:
                raise RuntimeError("configured compiler did not emit an assembly entry")
            native = run(cli + ["--emit", "obj", "--as", "as", "-"], b"42", env=env)
            if not native.startswith(b"\x7fELF"):
                raise RuntimeError("configured assembler did not emit an ELF object")
            harness = work / "staged.c"
            harness.write_text(
                "#define MULIB_IMPL_TERMSCRIPT_RUNTIME\n"
                "#define MULIB_IMPL_TERMSCRIPT_AST\n"
                "#define MULIB_IMPL_PARSEC\n"
                "#define MULIB_IMPL_UTF8\n"
                '#include "mulib-termscript-runtime.h"\n'
                "extern int compiled_script(mulib_termscript_vm_t *, mulib_termscript_error_t *);\n"
                "int main(void) {\n"
                "  mulib_termscript_vm_t vm;\n"
                "  mulib_termscript_vm_init(&vm, NULL);\n"
                "  int ok = compiled_script(&vm, NULL);\n"
                "  const mulib_termscript_value_t *v = mulib_termscript_peek(&vm, 0);\n"
                "  ok = ok && v && v->kind == MULIB_TERMSCRIPT_VALUE_INTEGER && v->integer == 42;\n"
                "  mulib_termscript_vm_clear(&vm);\n"
                "  return !ok;\n"
                "}\n", encoding="ascii",
            )
            run(cc + headers + ["-c", str(harness), "-o", str(obj)])
            executable.write_bytes(run(cli + ["--emit", "exe", "--as", "as", "--ld", driver,
                                              "--link-input", str(obj), "-"], b"42", env=env))
            executable.chmod(0o700)
            run([str(executable)])
    print("Termscript runner, SQLite database, and strict C90 equivalence passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
