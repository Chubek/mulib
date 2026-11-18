"""Catalog replacement and bootstrap checks, including no-Syren builds."""
import copy
import json
import subprocess
from support import NativeTests

t = NativeTests()
original = json.loads((t.root / 'stdlib/diagnostics/catalog.json').read_text())
bad = t.source('bad', 'fn main()->i64{return 42}')
valid = t.source('valid', 'fn main()->i64{return 42;}')
division = t.source('division', 'fn main()->i64{return 42/0;}')


def run(*args):
    return subprocess.run(
        [str(t.compiler), '--no-prelude', *map(str, args)],
        capture_output=True, text=True, timeout=20)


def catalog_file(name, catalog):
    path = t.work / name
    path.write_text(json.dumps(catalog), encoding='utf-8')
    return path


catalog = copy.deepcopy(original)
parse = next(item for item in catalog['diagnostics']
             if item.get('match') == 'ON1X-PARSE-EXPECTED')
parse.update(id='REPLACEMENT-PARSE', level='note',
             message='replacement text', category='replacement-category',
             phase='replacement-phase', guidance='replacement guidance',
             remediation='replacement remediation')
next(level for level in catalog['levels'] if level['kind'] == 0).update(
    name='notice', color=35)
parse['level'] = 'notice'
for item in catalog['diagnostics']:
    if item['role'] == 3:
        item.update(id='REPLACEMENT-POLICY', message='replacement policy error')
selected = catalog_file('replacement.json', catalog)

for color in ('never', 'always'):
    p = run('--check', '--diagnostic-catalog', selected, '--color', color, bad)
    assert p.returncode == 1 and not p.stdout, p
    for text in ('REPLACEMENT-PARSE', 'replacement text', 'replacement-category',
                 'replacement-phase', 'replacement guidance',
                 'replacement remediation'):
        assert text in p.stderr, p.stderr
    if color == 'always':
        assert p.stderr.count('\x1b') == 2 and '\x1b[35mnotice\x1b[0m' in p.stderr
    else:
        assert '\x1b' not in p.stderr

p = run('--check', '--diagnostic-catalog', selected, '--scrutiny', 'missing', valid)
assert p.returncode == 1 and not p.stdout, p
assert 'REPLACEMENT-POLICY' in p.stderr and 'replacement policy error' in p.stderr

# Catalog rejection must never use partly parsed untrusted bootstrap text.
malformed = copy.deepcopy(catalog)
next(item for item in malformed['diagnostics'] if item['role'] == 2).update(
    id='UNTRUSTED-BOOTSTRAP', message='untrusted bootstrap text')
malformed['diagnostics'][-1]['level'] = 'missing-level'
broken = catalog_file('malformed.json', malformed)
p = run('--check', '--diagnostic-catalog', broken, bad)
trusted_failure = next(item for item in original['diagnostics'] if item['role'] == 2)
assert p.returncode == 1 and not p.stdout, p
assert trusted_failure['id'] in p.stderr and trusted_failure['message'] in p.stderr
assert 'UNTRUSTED-BOOTSTRAP' not in p.stderr
assert 'untrusted bootstrap text' not in p.stderr

for path in (t.work / 'absent-catalog.json', t.work):
    p = run('--check', '--diagnostic-catalog', path, bad)
    assert p.returncode == 1 and trusted_failure['id'] in p.stderr, p

# Oversized legacy details must not throw recursively while reporting an error.
long_name = t.source('long-name', 'fn main()->i64{return ' + 'x' * 20000 + ';}')
p = run('--check', long_name)
report_failure = next(item for item in original['diagnostics'] if item['role'] == 5)
assert p.returncode == 1 and not p.stdout and len(p.stderr) < 16384, p
assert report_failure['id'] in p.stderr and report_failure['message'] in p.stderr

# Scrutiny names, check activation, and failure semantics are data in both
# renderers. Absence of Wetstone is not counted as exercised analysis.
p = run('--check', division)
finding = next(item for item in original['diagnostics']
               if item.get('upstream') == {
                   'rule': 'WET-DIVZERO', 'level': 1, 'adapter': 0})
if finding['id'] in p.stderr:
    changed = copy.deepcopy(original)
    next(item for item in changed['diagnostics'] if item['id'] == finding['id']).update(
        id='REPLACEMENT-DIVISION', level='note', message='replacement division')
    next(level for level in changed['levels'] if level['kind'] == 0)['failure'] = True
    enabled = catalog_file('analysis-enabled.json', changed)
    p = run('--check', '--diagnostic-catalog', enabled, division)
    assert p.returncode == 1 and not p.stdout, p
    assert 'REPLACEMENT-DIVISION' in p.stderr and 'replacement division' in p.stderr
    next(policy for policy in changed['scrutiny']
         if policy['name'] == changed['default_scrutiny'])['checks'].remove('arithmetic')
    disabled = catalog_file('analysis-disabled.json', changed)
    p = run('--check', '--diagnostic-catalog', disabled, division)
    assert p.returncode == 0 and p.stdout.startswith('OK:') and not p.stderr, p
else:
    assert p.returncode == 0 and not p.stderr, p
    print('Wetstone replacement activation: unavailable (not exercised)')

# Even without Syren, the native declaration and host-owned message are selected
# by the runtime catalog, not compiled into the native processor.
native = t.compiler.parent / 'stdppx/on1x-ppx-diagnostics'
if native.is_file():
    ppx_catalog = copy.deepcopy(catalog)
    ppx = next(item for item in ppx_catalog['diagnostics'] if 'processor' in item)
    ppx.update(id='REPLACEMENT-PPX', level='notice', message='replacement PPX')
    ppx_path = catalog_file('ppx.json', ppx_catalog)
    source = t.source('native', 'fn main()->i64{return @diagnostics("warn",42);}')
    p = run('--check', '--diagnostic-catalog', ppx_path, '--stdppx-path',
            native.parent, '--stdppx', 'diagnostics', source)
    assert p.returncode == 0 and p.stdout.startswith('OK:'), p
    assert 'REPLACEMENT-PPX' in p.stderr and 'notice: replacement PPX' in p.stderr
else:
    print('Native diagnostic PPX: not built (not exercised)')

print('catalog replacement and trusted bootstrap runtime OK')
