"""Exercise real pre-backend directive dispatch with hand-authored portable MIR."""
from pathlib import Path
import subprocess
import sys
import tempfile


on1xc, work, *native = map(Path, sys.argv[1:])
# Keep this fixture independent of HIR producer changes.
functions = """
((function 2 "keep" i64 (at "fixture" 0 1 1) ()
  ((block 4 5 (at "fixture" 0 1 1) ((value 6 i64 (at "fixture" 0 1 1)))
    ((instruction 7 neg.checked i64 (6) 0 0 5 8 (at "fixture" 0 1 1))
     (instruction 9 neg.checked i64 (7) 0 0 8 10 (at "fixture" 0 1 2)))
    (exit 11 return 9 10 () (at "fixture" 0 1 1)))))
 (function 3 "main" i64 (at "fixture" 0 1 1) ()
  ((block 12 13 (at "fixture" 0 1 1) ()
    ((instruction 14 const i64 () 42 0 0 0 (at "fixture" 0 1 1))
     (instruction 15 call i64 (14) 0 2 13 16 (at "fixture" 0 1 1)))
    (exit 17 return 15 16 () (at "fixture" 0 1 1)))))
)
"""
with tempfile.TemporaryDirectory(prefix="mir-dispatch-", dir=work) as directory:
    root = Path(directory)

    def fixture(backend, target):
        path = root / f"{backend}.mir"
        path.write_text(f'(on1x.mir 4 1 {functions}'
                        f'((directive "{backend}" "{target}" "rewrite" "meta-canonical" 2 true)))\n',
                        encoding="utf-8")
        return path

    def run(*args, code=0):
        result = subprocess.run(args, capture_output=True, check=False)
        assert result.returncode == code, (args, result.returncode, result.stderr)
        return result.stdout

    def check_negation(plain, opted_in):
        assert plain.count(b"on1x_neg_i64") == 2 and opted_in.count(b"on1x_neg_i64") == 1

    if not native:
        triple = "x86_64-unknown-linux-gnu"
        path = fixture("mir-c11", triple)
        cmd = (on1xc, "--emit-ir-c", "--ir-input", path)
        plain = run(*cmd, "--mir-target", triple)
        check_negation(plain, run(*cmd, "--mir-target", triple, "--optimize-ir"))
        assert run(*cmd, "--optimize-ir") == plain
        assert run(*cmd, "--mir-target", "x86_64-linux-gnu", "--optimize-ir") == plain

        path = fixture("mir-vm", triple)
        bytecode = (on1xc, "--emit-vm", "--ir-input", path)
        plain = run(*bytecode)
        assert run(*bytecode, "--optimize-ir") == plain
        assert run(*bytecode, "--mir-target", triple) == plain
        assert run(*bytecode, "--mir-target", "x86_64-linux-gnu", "--optimize-ir") == plain
        assert run(*bytecode, "--mir-target", triple, "--optimize-ir") != plain
        run(on1xc, "--run-vm", "--ir-input", path, code=42)
        run(on1xc, "--run-vm", "--ir-input", path, "--mir-target", triple, "--optimize-ir", code=42)

        path = fixture("mir-cfg", triple)
        cmd = (on1xc, "--ir-cfg", "--ir-input", path)
        assert run(*cmd, "--optimize-ir") == run(*cmd)
        # DOT does not print instructions; the rewrite preserves CFG topology.
        assert run(*cmd, "--mir-target", triple, "--optimize-ir") == run(*cmd)
        path = fixture("mir-eval", triple)
        run(on1xc, "--run-ir", "--ir-input", path, "--mir-target", triple, "--optimize-ir", code=42)
    else:
        tool, *sdk = native
        triple = run(tool, "--print-target").decode("ascii").strip()
        path = fixture("mir-archon", triple)
        plain = run(tool, "--ir-input", path, "--target", triple)
        selected = run(tool, "--ir-input", path, "--target", triple, "--speculate")
        check_negation(plain, selected)
        run(tool, "--timer-source", path, "--speculate", code=1)
        if sdk:
            cmd = (on1xc, "--sdk-emit", "mir-archon", "--sdk-target", triple, "--ir-input", path)
            check_negation(run(*cmd), run(*cmd, "--optimize-ir"))
