#!/usr/bin/env python3
"""Opt-in CLI, exact provenance, saved MIR, and transactional failure checks."""
import argparse
from pathlib import Path
import sys
from support import run

cli = argparse.ArgumentParser()
cli.add_argument('compiler', type=Path)
cli.add_argument('root', type=Path)
cli.add_argument('work', type=Path)
cli.add_argument('--enabled', action='store_true')
args = cli.parse_args()
args.work.mkdir(parents=True, exist_ok=True)
source = args.root/'examples/analysis.on1x'
out = args.work/'preserved'

if not args.enabled:
    for mode in ['--emit-ir-facts', '--analyze-ir']:
        out.write_text('keep me')
        p = run([args.compiler, mode, source, '-o', out])
        assert p.returncode == 2 and 'ON1X_WITH_NOVOSTONE=ON' in p.stderr, p.stderr
        assert out.read_text() == 'keep me'
    print('Novostone-disabled CLI rejects explicitly PASS')
    sys.exit(0)

sys.path.insert(0, str(args.root/'stdlib'))
import sexpr

p = run([args.compiler, '--emit-ir', source])
assert p.returncode == 0, p.stderr
mir = args.work/'example.mir'
mir.write_text(p.stdout)
tree = sexpr.loads(p.stdout)
functions = {f[2]: f for f in tree[3]}
even, odd, main = [str(functions[name][1]) for name in ['even', 'odd', 'main']]
for mode, suffix in [('--emit-ir-facts', '.dl'), ('--analyze-ir', '.analysis')]:
    path = args.work/('example'+suffix)
    p = run([args.compiler, mode, source, '-o', path])
    assert p.returncode == 0, p.stderr
    a = run([args.compiler, mode, source])
    b = run([args.compiler, mode, '--ir-input', mir])
    assert a.returncode == b.returncode == 0 and a.stdout == b.stdout == path.read_text(), (a.stderr, b.stderr)

report = sexpr.loads((args.work/'example.analysis').read_text())
assert report[:2] == ['on1x.mir-analysis', '1'] and report[2] == ['revision', '1']
assert report[3] == ['source', mir.read_text()]
assert report[4][0] == 'rules' and report[4][1]
queries = {q[1]: q for q in report[5]}
calls = {tuple(row) for row in queries['Calls'][3]}
assert calls == {(even, even), (even, odd), (odd, even), (odd, odd), (main, even), (main, odd)}
assert 'Call' in queries['Calls'][2]
assert len(queries['Reachable'][3]) == sum(len(f[6]) for f in functions.values())
assert [main, 'io'] in queries['MayEffect'][3]

for budget in ['0', '1', '100']:
    out.write_text('keep me')
    p = run([args.compiler, '--analyze-ir', source, '--analysis-work', budget, '-o', out])
    assert p.returncode == 75 and 'budget' in p.stderr and not p.stdout, p.stderr
    assert out.read_text() == 'keep me'
for options in [['--emit-c', '--analysis-work', '1'], ['--analyze-ir', '--analysis-work', '-1'],
                ['--analyze-ir', '--analysis-work', '18446744073709551616']]:
    p = run([args.compiler, source, *options])
    assert p.returncode == 2, p.stderr

unsupported = args.work/'unsupported.on1x'
unsupported.write_text('extern fn raw()->i64;fn main(){}')
malformed = args.work/'invalid.mir'
malformed.write_text('(on1x.mir 999 1 ())')
for mode in ['--emit-ir-facts', '--analyze-ir']:
    for path, options, diagnostic in [(unsupported, [], 'ON1X-MIR-UNSUPPORTED'),
                                      (malformed, ['--ir-input'], 'version')]:
        out.write_text('keep me')
        p = run([args.compiler, mode, path, *options, '-o', out])
        assert p.returncode == 1 and diagnostic in p.stderr and out.read_text() == 'keep me', p.stderr
    out.write_text('keep me')
    p=run([args.compiler, mode, args.root/'examples/float_tuples.on1x', '-o', out])
    assert p.returncode==1 and 'outside relational mapping v1' in p.stderr and out.read_text()=='keep me',p.stderr

p = run([args.compiler, '--run-ir', source])
assert p.returncode == 0 and p.stdout == 'true\nfalse\ntrue\nfalse\n', p.stderr
# CLI analyses must describe the selected optimized artifact, not the pre-pass IR.
optimized = run([args.compiler, '--emit-ir', '--optimize-ir', args.root/'examples/optimize.on1x'])
p = run([args.compiler, '--analyze-ir', '--optimize-ir', args.root/'examples/optimize.on1x'])
assert optimized.returncode == p.returncode == 0, p.stderr
assert sexpr.loads(p.stdout)[3] == ['source', optimized.stdout]
print('Novostone CLI, source identity, saved artifacts, rejection and budgets PASS')
