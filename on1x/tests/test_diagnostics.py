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
assert all(c['status'] == 'rejected' for c in json.loads(p.stderr)['diagnostics'][0]['repairs'])
report(fixture('undefined.on1x', 'fn main()->i64{return missing;}'), 'ON1X-SEMA')
report(fixture('mismatch.on1x', 'fn main()->i64{return true;}'), 'ON1X-TYPE')
assert report(fixture('semantic.on1x', 'fn main()->i64{let x=1;x=2;return x;}'), 'ON1X-SEMA')['repairs'] == []
nul = root / 'nul.on1x'
nul.write_bytes(b'fn main(){\x00}')
report(nul, 'ON1X-LEX-NUL')
report(fixture('token.on1x', 'fn main(){`;}'), 'ON1X-LEX-TOKEN')

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
