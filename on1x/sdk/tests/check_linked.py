"""Check a compiler explicitly configured with the SDK C fixture."""

import pathlib
import re
import subprocess
import sys
import tempfile


def run(*args, status=0):
    result = subprocess.run(args, capture_output=True, timeout=60)
    assert result.returncode == status, (args, result)
    return result


def main():
    compiler, cc, *cflags = sys.argv[1:]
    root = pathlib.Path(__file__).resolve().parents[2]
    source = root / "examples" / "hello.on1x"
    with tempfile.TemporaryDirectory(prefix="on1x-sdk-") as directory:
        work = pathlib.Path(directory)
        diagnostic = run(compiler, "--sdk-analyze", "mir-check", source)
        assert not diagnostic.stdout
        assert b"SDK-FIXTURE-MIR" in diagnostic.stderr
        artifact = work / "copy.mir"
        assert not run(compiler, "--sdk-emit", "mir-copy", source, "-o", artifact).stdout
        assert artifact.read_bytes() == run(compiler, "--emit-ir", source).stdout
        assert artifact.read_bytes() == run(
            compiler, "--ir-input", "--sdk-emit", "mir-copy", artifact).stdout

        # A generated application still links only its normal C11 runtime.
        for mode, flags in (("direct", ["--emit-c", source]),
                            ("round-trip", ["--ir-input", "--emit-ir-c", artifact])):
            src, exe = work / f"{mode}.c", work / mode
            run(compiler, *flags, "-o", src)
            runtime = root / "runtime"
            run(cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                "-I", runtime, src, runtime / "on1x_runtime.c", *cflags, "-o", exe)
            assert run(exe).stdout == b"21\n"

        clean = artifact.read_bytes()
        for hook, status in (("missing", 1), ("mir-fail", 1), ("mir-limit", 75)):
            failure = run(compiler, "--sdk-emit", hook, source, "-o", artifact, status=status)
            assert not failure.stdout and artifact.read_bytes() == clean
        failure = run(compiler, "--sdk-analyze", "mir-reject", source, status=1)
        assert b"SDK-FIXTURE-REJECT" in failure.stderr and not failure.stdout
        invalid_source = work / "invalid.on1x"
        invalid_source.write_text("fn main() -> i64 {return false;}\n", encoding="utf-8")
        invalid_mir = work / "invalid.mir"
        invalid_mir.write_bytes(clean.replace(b"(on1x.mir ", b"(broken ", 1))
        unverified_mir = work / "unverified.mir"
        invalid_id, changes = re.subn(rb"^(\(on1x\.mir \d+ )\d+", rb"\g<1>0", clean)
        assert changes == 1
        unverified_mir.write_bytes(invalid_id)
        for path, options in ((invalid_source, []), (invalid_mir, ["--ir-input"]),
                              (unverified_mir, ["--ir-input"])):
            failure = run(compiler, "--sdk-analyze", "mir-check", path, *options, status=1)
            assert b"SDK-FIXTURE" not in failure.stderr and not failure.stdout
        failure = run(compiler, "--sdk-emit", "mir-copy", source, "--check", status=2)
        assert not failure.stdout

        # Publishing refuses a symlink and never modifies its referent.
        alias = work / "alias.mir"
        alias.symlink_to(artifact)
        failure = run(compiler, "--sdk-emit", "mir-copy", source, "-o", alias, status=2)
        assert not failure.stdout and alias.is_symlink() and artifact.read_bytes() == clean


if __name__ == "__main__":
    main()
