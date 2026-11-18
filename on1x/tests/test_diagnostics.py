"""Black-box reporting, repair and unchanged-language regressions (no PPX execution)."""
import json
from pathlib import Path
import subprocess
from support import NativeTests

t = NativeTests()
compiler, root = str(t.compiler), t.work

def run(*args):
    return subprocess.run([compiler, '--no-prelude', *map(str, args)], capture_output=True, text=True, timeout=20)

def fixture(name, source):
    path = root / name
    path.write_text(source)
    return path

def report(path, expected):
    p = run('--check', '--diagnostics', 'json', path)
    assert p.returncode == 1 and not p.stdout, p
    data = json.loads(p.stderr)
    d = data['diagnostics'][0]
    assert d['id'] == expected, d
    assert p.stderr == run('--check', '--diagnostics', 'json', path).stderr
    return d

bad = fixture('missing.on1x', 'fn main()->i64{return 42}')
original = bad.read_bytes()
d = report(bad, 'ON1X-PARSE-EXPECTED')
assert d['repairs'] and all(c['status'] == 'candidate' for c in d['repairs'])
assert d['location']['offset'] == len(original) - 1
out = root / 'proposal.on1x'
# CTest may reuse its artifact directory: only remove this test's own output.
if out.exists():
    out.unlink()
p = run('--repair', bad, '-o', out)
assert p.returncode == 0 and 'validated' in p.stderr, p
assert out.read_text() == 'fn main()->i64{return 42;}'
assert bad.read_bytes() == original
assert run('--check', out).returncode == 0
executable = t.build('compiled-proposal', source=out)
executed = subprocess.run([executable], capture_output=True, text=True, timeout=20)
assert executed.returncode == 42 and not executed.stdout and not executed.stderr, executed
# Explicitly refuse all overwrite and in-place cases.
for destination in [out, bad]:
    before = destination.read_bytes()
    p = run('--repair', bad, '-o', destination)
    assert p.returncode == 2 and destination.read_bytes() == before, p
assert run('--repair', bad).returncode == 2
assert run('--repair', bad, '--ppx', 'x', 'never-execute', '-o', root / 'ppx-output').returncode == 2

typo = fixture('type.on1x', 'fn main()->i64{let x:booll=true;return 0;}')
assert any(c['edits'][0]['replacement'] == 'bool' for c in report(typo, 'ON1X-PARSE-TYPE')['repairs'])
type_out = root / 'type-proposal.on1x'
if type_out.exists():
    type_out.unlink()
assert run('--repair', typo, '-o', type_out).returncode == 0
assert run('--check', type_out).returncode == 0

# A syntactic edit is rejected if it leaves a type error. No guessed coercions.
typed = fixture('ill-typed.on1x', 'fn main()->i64{return true}')
p = run('--repair', '--diagnostics', 'json', typed, '-o', root / 'rejected-output')
assert p.returncode == 1 and not (root / 'rejected-output').exists(), p
assert all(c['status'] == 'inconclusive'
           for c in json.loads(p.stderr)['diagnostics'][0]['repairs'])
report(fixture('undefined.on1x', 'fn main()->i64{return missing;}'), 'ON1X-SEMA')
report(fixture('mismatch.on1x', 'fn main()->i64{return true;}'), 'ON1X-TYPE')
assert report(fixture('semantic.on1x', 'fn main()->i64{let x=1;x=2;return x;}'), 'ON1X-SEMA')['repairs'] == []
nul = root / 'nul.on1x'
nul.write_bytes(b'fn main(){\x00}')
report(nul, 'ON1X-LEX-NUL')
report(fixture('token.on1x', 'fn main(){`;}'), 'ON1X-LEX-TOKEN')

# Wetstone findings are advisory at standard scrutiny and errors at strict
# scrutiny. CLI policy overrides a source directive, which overrides project
# policy. Builds without Wetstone remain usable and emit no fabricated result.
division = fixture('division.on1x', 'fn main()->i64{return 42/0;}')
p = run('--check', '--diagnostics', 'json', division)
wetstone_enabled = bool(p.stderr)
if wetstone_enabled:
    finding = json.loads(p.stderr)['diagnostics'][0]
    assert p.returncode == 0 and finding['id'] == 'ON1X-ANALYSIS-DIVZERO', p
    assert finding['severity'] == 'warning' and p.stdout.startswith('OK:'), p
    assert any('advisory proposal' in note and 'not compiler-revalidated' in note
               for note in finding['notes'])
    assert p.stderr == run('--check', '--diagnostics', 'json', division).stderr
    p = run('--check', '--diagnostics', 'json', '--scrutiny', 'strict', division)
    assert p.returncode == 1 and not p.stdout
    assert json.loads(p.stderr)['diagnostics'][0]['severity'] == 'error'
    directed = fixture('directed.on1x', '@scrutiny strict;\nfn main()->i64{return 42/0;}')
    assert run('--check', '--project-scrutiny', 'off', directed).returncode == 1
    assert run('--check', '--project-scrutiny', 'strict', '--scrutiny', 'off', directed).returncode == 0
    p = run('--check', '--diagnostics', 'json', '--no-analysis-repairs', division)
    assert p.returncode == 0
    assert all('advisory proposal' not in note
               for note in json.loads(p.stderr)['diagnostics'][0]['notes'])
else:
    assert p.returncode == 0 and p.stdout.startswith('OK:'), p
for bad_policy in [
        ('--scrutiny', 'unknown'),
        ('--scrutiny-work', '0'),
]:
    p = run('--check', '--diagnostics', 'json', *bad_policy, division)
    assert p.returncode == 1 and not p.stdout, p
    assert json.loads(p.stderr)['diagnostics'][0]['id'] == 'ON1X-DIAGNOSTIC-SCRUTINY'

# External catalogs own presented identities, levels, messages, remediation,
# scrutiny/check mappings, repair policy and native PPX declarations. These
# replacements are loaded by the already-built compiler.
catalog_source = t.root / 'stdlib/diagnostics/catalog.json'
catalog = json.loads(catalog_source.read_text())
parse_entry = next(item for item in catalog['diagnostics']
                   if item.get('match') == 'ON1X-PARSE-EXPECTED')
parse_entry.update(id='EXTERNAL-PARSE', level='notice',
                   message='external catalog message',
                   remediation='external catalog remediation')
next(level for level in catalog['levels'] if level['kind'] == 0)['name'] = 'notice'
ppx_entry = next(item for item in catalog['diagnostics']
                 if item.get('processor') == 'diagnostics')
ppx_entry.update(id='EXTERNAL-PPX', level='notice',
                 message='external PPX catalog message')
catalog_path = root / 'catalog.json'
catalog_path.write_text(json.dumps(catalog, sort_keys=True))
p = run('--check', '--diagnostics', 'json', '--diagnostic-catalog', catalog_path, bad)
assert p.returncode == 1
replaced = json.loads(p.stderr)['diagnostics'][0]
assert (replaced['id'], replaced['severity'], replaced['message'],
        replaced['remediation']) == (
            'EXTERNAL-PARSE', 'notice', 'external catalog message',
            'external catalog remediation')

# Replacements and candidate identity/labels are repair policy data as well.
repair_catalog = json.loads(catalog_source.read_text())
spelling = next(policy for policy in repair_catalog['repair_policies']
                if policy['kind'] == 2)
spelling.update(name='spelling-review', candidate_id='review-{replacement}',
                replacements=['bool'], maximum_distance=1,
                notice='replacement repair notice')
next(item for item in repair_catalog['diagnostics']
     if item.get('match') == 'ON1X-PARSE-TYPE')['repair'] = 'spelling-review'
repair_path = root / 'repair-catalog.json'
repair_path.write_text(json.dumps(repair_catalog))
p = run('--check', '--diagnostics', 'json', '--diagnostic-catalog',
        repair_path, typo)
assert p.returncode == 1
diagnosed = json.loads(p.stderr)['diagnostics'][0]
assert [candidate['id'] for candidate in diagnosed['repairs']] == ['review-bool']
assert 'replacement repair notice' in diagnosed['notes']
spelling['replacements'] = ['i64']
repair_path.write_text(json.dumps(repair_catalog))
p = run('--check', '--diagnostics', 'json', '--diagnostic-catalog',
        repair_path, typo)
assert p.returncode == 1 and not json.loads(p.stderr)['diagnostics'][0]['repairs']

if wetstone_enabled:
    analysis_catalog = json.loads(catalog_source.read_text())
    for policy in analysis_catalog['scrutiny']:
        policy['checks'] = [
            'division-check' if check == 'arithmetic' else check
            for check in policy['checks']
        ]
        if policy['name'] == 'standard':
            policy['name'] = 'review'
        elif policy['name'] == 'strict':
            policy['name'] = 'audit'
    analysis_catalog['default_scrutiny'] = 'review'
    for item in analysis_catalog['diagnostics']:
        if item.get('check') == 'arithmetic':
            item['check'] = 'division-check'
        if item.get('minimum_scrutiny') == 'standard':
            item['minimum_scrutiny'] = 'review'
        elif item.get('minimum_scrutiny') == 'strict':
            item['minimum_scrutiny'] = 'audit'
    division_entry = next(
        item for item in analysis_catalog['diagnostics']
        if item.get('upstream') == {
            'rule': 'WET-DIVZERO', 'level': 1, 'adapter': 0
        })
    division_entry.update(id='EXTERNAL-DIVISION',
                          message='external division message')
    analysis_catalog_path = root / 'analysis-catalog.json'
    analysis_catalog_path.write_text(json.dumps(analysis_catalog, sort_keys=True))
    p = run('--check', '--diagnostics', 'json', '--diagnostic-catalog',
            analysis_catalog_path, division)
    assert p.returncode == 0 and p.stdout.startswith('OK:'), p
    replaced = json.loads(p.stderr)['diagnostics'][0]
    assert (replaced['id'], replaced['message'], replaced['check']) == (
        'EXTERNAL-DIVISION', 'external division message', 'division-check')
    p = run('--check', '--diagnostics', 'json', '--diagnostic-catalog',
            analysis_catalog_path, '--scrutiny', 'audit', division)
    assert p.returncode == 1 and json.loads(p.stderr)['diagnostics'][0][
        'severity'] == 'error', p
    p = run('--check', '--diagnostics', 'json', '--diagnostic-catalog',
            analysis_catalog_path, '--scrutiny', 'standard', division)
    assert p.returncode == 1
    assert json.loads(p.stderr)['diagnostics'][0][
        'id'] == 'ON1X-DIAGNOSTIC-SCRUTINY'

    disabled_catalog = json.loads(json.dumps(analysis_catalog))
    next(policy for policy in disabled_catalog['scrutiny']
         if policy['name'] == 'review')['checks'].remove('division-check')
    disabled_path = root / 'disabled-analysis-catalog.json'
    disabled_path.write_text(json.dumps(disabled_catalog, sort_keys=True))
    p = run('--check', '--diagnostics', 'json', '--diagnostic-catalog',
            disabled_path, division)
    assert p.returncode == 0 and p.stdout.startswith('OK:') and not p.stderr, p

malformed_catalog = root / 'malformed-catalog.json'
malformed_catalog.write_text(
    '{"schema":"on1x.diagnostic-catalog/v2","schema":"duplicate"}')
p = run('--check', '--diagnostics', 'json', '--diagnostic-catalog', malformed_catalog, bad)
assert p.returncode == 1 and not p.stdout
assert json.loads(p.stderr)['diagnostics'][0]['id'] == 'ON1X-DIAGNOSTIC-CATALOG'

# Native and ordinary external PPXs use descriptor 3, never stderr, and must
# register a cataloged PPX ID before emission.
ppx_source = fixture('ppx-diagnostic.on1x',
                     'fn main()->i64{return @diagnostics("warn",42);}')
native_ppx = t.compiler.parent / 'stdppx/on1x-ppx-diagnostics'
if native_ppx.is_file():
    p = run('--check', '--diagnostics', 'json', '--stdppx-path', native_ppx.parent,
            '--stdppx', 'diagnostics', ppx_source)
    assert p.returncode == 0 and p.stdout.startswith('OK:'), p
    emitted = json.loads(p.stderr)['diagnostics'][0]
    assert emitted['id'] == 'ON1X-PPX-DIAGNOSTIC' and emitted['origin'] == 'ppx'
    # The same compiled native processor consumes the replacement declaration.
    p = run('--check', '--diagnostics', 'json', '--diagnostic-catalog',
            catalog_path, '--stdppx-path', native_ppx.parent,
            '--stdppx', 'diagnostics', ppx_source)
    assert p.returncode == 0 and p.stdout.startswith('OK:'), p
    emitted = json.loads(p.stderr)['diagnostics'][0]
    assert (emitted['id'], emitted['severity'], emitted['message']) == (
        'EXTERNAL-PPX', 'notice', 'external PPX catalog message')

def external_ppx(name, records):
    path = root / name
    path.write_text(
        '#!/usr/bin/python3\n'
        'import os,sys\n'
        f'os.write(3,{records!r})\n'
        'sys.stdout.buffer.write(sys.stdin.buffer.read())\n')
    path.chmod(0o755)
    return path

plugin_source = fixture('external-ppx-source.on1x', 'fn main()->i64{return 42;}')
location = f'(Loc {json.dumps(str(plugin_source.resolve()))} 0 1 1)'
wire = (f'(register "ON1X-PPX-DIAGNOSTIC")\n'
        f'(emit "ON1X-PPX-DIAGNOSTIC" {location})\n').encode()
plugin = external_ppx('diagnostic-ppx.py', wire)
p = run('--check', '--diagnostics', 'json', '--ppx', 'external_diagnostic', plugin, plugin_source)
assert p.returncode == 0 and p.stdout.startswith('OK:'), p
assert json.loads(p.stderr)['diagnostics'][0]['message'] == (
    'The selected diagnostics PPX emitted its cataloged advisory.')

# A catalog-selected failing level emits exactly one registered finding; it
# must not acquire a second generic compiler identity during exception routing.
failure_catalog = json.loads(catalog_source.read_text())
next(item for item in failure_catalog['diagnostics']
     if item.get('processor') == 'diagnostics')['level'] = 'error'
failure_catalog_path = root / 'failure-ppx-catalog.json'
failure_catalog_path.write_text(json.dumps(failure_catalog))
p = run('--check', '--diagnostics', 'json', '--diagnostic-catalog',
        failure_catalog_path, '--ppx', 'external_diagnostic', plugin, plugin_source)
assert p.returncode == 1 and not p.stdout
diagnosed = json.loads(p.stderr)['diagnostics']
assert len(diagnosed) == 1 and diagnosed[0]['id'] == 'ON1X-PPX-DIAGNOSTIC'
assert diagnosed[0]['severity'] == 'error'

# PPX records cannot supply message text at all. Catalog-controlled text still
# cannot inject terminal lines or control sequences into text output.
hostile_message = r'external advisory\nforged: error\x1b[31mred\x1b[0m'
hostile_catalog = json.loads(catalog_source.read_text())
next(item for item in hostile_catalog['diagnostics']
     if item.get('processor') == 'diagnostics')['message'] = (
         'external advisory\nforged: error\x1b[31mred\x1b[0m')
hostile_catalog_path = root / 'hostile-catalog.json'
hostile_catalog_path.write_text(json.dumps(hostile_catalog, sort_keys=True))
hostile_wire = (f'(register "ON1X-PPX-DIAGNOSTIC")\n'
                 f'(emit "ON1X-PPX-DIAGNOSTIC" {location})\n').encode()
hostile_plugin = external_ppx('hostile-diagnostic-ppx.py', hostile_wire)
p = run('--check', '--color', 'always', '--diagnostic-catalog',
        hostile_catalog_path, '--ppx', 'hostile_diagnostic', hostile_plugin,
        plugin_source)
assert p.returncode == 0 and p.stdout.startswith('OK:'), p
assert p.stderr.count('\x1b') == 2, repr(p.stderr)
assert r'external advisory\nforged: error\x1b[31mred\x1b[0m' in p.stderr
assert '\nforged: error' not in p.stderr
p = run('--check', '--diagnostics', 'json', '--color', 'always',
        '--diagnostic-catalog', hostile_catalog_path, '--ppx',
        'hostile_diagnostic', hostile_plugin, plugin_source)
assert p.returncode == 0 and '\x1b' not in p.stderr
assert json.loads(p.stderr)['diagnostics'][0]['message'] == (
    'external advisory\nforged: error\x1b[31mred\x1b[0m')

# PPX-controlled locations never authorize reading an arbitrary host path.
# Without a loaded-graph snapshot SARIF must omit, rather than derive, a column.
foreign = fixture('not-a-dependency.on1x', 'fn hidden(){let s="😀";}')
foreign_location = f'(Loc {json.dumps(str(foreign.resolve()))} 0 1 1)'
foreign_wire = (f'(register "ON1X-PPX-DIAGNOSTIC")\n'
                f'(emit "ON1X-PPX-DIAGNOSTIC" {foreign_location})\n').encode()
p = run('--check', '--diagnostics', 'sarif', '--ppx', 'foreign_location',
        external_ppx('foreign-location-ppx.py', foreign_wire), plugin_source)
assert p.returncode == 0, p
region = json.loads(p.stderr)['runs'][0]['results'][0]['locations'][0][
    'physicalLocation']['region']
assert 'startColumn' not in region, region

for index, (name, records) in enumerate([
    ('unregistered-ppx.py',
     f'(emit "ON1X-PPX-DIAGNOSTIC" {location})\n'.encode()),
    ('free-form-message-ppx.py',
     f'(register "ON1X-PPX-DIAGNOSTIC")\n'
     f'(emit "ON1X-PPX-DIAGNOSTIC" "forbidden" {location})\n'.encode()),
    ('malformed-ppx.py', b'(emit "ON1X-PPX-DIAGNOSTIC")\n'),
    ('unterminated-ppx.py', b'(register "ON1X-PPX-DIAGNOSTIC")'),
    ('bounded-ppx.py', b'(register "ON1X-PPX-DIAGNOSTIC")\n' * 257),
]):
    p = run('--check', '--diagnostics', 'json', '--ppx', f'bad_ppx_{index}', external_ppx(name, records),
            plugin_source)
    assert p.returncode == 1 and not p.stdout, p
    failure = json.loads(p.stderr)['diagnostics'][0]
    assert failure['id'] == 'ON1X-DIAGNOSTIC-PROTOCOL', failure

# Text diagnostics color only the severity and never contaminate stdout or
# structured JSON.
p = run('--check', '--color', 'always', bad)
assert p.returncode == 1 and not p.stdout and '\x1b[31merror\x1b[0m' in p.stderr
assert '\x1b[' not in run('--check', '--color', 'never', bad).stderr
assert '\x1b[' not in run('--check', '--diagnostics', 'json', '--color', 'always', bad).stderr

# Imported errors retain imported-file identity, not entry-file offsets.
dep = fixture('dep.on1x', 'fn answer()->i64{return 42}')
entry = fixture('entry.on1x', 'import dep;fn main()->i64{return dep.answer();}')
assert Path(report(entry, 'ON1X-PARSE-EXPECTED')['location']['file']) == dep.resolve()
p = run('--repair', entry, '-o', root / 'wrong-file-output')
assert p.returncode == 1 and not (root / 'wrong-file-output').exists(), p
dep.write_text('fn answer()->i64{return 42;}')
assert run('--check', entry).returncode == 0

# Validation must check the whole import graph after changing the entry.
dep.write_text('fn answer()->i64{return true;}')
entry.write_text('import dep;fn main()->i64{return dep.answer()}')
p = run('--repair', entry, '-o', root / 'bad-dependency-output')
assert p.returncode == 1 and not (root / 'bad-dependency-output').exists(), p

# Existing symlinks, even dangling ones, cannot be repair destinations.
link = root / 'output-link'
if link.is_symlink():
    link.unlink()
link.symlink_to('nonexistent-target')
assert run('--repair', bad, '-o', link).returncode == 2
assert not (root / 'nonexistent-target').exists()

source_ppx = fixture('source-ppx.on1x', '@ppx checks "never-execute"\nfn main()->i64{return 42}')
p = run('--repair', '--diagnostics', 'json', source_ppx, '-o', root / 'source-ppx-output')
assert p.returncode == 1 and not (root / 'source-ppx-output').exists(), p
assert any(c['status'] == 'inconclusive' and 'never executes source PPX' in c['reason']
           for c in json.loads(p.stderr)['diagnostics'][0]['repairs'])

unicode = fixture('space #%.on1x', 'fn main()->i64{let s="😀";return 1}')
p = run('--check', '--diagnostics', 'sarif', unicode)
assert p.returncode == 1
sarif = json.loads(p.stderr)
assert sarif['version'] == '2.1.0'
result = sarif['runs'][0]['results'][0]
location = result['locations'][0]['physicalLocation']
assert '%20%23%25' in location['artifactLocation']['uri']
prefix = unicode.read_text().split('}')[0]
assert location['region']['startColumn'] == len(prefix.encode('utf-16-le')) // 2 + 1
assert sarif['runs'][0]['columnKind'] == 'utf16CodeUnits'
assert 'fixes' not in result  # A suggestion is not certified semantic repair.

# Unchanged acceptance and byte-identical C output across reporting modes.
valid = fixture('valid.on1x', 'fn main()->i64{let f=fn(x:i64)->i64{return x+1;};return f(41);}')
base = run('--emit-c', valid)
assert base.returncode == 0, base
for fmt in ['text', 'json', 'sarif']:
    p = run('--emit-c', '--diagnostics', fmt, valid)
    assert (p.returncode, p.stdout, p.stderr) == (base.returncode, base.stdout, base.stderr)
for source in ['fn main(){let x=1}', 'fn main()->i64{return true;}', 'fn main(){let x=1;x=2;}']:
    invalid = fixture('invalid.on1x', source)
    assert all(run('--check', '--diagnostics', fmt, invalid).returncode == 1 for fmt in ['text', 'json', 'sarif'])
print('diagnostics and repair CLI OK')
