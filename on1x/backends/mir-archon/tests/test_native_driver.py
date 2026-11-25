#!/usr/bin/env python3
"""Real compiler/toolchain integration and atomic-output failure tests."""
import argparse
from pathlib import Path
import re
import signal
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--cc", type=Path, required=True)
    parser.add_argument("--sdk", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    helper = root / "tools/on1x-native-build.py"
    compiler = args.build_dir / "on1xc"
    tool = args.build_dir / "backends/mir-archon/on1x-archon-mir"
    base = [sys.executable, helper, "--build-dir", args.build_dir, "--cc", args.cc,
            "--target", "x86_64-unknown-linux-gnu"]

    def run(command, ok=True):
        result = subprocess.run([str(x) for x in command], capture_output=True, timeout=60)
        assert (result.returncode == 0) == ok, (command, result.returncode, result.stderr)
        return result

    with tempfile.TemporaryDirectory(prefix="native-driver-", dir=args.build_dir) as tmp:
        work = Path(tmp)
        source = work / "main.on1x"
        source.write_text("fn main() -> i64 { println(42); return 0; }\n")
        output = work / "program"
        for flags in ([], ["--optimize-ir"], ["--pic"]):
            run(base + [source, "-o", output] + flags)
            assert run([output]).stdout == b"42\n"
        generated = work / "reference.c"
        reference = work / "reference"
        run([compiler, "--emit-ir-c", source, "-o", generated])
        run([args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", generated,
             "-I", root / "runtime", args.build_dir / "libon1x_runtime.a", "-lm", "-o", reference])
        assert run([reference]).stdout == run([output]).stdout
        mir = work / "main.mir"
        run([compiler, "--emit-ir", source, "-o", mir])
        run(base + [mir, "--ir-input", "-o", output])
        assert run([output]).stdout == b"42\n"
        output_bytes = output.read_bytes()
        run(base + [mir, "--ir-input", "--enable-opt", "integer_add_zero_right", "-o", output], False)
        assert output.read_bytes() == output_bytes
        assembly = work / "main.s"
        wrapper = work / "wrapper.on1x"
        wrapper.write_text("fn plus(x:i64)->i64{return x+0;}fn main(){println(plus(42));}\n")
        build_asm = base + [wrapper, "--emit-only", "-o", assembly, "--profile", "debug"]
        run(build_asm)
        with_add = assembly.read_bytes()
        run(build_asm + ["--enable-opt", "integer_add_zero_right"])
        assert assembly.read_bytes() != with_add
        # This portable directive is interpreted only at the Archon product
        # boundary, not by the compiler's ordinary MIR pass pipeline.
        portable = work / "scoped.mir"
        portable.write_text("""(on1x.mir 4 1
          ((function 2 "keep" i64 (at "fixture" 0 1 1) ()
            ((block 4 5 (at "fixture" 0 1 1) ((value 6 i64 (at "fixture" 0 1 1)))
              ((instruction 7 neg.checked i64 (6) 0 0 5 8 (at "fixture" 0 1 1))
               (instruction 9 neg.checked i64 (7) 0 0 8 10 (at "fixture" 0 1 2)))
              (exit 11 return 9 10 () (at "fixture" 0 1 1))))))
          ((directive "mir-archon" "x86_64-unknown-linux-gnu" "rewrite" "meta-canonical" 2 true)))
        """, encoding="utf-8")
        build_scoped = base + [portable, "--ir-input", "--emit-only", "--no-entry", "-o", assembly]
        run(build_scoped)
        assert assembly.read_bytes().count(b"on1x_neg_i64") == 2
        run(build_scoped + ["--optimize-ir"])
        assert assembly.read_bytes().count(b"on1x_neg_i64") == 1
        run(build_scoped + ["--speculate"])
        assert assembly.read_bytes().count(b"on1x_neg_i64") == 1
        run(base + [source, "--emit-only", "--no-entry", "-o", assembly])
        assert b"\nmain:" not in assembly.read_bytes()
        marker = b"previous complete output\n"
        failures = [
            ["--target", "aarch64-unknown-linux-gnu"],
            ["--max-assembly-bytes", "1"],
            ["--max-output-bytes", "1"],
            ["--timeout", "0.000001"],
            ["--cflag=-ffast-math"],
            ["--cflag=-m32"],
            ["--on1xc-arg=--emit-c"],
            ["--no-entry"],
            ["--assembler", "/usr/bin/false"],
            ["--linker", "/usr/bin/false"],
        ]
        for flags in failures:
            output.write_bytes(marker)
            run(base + [source, "-o", output] + flags, False)
            assert output.read_bytes() == marker
            assert not list(work.glob(".on1x-native-*"))
        source.write_text("fn main() -> i64 { return true; }\n")
        run(base + [source, "-o", output], False)
        assert output.read_bytes() == marker
        finite = root / "examples/finite_iteration.on1x"
        unsupported = run(base + [finite, "-o", output], False)
        assert b"ON1X-MIR-UNSUPPORTED: scalar MIR cannot lower array" in unsupported.stderr
        assert output.read_bytes() == marker
        assembly.write_bytes(marker)
        run([tool, "--ir-input", mir, "--target", "aarch64-unknown-linux-gnu", "-o", assembly], False)
        assert assembly.read_bytes() == marker
        run([tool, "--ir-input", mir, "--max-assembly-bytes", "1", "-o", assembly], False)
        assert assembly.read_bytes() == marker
        bad_mir = work / "bad.mir"
        bad_mir.write_text("(not-valid-mir)")
        run([tool, "--ir-input", bad_mir, "-o", assembly], False)
        assert assembly.read_bytes() == marker
        # Explicit fault-injection tool blocks during target probing. Wait for
        # its marker before cancelling, rather than relying on compiler timing.
        import time
        blocked = work / "blocked-cc"
        started = work / "cc-started"
        blocked.write_text("#!" + sys.executable + "\nimport os, time\n"
                           "open(" + repr(str(started)) + ", 'w').write(str(os.getpid()))\n"
                           "time.sleep(30)\n")
        blocked.chmod(0o700)
        child = subprocess.Popen([str(x) for x in base + [source, "--cc", blocked, "-o", output]],
                                 stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        deadline = time.monotonic() + 5
        while not started.exists() and child.poll() is None and time.monotonic() < deadline:
            time.sleep(0.01)
        assert started.exists(), child.communicate(timeout=5)
        child.send_signal(signal.SIGTERM)
        _, error = child.communicate(timeout=5)
        assert child.returncode == 130 and b"cancelled" in error, error
        assert output.read_bytes() == marker
        assert not list(work.glob(".on1x-native-*"))
        if args.sdk:
            source.write_text("fn main() -> i64 { println(42); return 0; }\n")
            request = [compiler, "--sdk-emit", "mir-archon", "--sdk-target",
                       "x86_64-unknown-linux-gnu", source, "-o", assembly]
            run(request)
            run([args.cc, "-no-pie", assembly,
                 args.build_dir / "backends/mir-archon/libon1x_archon_scalar_abi.a",
                 args.build_dir / "libon1x_runtime.a", "-lm", "-Wl,-z,noexecstack", "-o", output])
            assert run([output]).stdout == b"42\n"
            run(request + ["--optimize-ir"])
            run(request + ["--native-pic"])
            run([args.cc, "-pie", assembly,
                 args.build_dir / "backends/mir-archon/libon1x_archon_scalar_abi.a",
                 args.build_dir / "libon1x_runtime.a", "-lm", "-Wl,-z,noexecstack", "-o", output])
            assert output.read_bytes()[16:18] == b"\x03\x00"
            assert run([output]).stdout == b"42\n"
            for flags in (["--native-no-entry"], ["--native-pic", "--native-no-entry"]):
                run(request + flags)
                assert not re.search(rb"(?m)^\s*main\s*:", assembly.read_bytes())
                run([args.cc, "-c", "-x", "assembler", assembly, "-o", work / "library.o"])
            assembly.write_bytes(marker)
            for flags in (["--sdk-target", "aarch64-unknown-linux-gnu"],
                          ["--plugin", "+untrusted"]):
                run(request + flags, False)
                assert assembly.read_bytes() == marker
            for flag in ("--native-pic", "--native-no-entry"):
                for mode in (["--emit-c"], ["--emit-ir"],
                             ["--sdk-emit", "mir-c11"], ["--sdk-emit", "mir-archon"]):
                    run([compiler, *mode, flag, source, "-o", assembly], False)
                    assert assembly.read_bytes() == marker
    print("native driver: positive, differential, target/flags/limits/cancellation/atomicity passed")


if __name__ == "__main__":
    main()
