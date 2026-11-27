"""Exercise both source-derived MIR analysis requests through real products."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile


on1xc, cc, runtime, work = map(Path, sys.argv[1:5])
flags = sys.argv[5:]
triple = "x86_64-unknown-linux-gnu"


def run(*args):
    result = subprocess.run(args, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, (args, result.returncode, result.stderr)
    return result.stdout


with tempfile.TemporaryDirectory(prefix="analysis-directives-", dir=work) as directory:
    root = Path(directory)
    source = root / "requests.on1x"
    source.write_text("""
fn typed_scratch(x:u8)->u8 { var y=x; return y; }
fn canonical(x:i64,b:bool)->(bool,i64) { return (b==!b,-(-x)); }
fn main() {
    println(typed_scratch(7u8));
    let (b,n)=canonical(42,true); println(b); println(n);
}
""", encoding="utf-8")
    wire = run(on1xc, "--emit-ir", source)
    load = re.search(r"\(instruction (\d+) load u8 ", wire)
    assert load is not None, wire
    load_value = re.compile(rf"\bv{load[1]}\b")
    mir = root / "requests.mir"
    mir.write_text(wire, encoding="utf-8")

    cmd = (on1xc, "--emit-ir-c", "--ir-input", mir)
    unselected = run(*cmd)
    safe = run(*cmd, "--mir-target", triple)
    opted_in = run(*cmd, "--mir-target", triple, "--optimize-ir")
    assert run(*cmd, "--mir-target", "x86_64-linux-gnu") == unselected
    assert run(on1xc, "--emit-ir-c", "--mir-target", triple, source) == safe
    assert load_value.search(unselected) and not load_value.search(safe)
    for generated, negations in [(unselected, 2), (safe, 2), (opted_in, 1)]:
        assert generated.count("on1x_neg_i64") == negations
        c = root / "requests.c"
        exe = root / "requests"
        c.write_text(generated, encoding="utf-8")
        run(cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
            "-I", runtime, c, runtime / "on1x_runtime.c", *flags, "-o", exe)
        assert run(exe) == "7\nfalse\n42\n"

    # VM receives META's wildcard-target request, not MEMGraph's C11 request.
    for options in [(), ("--optimize-ir",)]:
        assert run(on1xc, "--run-vm", source, *options) == "7\nfalse\n42\n"
        assert run(on1xc, "--run-vm", "--ir-input", mir, *options) == "7\nfalse\n42\n"
