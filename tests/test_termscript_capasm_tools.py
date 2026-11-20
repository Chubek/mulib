#!/usr/bin/env python3
"""Exercise the Termscript CLI's trusted argv and artifact boundaries on POSIX."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


FAKE_TOOL = """#!/usr/bin/env python3
import json
import os
from pathlib import Path
import sys

kind, args = Path(sys.argv[0]).name, sys.argv[1:]
data = sys.stdin.buffer.read()
with open(os.environ["TEST_TOOL_LOG"], "a") as log:
    log.write(json.dumps([kind, args]) + "\\n")
if kind == "test m4":
    assert len(args) == 3 and args[0] == "-P" and args[2] == "-", args
    assert data.startswith(b"XM_BLOB("), data
    assert b"m4_syscmd" not in data, data
    action = Path(args[1]).read_text().strip()
    if action == "fail":
        sys.stdout.buffer.write(data)
        sys.stderr.write("rewrite fixture failed\\n")
        sys.exit(42)
    if action == "oversize":
        sys.stdout.buffer.write(b"X" * (17 * 1024 * 1024))
        sys.exit(0)
    if action == "replace":
        sys.stdout.buffer.write(data.replace(b"script_entry,", b"wrong_entry,", 1))
        sys.exit(0)
    assert action == "pass", action
    sys.stdout.buffer.write(data)
elif kind == "test cpp":
    assert b"script_entry(" in data, data
    sys.stdout.buffer.write(b"fixture preprocessed C")
elif kind == "test cc":
    assert "-S" in args and "-c" not in args, args
    if "cpp-output" in args:
        assert data == b"fixture preprocessed C", data
    else:
        assert b"script_entry(" in data, data
    sys.stdout.buffer.write(b"fixture assembler input")
elif kind == "test as":
    assert data == b"fixture assembler input", data
    Path(args[args.index("-o") + 1]).write_bytes(b"fixture object\\x00")
    sys.stdout.write("must not appear in artifact")
elif kind == "test ld":
    assert data == b"", data
    assert Path(args[0]).read_bytes() == b"fixture object\\x00"
    assert args[1:3] == ["./runtime helpers.c", "--fixture-link-opt"]
    Path(args[args.index("-o") + 1]).write_bytes(b"fixture executable\\x00")
    sys.stdout.write("must not appear in artifact")
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", required=True, type=lambda path: str(Path(path).resolve()))
    runner = parser.parse_args().runner
    if os.name != "posix":
        print("Skipping POSIX Termscript tool test")
        return
    temp_root = Path("/mnt/andromeda/tempdir/opencode")
    with tempfile.TemporaryDirectory(dir=temp_root if temp_root.is_dir() else None) as directory:
        work = Path(directory)
        log = work / "tools.log"
        env = dict(os.environ, TEST_TOOL_LOG=str(log), TMPDIR=str(work))
        env["PATH"] = f"{work}{os.pathsep}{env.get('PATH', os.defpath)}"

        def run(options, *, fail=False, diagnostic=None, source=b"1 2 +", entry="script_entry"):
            proc = subprocess.run(
                [runner, "--compile", entry, *options, "-"], input=source, capture_output=True,
                timeout=40, cwd=work, env=env,
            )
            assert (proc.returncode != 0) == fail, proc
            if fail:
                assert proc.stdout == b"", proc
            if diagnostic:
                assert diagnostic in proc.stderr, proc
            return proc.stdout

        def calls():
            if not log.exists():
                return []
            result = [json.loads(line) for line in log.read_text().splitlines()]
            log.unlink()
            return result

        for kind in ("m4", "cpp", "cc", "as", "ld"):
            program = work / f"test {kind}"
            program.write_text(FAKE_TOOL)
            program.chmod(0o755)
        (work / "decks").mkdir()
        (work / "other").mkdir()
        (work / "second.xmtrs").write_text("fail")
        (work / "decks" / "first.xmtrs").write_text("pass")
        (work / "other" / "first.xmtrs").write_text("fail")
        (work / "other" / "second.xmtrs").write_text("pass")
        (work / "other" / "oversize.xmtrs").write_text("oversize")
        (work / "other" / "replace.xmtrs").write_text("replace")
        env["XMACIR_REWRITE_PATH"] = ":decks::other:"
        m4 = ["--m4", "test m4"]
        c90 = run([])
        assert b"script_entry(" in c90 and not calls()
        assert run([*m4, "--rewrite", "first", "--rewrite", "second.xmtrs"]) == c90
        assert calls() == [
            ["test m4", ["-P", "./decks/first.xmtrs", "-"]],
            ["test m4", ["-P", "./other/second.xmtrs", "-"]],
        ]
        run([*m4, "--rewrite-path", str(work / "other"), "--rewrite", "first"],
            fail=True, diagnostic=b"exited with status 42")
        assert calls() == [["test m4", ["-P", str(work / "other" / "first.xmtrs"), "-"]]]
        run([*m4, "--rewrite", "oversize"], fail=True)
        assert len(calls()) == 1
        run([*m4, "--rewrite", "replace", "--rewrite", "second"],
            fail=True, diagnostic=b"changed the XMacIR ABI")
        assert len(calls()) == 1
        run([*m4, "--rewrite", "first"], source=b'"m4_syscmd(touch unexpected)" drop')
        assert len(calls()) == 1 and not (work / "unexpected").exists()
        assert b"name_m4_test(" in run([*m4, "--rewrite", "first"], entry="name_m4_test")
        assert len(calls()) == 1
        run([*m4, "--rewrite", "first"], entry="m4_syscmd", fail=True)
        assert not calls()
        for invalid in ("../first", "\\first", "decks/first", ".xmtrs"):
            run([*m4, "--rewrite", invalid], fail=True, diagnostic=b"invalid")
        run([*m4, "--rewrite", "first"], fail=True, source=b'"unterminated')
        assert not calls()
        proc = subprocess.run([runner, *m4, "--rewrite", "first", "-"], input=b"1 2 +",
                              capture_output=True, env=env, cwd=work, timeout=5)
        assert proc.returncode and not proc.stdout and b"require --compile" in proc.stderr
        assert not calls()

        plain = work / "plain text tool"
        plain.write_text("echo shell-fallback > unexpected\ncat\n")
        plain.chmod(0o755)
        run(["--m4", plain.name, "--rewrite", "first"], fail=True, diagnostic=b"cannot execute")
        assert not (work / "unexpected").exists()

        cc = ["--cc", "test cc", "--cc-arg", "-DOPT=a b;literal"]
        asm = ["--as", "test as"]
        cpp = ["--cpp", "test cpp", "--cpp-arg", "-DCPP=a b"]
        ld = ["--ld", "test ld", "--link-input", "runtime helpers.c",
              "--ld-arg", "--fixture-link-opt"]
        assert run(["--emit", "preprocessed", *cpp]) == b"fixture preprocessed C"
        assert [kind for kind, _ in calls()] == ["test cpp"]
        assert run(["--emit", "asm", *cc]) == b"fixture assembler input"
        assert calls() == [["test cc", ["-DOPT=a b;literal", "-S", "-std=c90", "-x", "c", "-", "-o", "-"]]]
        assert run(["--emit", "obj", *cc, *asm]) == b"fixture object\x00"
        assert [kind for kind, _ in calls()] == ["test cc", "test as"]
        run(["--emit", "exe", *cc, *asm], fail=True, diagnostic=b"explicit --link-input")
        assert not calls()
        assert run(["--emit", "exe", *cpp, *cc, *asm, *ld]) == b"fixture executable\x00"
        assert [kind for kind, _ in calls()] == ["test cpp", "test cc", "test as", "test ld"]
        assert not list(work.glob("mulib-capasm-*"))
    print("Termscript argv, explicit stage selection and artifact isolation passed.")


if __name__ == "__main__":
    main()
