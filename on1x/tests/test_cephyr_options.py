#!/usr/bin/env python3
"""Exercise the pinned QML and FactIR driver options and generated/source diagnostics."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='on1x-cephyr-options-') as tmp:
    work = Path(tmp)
    obj, report = work / 'program.o', work / 'diagnostics.sarif'
    source = root / 'examples/hello.on1x'
    sentinel = b'previous object'

    def emit(*options, unit=source):
        return subprocess.run([compiler, '--emit-cephyr-object', unit, '-o', obj,
                               '--cephyr-sarif', report, *options], cwd=work,
                              capture_output=True, timeout=120)

    def results():
        return json.loads(report.read_bytes())['runs'][-1]['results']

    # Ambient profiles must not select passes, tools, or settings for the driver.
    (work / 'Cephyr.toml').write_text('invalid !\n')
    assert emit().returncode == 0 and not results()
    baseline = obj.read_bytes()
    assert emit('--cephyr-factir', 'off', '--cephyr-sandslab', 'off', '--cephyr-blackstone', 'off').returncode == 0
    assert obj.read_bytes() == baseline
    assert emit('--cephyr-sandslab', 'on').returncode == 0 and not results()
    assert obj.read_bytes() != baseline, 'SandSLAB did not shorten any Fibonacci constants'
    assert emit('--cephyr-blackstone', 'on').returncode == 0 and not results()
    assert obj.read_bytes() != baseline, 'Blackstone did not change the Fibonacci object'

    rule = work / 'trusted local.qpass'
    rule.write_text('rewrite local remove_zero : add(?x, 0) => ?x;\n')
    facts, proof = work / 'facts', work / 'proof'
    ok = emit('--cephyr-qml-pass', 'local', '--cephyr-trusted-qml-file', rule.name,
              '--cephyr-factir', 'on', '--cephyr-factir-facts', facts,
              '--cephyr-factir-smtlib', proof)
    assert ok.returncode == 0 and not results(), (ok.stderr, results())
    assert '[function]' in facts.read_text()
    assert 'eq(eq(x,0),0)' in proof.read_text() and 'Satie: unsat' in proof.read_text()

    obj.write_bytes(sentinel)
    for args, message in [
        (('--cephyr-profile', 'unknown'), 'unknown QML profile'),
        (('--cephyr-qml-pass', 'unknown'), 'phase'),
        (('--cephyr-qml-pass', 'canonicalize', '--cephyr-qml-param', 'canonicalize.typo=2'), 'parameter'),
        (('--cephyr-qml-fuel', '0'), 'positive unsigned'),
        (('--cephyr-factir', 'on', '--cephyr-factir-facts', facts), 'new output files'),
    ]:
        failed = emit(*args)
        assert failed.returncode and any(message in r['message']['text'] for r in results()), (args, results())
        assert obj.read_bytes() == sentinel
    for args in [('--cephyr-blackstone', 'maybe'), ('--cephyr-sandslab', 'maybe'),
                 ('--cephyr-factir', 'maybe'), ('--cephyr-factir-smtlib', obj),
                 ('--cephyr-trusted-qml-file', work / 'missing.qpass')]:
        prior = report.read_bytes()
        failed = emit(*args)
        assert failed.returncode and failed.stderr and obj.read_bytes() == sentinel and report.read_bytes() == prior
    wrong_mode = subprocess.run([compiler, '--emit-c', source, '--cephyr-factir', 'on', '-o', obj],
                                capture_output=True)
    assert wrong_mode.returncode and obj.read_bytes() == sentinel

    # Raw declaration conflicts with the actual C header. One error is at its
    # imported declaration and one is in the body expanded by the checks PPX.
    decl = work / 'foreign api.on1x'
    decl.write_text('# Header disagrees with this raw extern.\nextern fn strlen(n:i64) -> i64;\n')
    unit = work / 'unit.on1x'
    unit.write_text('import "foreign api.on1x";\nfn main() {\n  @checks("assert_eq", strlen(7), 1);\n}\n')
    ppx = ('--ppx', 'checks', root / 'stdppx/checks.py')
    original = decl.read_bytes()
    alias = subprocess.run([compiler, '--emit-cephyr-object', unit, *ppx, '-o', obj,
                            '--cephyr-sarif', decl], capture_output=True)
    assert alias.returncode and decl.read_bytes() == original and obj.read_bytes() == sentinel
    flags = ('--diagnostics', 'sarif') if '--syren' in sys.argv[2:] else ()
    bad = emit(*ppx, *flags, unit=unit)
    assert bad.returncode == 1 and obj.read_bytes() == sentinel, bad.stderr
    if flags:
        assert len(json.loads(bad.stderr)['runs']) == 2 and bad.stderr == report.read_bytes()
    generated = subprocess.check_output([compiler, '--emit-c', unit, *ppx]).decode().splitlines()
    seen = set()
    for error in results():
        physical = error['locations'][0]['physicalLocation']
        assert physical['artifactLocation']['uri'] == Path(str(unit) + '.c').as_uri()
        assert 'strlen(' in generated[physical['region']['startLine'] - 1]
        related, = error['relatedLocations']
        origin = related['physicalLocation']
        assert 'startColumn' not in origin['region']
        seen.add((origin['artifactLocation']['uri'], origin['region']['startLine']))
    assert seen == {(decl.as_uri(), 2), (unit.as_uri(), 3)}, seen
    bad = emit(unit=root / 'examples/f64.on1x')
    assert bad.returncode == 1 and results() and all('relatedLocations' not in r for r in results())
