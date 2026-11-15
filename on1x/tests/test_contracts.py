"""Canonical header/manual evidence and executable semantics without ExoML."""
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
from support import NativeTests, run

t = NativeTests()
root = t.root.parent.resolve()
if not all((root/'include'/('mulib-'+n+'.h')).is_file() for n in ['json', 'vec', 'parsec', 'mune']):
    print('Contract source/semantics tests: SKIP (canonical MuLIB headers absent)')
    sys.exit(77)
tool = t.root/'tools/on1x-contracts.py'
p = run([sys.executable, tool, 'lint'])
assert p.returncode == 0, p.stderr
lint = json.loads(p.stdout)
assert lint['profiles'] == {'json':20, 'vec':7, 'parsec':18, 'elvi-scrape':6, 'mune':52} and not lint['safe_export'] and not lint['gate_b']
p = run([sys.executable, tool, 'build-plan', 'json', 'vec', 'parsec'])
assert p.returncode == 0, p.stderr
plan = json.loads(p.stdout)
macros = ['MULIB_IMPL_PARSEC', 'MULIB_IMPL_STRBUF', 'MULIB_IMPL_UTF8', 'MULIB_IMPL_JSON', 'MULIB_IMPL_VEC']
assert plan['implementation_macros'] == macros
assert plan['implementation_translation_units'] == 1 and plan['inactive_optional_macros'] == ['MULIB_IMPL_PARSER']
assert plan['link_libraries'] == ['m'] and not plan['safe_export']
p = run([sys.executable, tool, 'build-plan', 'elvi-scrape'])
assert p.returncode == 0, p.stderr
elvi = json.loads(p.stdout)
assert elvi['implementation_translation_units'] == 1 and not elvi['safe_export']
assert len(elvi['implementation_macros']) == len(set(elvi['implementation_macros']))
assert elvi['implementation_macros'][-1] == 'MULIB_IMPL_ELVI_SCRAPE'
assert 'MULIB_IMPL_PARSER' not in elvi['implementation_macros']
p = run([sys.executable, tool, 'build-plan', 'mune']); assert p.returncode == 0, p.stderr
mune = json.loads(p.stdout)
assert mune['implementation_translation_units'] == 1 and not mune['safe_export']
assert mune['implementation_macros'].count('MULIB_IMPL_PARSER') == 1
assert mune['implementation_macros'].count('MULIB_IMPL_MUNE') == 1 and not mune['inactive_optional_macros']
assert mune['link_libraries'] == ['dl','m','pthread']
p = run([sys.executable, tool, 'build-plan', 'parsec', 'mune']); assert p.returncode == 0, p.stderr
combined = json.loads(p.stdout)
assert combined['implementation_macros'].count('MULIB_IMPL_PARSER') == 1 and not combined['inactive_optional_macros']
for name in ['unknown', '../json', 'json;false']:
    p = run([sys.executable, tool, 'build-plan', name])
    assert p.returncode != 0 and not p.stdout, p.stderr
with tempfile.TemporaryDirectory(prefix='contract-root-', dir=t.work) as tmp:
    fake = Path(tmp)
    (fake/'include').mkdir(); (fake/'manual').mkdir()
    for n in ['json', 'vec', 'parsec', 'elvi-scrape', 'mune']:
        for directory, suffix in [('include', '.h'), ('manual', '.md')]:
            shutil.copyfile(root/directory/('mulib-'+n+suffix), fake/directory/('mulib-'+n+suffix))
    evidence = json.loads((t.root/'contracts-exoml/catalog.json').read_text())['profiles']['mune']['evidence_sha256']
    for path in evidence:
        (fake/path).parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(root/path,fake/path)
    shutil.copyfile(root/'MANIFEST.yaml', fake/'MANIFEST.yaml')
    p = run([sys.executable, tool, '--root', fake, 'lint']); assert p.returncode == 0, p.stderr
    for path in ['include/mulib-vec.h','include/mulib-mune.h','manual/mulib-mune.md',*evidence]:
        original = (fake/path).read_bytes(); (fake/path).write_bytes(original+b'\n')
        p = run([sys.executable, tool, '--root', fake, 'lint'])
        assert p.returncode == 3 and not p.stdout and 'fingerprint drift' in p.stderr, p.stderr
        (fake/path).write_bytes(original)

spec = importlib.util.spec_from_file_location('on1x_contracts', tool)
module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
with tempfile.TemporaryDirectory(prefix='contract-profile-',dir=t.work) as tmp:
    destination = Path(tmp)/'profiles'; shutil.copytree(module.CONTRACTS,destination)
    original_contracts = module.CONTRACTS; module.CONTRACTS = destination
    original_catalog = json.loads((destination/'catalog.json').read_text())
    for active in [['MULIB_IMPL_NOT_A_DECLARED_OPTION'],['MULIB_IMPL_PARSER','MULIB_IMPL_PARSER'],[{}]]:
        data = json.loads(json.dumps(original_catalog)); data['profiles']['mune']['active_optional_macros'] = active
        (destination/'catalog.json').write_text(json.dumps(data))
        try: module.catalog(root)
        except ValueError: pass
        else: raise AssertionError('unsupported/duplicate optional implementation macro admitted')
    module.CONTRACTS = original_contracts
for text in ['{"x":1,"x":2}', '[NaN]', '['*65+']'*65]:
    try: module.loads(text)
    except (ValueError, RecursionError): pass
    else: raise AssertionError('unbounded/malformed JSON accepted')
# Atomic publication must never replace even an empty, concurrently existing directory.
if sys.platform.startswith('linux'):
    with tempfile.TemporaryDirectory(prefix='contract-publish-', dir=t.work) as tmp:
        parent = Path(tmp); source = parent/'source'; target = parent/'target'
        source.mkdir(); target.mkdir(); (source/'marker').write_text('ours')
        try: module.publish_new(source, target)
        except FileExistsError: pass
        else: raise AssertionError('snapshot publication replaced an existing directory')
        assert (source/'marker').read_text() == 'ours' and list(target.iterdir()) == []

native = t.work/'contract_semantics'
p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', root/'include',
         t.root/'tests/fixtures/contract_semantics.c', *t.flags, '-lm', '-o', native])
assert p.returncode == 0, p.stderr
p = run([native])
assert p.returncode == 0 and p.stdout == 'Contract semantics: PASS\n', (p.returncode, p.stdout, p.stderr)
example = t.work/'contract_usage'
p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', root/'include',
         t.root/'examples/contracts/contract_usage.c', *t.flags, '-lm', '-o', example])
assert p.returncode == 0, p.stderr
p = run([example]); assert p.returncode == 0 and p.stdout == '[42]\n', (p.returncode, p.stdout, p.stderr)
# The Mune recipe enables the owned parser explicitly, without widening the
# standalone Parsec recipe. ABI declarations still come from the real headers.
implementation = t.work/'mune-implementation.c'
implementation.write_text(''.join('#define '+m+'\n' for m in mune['implementation_macros'])+
                          ''.join('#include "'+str(root/e['header'])+'"\n' for e in mune['modules']))
example = t.work/'mune-contract-usage'
p = run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I',root/'include',
         implementation,t.root/'contracts-exotract/examples/mune.c',*t.flags,'-pthread','-lm','-ldl','-o',example])
assert p.returncode == 0, p.stderr
p = run([example]); assert p.returncode == 0 and p.stdout == 'mune: archive OK\n',(p.returncode,p.stdout,p.stderr)
print('Contract source inventory, build-plan, transfer/borrow/cleanup/allocation semantics: PASS')
