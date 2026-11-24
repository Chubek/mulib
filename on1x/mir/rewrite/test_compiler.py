"""Exercise the actual LLSynth parser and database compiler."""
from pathlib import Path
import subprocess
import sys
import tempfile


compiler, grammar, source, parser, bootstrap = map(Path, sys.argv[1:])
core = source.read_text(encoding="utf-8")
with tempfile.TemporaryDirectory(prefix="mir-policy-", dir=parser.parent) as work:
    policy = Path(work) / "cases.mirp"
    header = Path(work) / "cases.hpp"

    def check(text, expected="", snapshot=False, use_bootstrap=False):
        policy.write_text(text, encoding="utf-8")
        mode = ["--bootstrap", bootstrap] if use_bootstrap else ["--parser", parser]
        run = subprocess.run([sys.executable, "-B", compiler, "--grammar", grammar,
                              "--input", policy, *mode, "--output", header],
                             capture_output=True, text=True, check=False)
        assert (run.returncode != 0 and expected in run.stderr) if expected else run.returncode == 0, run.stderr
        if snapshot:
            assert header.read_bytes() == bootstrap.read_bytes()
        return header.read_text(encoding="utf-8") if not expected else ""

    check(core, snapshot=True)
    check(core, snapshot=True, use_bootstrap=True)
    check(core + "\n# changed\n", "stale MIR policy", use_bootstrap=True)
    check(core.replace('backend "mir-archon"', 'backend "mir-test"'))
    check(core.replace('target "x86_64-unknown-linux-gnu"', 'target "wasm32-wasi"'))
    check(core.replace("pattern add_zero ", "pattern call ").replace("$x", "$return"))
    check(core.replace("mir-policy 1;", "mir-policy 2;"), "unsupported MIR policy version")
    check("(on1x.mir 1 1 ())", "expected a MIR policy document, not a wire envelope")
    check(core.replace('target * enables slots;', 'enables slots;'), "PARSE:")
    check(core.replace('target * enables slots;', 'target @ enables slots;'), "LEX:")
    check(core.replace('target * enables slots;', 'target "two words" enables slots;'),
          "expected an unescaped policy name")
    check(core.replace('enables slots;', 'enables arithmetic;'), "unsupported action/class")
    check(core.replace('requires identity;', 'requires assumption;', 1), "unknown pattern class, opcode, or proof")
    check(core.replace('replace $x requires identity;', 'replace $different requires identity;', 1),
          "not justified")
    check(core.replace('($x, 0)', '($x, 9223372036854775808)', 1), "not a canonical i64")
    check(core.replace('($x, 0)', '($x, 0, 0)', 1), "at most two operands")
    check(core.replace('replace 1 requires nonzero;', 'replace 1 requires identity;'), "not justified")
    check(core.replace('($x, 1) replace $x requires identity;', '($x, -1) replace $x requires identity;'),
          "not justified")
    check(core + next(line for line in core.splitlines() if line.startswith("pattern ")), "duplicate pattern")
    check(core + "rewrite \"fold-identity\" backend * target * enables arithmetic;", "duplicate activation")
    check(core + "\n#" + "a" * 16384, "exceeds 16 KiB")
    conditional = check(core + '\npattern zero_div backend "mir-test" target "wasm32-wasi" class arithmetic '
                        'mode speculative match "div.checked"(0, $x) replace 0 requires nonzero;\n')
    assert '{"mir-test", "wasm32-wasi"}, Group::Arithmetic, true, Op::Div' in conditional
