"""Exercise the actual LLSynth parser, policy compiler, and bootstrap."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile


compiler, grammar, source, parser, bootstrap, *banks = map(Path, sys.argv[1:])
core = source.read_text(encoding="utf-8")
bodies = {bank.stem: bank.read_text(encoding="utf-8") for bank in banks}
assert len(banks) == len(bodies) == 10
with tempfile.TemporaryDirectory(prefix="mir-policy-", dir=parser.parent) as work:
    scratch = Path(work)
    policy, header = scratch / "core.mirp", scratch / "cases.hpp"
    paths = {name: scratch / f"{name}.mirp" for name in bodies}

    def check(expected="", *, manifest=core, edit=None, inputs=paths.values(), use_bootstrap=False):
        policy.write_text(manifest, encoding="utf-8")
        for name, path in paths.items():
            path.write_text(edit[1] if edit and name == edit[0] else bodies[name], encoding="utf-8")
        mode = ["--bootstrap", bootstrap] if use_bootstrap else ["--parser", parser]
        bank_args = [arg for path in inputs for arg in ("--bank", path)]
        run = subprocess.run([sys.executable, "-B", compiler, "--grammar", grammar,
                              "--input", policy, *bank_args, *mode, "--output", header],
                              capture_output=True, text=True, check=False)
        if expected:
            assert run.returncode != 0 and expected in run.stderr, run.stderr
        else:
            assert run.returncode == 0, run.stderr
            return header.read_text(encoding="utf-8")

    def change(bank, old, new, expected=""):
        assert old in bodies[bank], old
        return check(expected, edit=(bank, bodies[bank].replace(old, new, 1)))

    snapshot = bootstrap.read_text(encoding="utf-8")
    assert check() == check(inputs=list(reversed(paths.values()))) == check(use_bootstrap=True) == snapshot
    assert header.read_bytes() == bootstrap.read_bytes()
    emitted = re.findall(r"^    // ([a-z-]+): \w+$", snapshot, re.MULTILINE)
    assert len(emitted) == 29 and set(emitted) == bodies.keys()

    add = "additive-identities"
    units = "division-units"
    guarded = "guarded-division"
    negation = "boolean-negation"
    predicate = "boolean-comparisons"

    check("stale MIR policy", manifest=core + "\n# changed\n", use_bootstrap=True)
    check("stale MIR policy", edit=(add, bodies[add] + "\n# changed bank\n"), use_bootstrap=True)
    check("missing MIR bank", inputs=[path for name, path in paths.items() if name != add])
    check("duplicate MIR bank", inputs=[*paths.values(), paths[add]])
    check("unknown MIR bank", inputs=[*paths.values(), scratch / "unknown.mirp"])
    change(units, 'backend "mir-archon"', 'backend "mir-test"')
    changed = change(units, 'target "x86_64-unknown-linux-gnu"', 'target "wasm32-wasi"')
    assert '{"mir-archon", "wasm32-wasi"}, Bank::DivisionUnits, false, Op::Mod' in changed
    change(add, "pattern add_zero ", "pattern call ")
    change(add, "($x, 0) replace $x", "($return, 0) replace $return")
    check("unsupported MIR policy/bank version", manifest=core.replace("mir-policy 1;", "mir-policy 2;"))
    change(add, "mir-bank 1", "mir-bank 4", "unsupported MIR policy/bank version")
    change(add, "mir-bank 1 additive-identities", "mir-bank 1 scalar-equality", "bank declaration must name")
    check("MIR bank has no patterns", edit=(add, "mir-bank 1 additive-identities;"))
    check("expected a MIR policy document", manifest="(on1x.mir 1 1 ())")
    check("expected a MIR bank document", edit=(add, core))
    check("PARSE:", manifest=core.replace('target * enables load-reuse;', 'enables load-reuse;'))
    check("LEX:", manifest=core.replace('target * enables load-reuse;', 'target @ enables load-reuse;'))
    check("expected an unescaped policy name",
          manifest=core.replace('target * enables load-reuse;', 'target "two words" enables load-reuse;'))
    check("unsupported action/bank",
          manifest=core.replace('enables load-reuse;', 'enables additive-identities;'))
    check("no activation for MIR bank(s): load-reuse",
          manifest=core.replace('hint "local-memory" backend * target * enables load-reuse;', ''))
    change(add, "requires identity;", "requires assumption;", "unknown pattern opcode or proof")
    change(add, "replace $x", "replace $different", "not justified")
    change(add, "($x, 0)", "($x, 9223372036854775808)", "not a canonical i64")
    change(add, "($x, 0)", "($x, 0, 0)", "at most two operands")
    change(guarded, "requires nonzero;", "requires identity;", "not justified")
    change(guarded, "mode speculative", "mode safe", "not justified")
    change(units, "($x, 1)", "($x, -1)", "not justified")
    change(negation, "replace $original", "replace $x", "not justified")
    change(predicate, "requires bool-operands;", "requires identity;", "not justified")
    change("load-reuse", "requires local-load;", "requires local-store;", "not justified")
    change("ordered-reflexivity", 'match "lt"', 'match "eq"', "not justified")
    change("scalar-equality", "pattern eq_self", "pattern add_zero", "duplicate pattern name")
    check("duplicate pattern match and replacement", edit=(add, bodies[add] +
          '\npattern alias backend * target * mode safe match "add.checked"($y, 0) replace $y requires identity;\n'))
    check("duplicate activation", manifest=core + next(line for line in core.splitlines() if line.startswith("rewrite ")))
    check("exceeds 16 KiB", edit=(add, bodies[add] + "\n#" + "a" * 16384))
