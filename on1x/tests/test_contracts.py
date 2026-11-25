"""Canonical header/manual evidence and executable semantics without ExoML."""
import importlib.util
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
from support import NativeTests, run

t = NativeTests()
root = t.root.parent.resolve()
if not all((root/'kernels'/('mulib-'+n+'.h')).is_file() for n in ['json', 'vec', 'parsec', 'mune']):
    print('Contract source/semantics tests: SKIP (canonical MuLIB headers absent)')
    sys.exit(77)
tool = t.root/'tools/on1x-contracts.py'
p = run([sys.executable, tool, 'lint'])
assert p.returncode == 0, p.stderr
lint = json.loads(p.stdout)
assert lint['profiles'] == {'json':20, 'vec':7, 'parsec':18, 'elvi-scrape':9, 'mune':53,
                            'quic':12, 'http3':8} and not lint['safe_export'] and not lint['gate_b']
exo = t.root/'tools/on1x-exotract.py'
p = run([sys.executable, tool, '--root', root, 'list'])
assert p.returncode == 0, p.stderr
inventory = json.loads(p.stdout)
p = run([sys.executable, exo, '--root', root, 'list'])
assert p.returncode == 0, p.stderr
assert inventory == dict(json.loads(p.stdout), schema='on1x.contract-inventory/v1')
modules, coverage = inventory['modules'], inventory['coverage']
auxiliary = set(inventory['auxiliary_headers'])
assert not auxiliary, sorted(auxiliary)
registered = {m['header'] for m in modules.values()}
reserved = set(inventory['reserved_headers'])
assert not registered & (auxiliary | reserved)
assert registered | auxiliary | reserved == {
    p.relative_to(root).as_posix() for p in (root/'kernels').glob('mulib-*.h')}
assert set(coverage) == set(modules) and inventory['safe_export'] is False
assert {n for n, p in coverage.items() if p['snapshot_profile']} == lint['profiles'].keys()
assert coverage['clamp']['semantic_sidecar'] == 'clamp.exocontract.json'
assert coverage['clamp']['sidecar_header_matches'] is True
pretty = json.loads((t.root/'contracts-exoml/prettysexpr.exocontract.json').read_text())
pretty_revision = 'sha256:'+hashlib.sha256((root/'kernels/mulib-prettysexpr.h').read_bytes()).hexdigest()
assert coverage['prettysexpr']['sidecar_header_matches'] is True
assert pretty['bundle_provenance']['revision'] == pretty_revision
assert coverage['elvi-scrape']['archive_recipe'] == 'exoml-core'
for name in ['elvi-script', 'elvi-script-ipc', 'elvi-script-logic', 'elvi-script-transport']:
    assert not coverage[name]['snapshot_profile'] and coverage[name]['archive_recipe'] is None
    assert coverage[name]['discovery_only_reason'] and coverage[name]['semantic_sidecar'] is None
for name in ['quic', 'http3']:
    assert coverage[name] == dict(semantic_sidecar=name+'.exocontract.json',
                                 sidecar_header_matches=True, snapshot_profile=True,
                                 archive_recipe='exoml-core', discovery_only_reason=None)
    assert modules[name]['extra_macros'] == ['MULIB_IMPL_'+name.upper()+'_CLIENT']
assert coverage['ipc-framed']['semantic_sidecar'] == 'sandbox/ipc-framed.exocontract.json'
assert coverage['b+tree']['semantic_sidecar'] is None
assert inventory['non_kernel_sidecars'] == ['prettyprint.exocontract.json']

if 'glx-core' in modules:
    assert modules['glx-core']['implementation'] == 'static-inline'
    assert modules['glx-core']['macro'] is None
    assert coverage['glx-core']['archive_recipe'] is None
    assert coverage['glx-core']['discovery_only_reason']
    p = run([sys.executable, exo, '--root', root, 'plan', 'glx-core'])
    assert p.returncode == 0, p.stderr
    inline_plan = json.loads(p.stdout)
    assert 'glx-core' in inline_plan['discovery_only']
    assert not inline_plan['implementation_macros'] and not inline_plan['safe_export']
    with tempfile.TemporaryDirectory(prefix='contract-inline-', dir=t.work) as tmp:
        output = Path(tmp)/'no-archive'
        p = run([sys.executable, exo, '--root', root, 'build', 'glx-core',
                 '--output', output, '--exotract', Path(tmp)/'unused'])
        assert p.returncode != 0 and 'discovery-only MuLIB kernel' in p.stderr and not output.exists(), p

cdd = {'cdd-'+n for n in ['catalogue', 'registry', 'schema', 'query', 'plan', 'resolve', 'execute', 'diff']}
assert cdd <= modules.keys()
for name in cdd:
    entry = coverage[name]
    assert not entry['snapshot_profile'] and entry['semantic_sidecar'] is None and entry['archive_recipe'] is None
    assert entry['sidecar_header_matches'] is None
    assert entry['discovery_only_reason']
p = run([sys.executable, exo, '--root', root, 'plan', 'cdd-resolve', 'cdd-diff'])
assert p.returncode == 0, p.stderr
cdd_plan = json.loads(p.stdout)
assert cdd_plan['safe_export'] is False and cdd_plan['implementation_translation_units'] == 1
assert [m['name'] for m in cdd_plan['modules']] == ['cdd-catalogue', 'cdd-plan', 'cdd-resolve', 'cdd-diff']
assert set(cdd_plan['discovery_only']) == {'cdd-catalogue', 'cdd-plan', 'cdd-resolve', 'cdd-diff'}
assert cdd_plan['implementation_macros'] == ['MULIB_IMPL_CDD_CATALOGUE', 'MULIB_IMPL_CDD_PLAN',
                                             'MULIB_IMPL_CDD_RESOLVE', 'MULIB_IMPL_CDD_DIFF']
with tempfile.TemporaryDirectory(prefix='contract-discovery-only-', dir=t.work) as tmp:
    output = Path(tmp)/'no-archive'
    p = run([sys.executable, exo, '--root', root, 'build', 'vec', 'cdd-resolve',
             '--output', output, '--exotract', Path(tmp)/'unused'])
    assert p.returncode != 0 and 'discovery-only MuLIB kernel' in p.stderr and not output.exists(), p
p = run([sys.executable, tool, 'build-plan', 'cdd-catalogue'])
assert p.returncode != 0 and not p.stdout, p
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
assert elvi['link_libraries']==['m']
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
transport_plan = None
for route in [[tool, 'build-plan'], [exo, 'plan'], [t.root/'tools/on1x-mulib.py', 'plan']]:
    for requested in [['http3'], ['quic', 'http3']]:
        p = run([sys.executable, *route, *requested])
        assert p.returncode == 0, p.stderr
        live = json.loads(p.stdout)
        switches = live['implementation_macros']
        assert live['implementation_translation_units'] == 1 and not live['safe_export']
        assert live['compile_definitions'] == ['_POSIX_C_SOURCE=200809L']
        assert len(switches) == len(set(switches))
        assert {'MULIB_IMPL_QUIC', 'MULIB_IMPL_HTTP3', 'MULIB_IMPL_QUIC_CLIENT',
                'MULIB_IMPL_HTTP3_CLIENT', 'MULIB_IMPL_TLS', 'MULIB_IMPL_AES_GCM',
                'MULIB_IMPL_SOCKET'} <= set(switches)
        assert 'MULIB_IMPL_PARSER' not in switches
        assert not live.get('dependency_cycles') and not live.get('native_sources')
        if route[0] == tool and requested == ['http3']:
            transport_plan = live
if sys.platform.startswith('linux'):
    with tempfile.TemporaryDirectory(prefix='contract-http3-', dir=t.work) as tmp:
        work = Path(tmp)
        implementation = work/'impl.c'
        implementation.write_text(
            ''.join('#define '+d.replace('=', ' ', 1)+'\n' for d in transport_plan['compile_definitions'])+
            ''.join('#define '+m+'\n' for m in transport_plan['implementation_macros'])+
            ''.join('#include "'+str(root/m['header'])+'"\n' for m in transport_plan['modules']))
        (work/'mulib.h').write_text('#include "mulib-http3.h"\n')
        binary = work/'consumer'
        p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', *t.flags,
                 '-I', work, '-I', root/'kernels', implementation,
                 t.root/'contracts-exotract/examples/http3.c', '-o', binary])
        assert p.returncode == 0, p.stderr
        p = run([binary])
        assert p.returncode == 0 and p.stdout == 'quic + http3: archive OK\n', p
p = run([sys.executable, exo, 'list-kernels'])
assert p.returncode == 0, p.stderr
for name in ['quic', 'http3']:
    entry = json.loads(p.stdout)['modules'][name]
    assert entry['source_module'] == 'mulib.'+name and entry['source_status'] == 'eligible'
    assert entry['static_compile'] == dict(unsafe='unsupported', safe='unsupported')
with tempfile.TemporaryDirectory(prefix='contract-root-', dir=t.work) as tmp:
    fake = Path(tmp)
    (fake/'kernels').mkdir(); (fake/'manual').mkdir()
    for n in lint['profiles']:
        for directory, suffix in [('kernels', '.h'), ('manual', '.md')]:
            shutil.copyfile(root/directory/('mulib-'+n+suffix), fake/directory/('mulib-'+n+suffix))
    profiles = json.loads((t.root/'contracts-exoml/catalog.json').read_text())['profiles']
    evidence = {path:digest for profile in profiles.values() for path,digest in profile.get('evidence_sha256',{}).items()}
    for path in evidence:
        (fake/path).parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(root/path,fake/path)
    shutil.copyfile(root/'MANIFEST.yaml', fake/'MANIFEST.yaml')
    p = run([sys.executable, tool, '--root', fake, 'lint']); assert p.returncode == 0, p.stderr
    for path in ['kernels/mulib-vec.h','kernels/mulib-mune.h','manual/mulib-mune.md',
                 'kernels/mulib-elvi-scrape.h','manual/mulib-elvi-scrape.md',*evidence]:
        original = (fake/path).read_bytes(); (fake/path).write_bytes(original+b'\n')
        p = run([sys.executable, tool, '--root', fake, 'lint'])
        assert p.returncode == 3 and not p.stdout and 'fingerprint drift' in p.stderr, p.stderr
        (fake/path).write_bytes(original)

spec = importlib.util.spec_from_file_location('on1x_contracts', tool)
module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
token = r'[a-z][a-z0-9-]*'
for sequence in [' [one, two]', '\n  - one\n  - two', '\n    - one\n    - two']:
    assert module.manifest_list('  dependencies:'+sequence+'\n  status: implemented', 'dependencies', token) == ['one', 'two']
for sequence in ['', ' [one,]', ' [one]\n  - two', '\n  - one\n    - two',
                 '\n  - one\n  - one', '\n    item: value', '\n  - ../one',
                 '\n  - one\n  dependencies: [two]']:
    try: module.manifest_list('  dependencies:'+sequence+'\n  status: implemented', 'dependencies', token)
    except ValueError: pass
    else: raise AssertionError('malformed dependency list accepted: '+sequence)
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
p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', root/'kernels',
         t.root/'tests/fixtures/contract_semantics.c', *t.flags, '-lm', '-o', native])
assert p.returncode == 0, p.stderr
p = run([native])
assert p.returncode == 0 and p.stdout == 'Contract semantics: PASS\n', (p.returncode, p.stdout, p.stderr)
example = t.work/'contract_usage'
p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', root/'kernels',
         t.root/'examples/contracts/contract_usage.c', *t.flags, '-lm', '-o', example])
assert p.returncode == 0, p.stderr
p = run([example]); assert p.returncode == 0 and p.stdout == '[42]\n', (p.returncode, p.stdout, p.stderr)
# The Mune recipe enables the owned parser explicitly, without widening the
# standalone Parsec recipe. ABI declarations still come from the real headers.
implementation = t.work/'mune-implementation.c'
implementation.write_text(''.join('#define '+m+'\n' for m in mune['implementation_macros'])+
                          ''.join('#include "'+str(root/e['header'])+'"\n' for e in mune['modules']))
example = t.work/'mune-contract-usage'
p = run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I',root/'kernels',
         implementation,t.root/'contracts-exotract/examples/mune.c',*t.flags,'-pthread','-lm','-ldl','-o',example])
assert p.returncode == 0, p.stderr
p = run([example]); assert p.returncode == 0 and p.stdout == 'mune: archive OK\n',(p.returncode,p.stdout,p.stderr)
print('Contract source inventory, build-plan, transfer/borrow/cleanup/allocation semantics: PASS')
