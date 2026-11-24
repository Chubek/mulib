"""Every canonical header reaches registry, planner and native request discovery.

The filesystem is the independent coverage oracle: adding a header without
registering it must fail, even when all generated catalogs agree with each other.
Actual SDK extraction for the newly registered shapes lives in
test_kernel_auto_discovery_sdk.py.
"""
import importlib.util
import json
import sys

from support import NativeTests, run

t = NativeTests()
root = t.root.parent.resolve()


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


house = load('registration_house', t.root/'stdlib/mulib/housekeeping.py')
exo = load('registration_exo', t.root/'tools/on1x-exotract.py')
contracts = house.contracts
headers = {p.relative_to(root).as_posix() for p in (root/'include').glob('mulib-*.h')}
assert headers
inventory = contracts.inventory(root)
modules = inventory['modules']
reservations = set(inventory['reserved_headers'])
assert reservations <= headers
headers -= reservations
assert {m['header'] for m in modules.values()} == headers
assert len(modules) == len(headers), 'duplicate canonical header registration'
assert not inventory['auxiliary_headers']
assert set(inventory['coverage']) == set(modules)
assert inventory['safe_export'] is False
manifest = house.inventory(root, registration=True)
assert {m['header'] for m in manifest.values()} == headers
automatic, _ = contracts.kernel_auto_catalog(manifest)
assert set(automatic) == set(modules)
assert {m['header'] for m in automatic.values()} == headers
for tool in ('on1x-contracts.py', 'on1x-exotract.py'):
    result = run([sys.executable, t.root/'tools'/tool, '--root', root, 'list'])
    assert result.returncode == 0, result.stderr
    listed = json.loads(result.stdout)
    assert listed['modules'] == modules
    assert set(listed['reserved_headers']) == reservations
    assert not listed['auxiliary_headers'] and listed['safe_export'] is False

# The shared archive/discovery planner must retain every selected header and
# hash its actual source, including internal-linkage dependency-only modules.
plan = house.plan(root, sorted(modules), discovery=True)
assert {m['header'] for m in plan['modules']} == headers
assert len(plan['implementation_macros']) == len(set(plan['implementation_macros']))
assert plan['implementation_translation_units'] == 1 and plan['safe_export'] is False
for header in headers:
    assert plan['inputs'][str(root/header)] == contracts.digest(root/header)

# Every source spelling must resolve unambiguously, including punctuation and
# leading-digit names. Chunk at the documented 1024 native request budget.
aliases = exo.manifest_aliases(modules)
assert len(aliases) == len(modules)
names = sorted(aliases)
for logical in names:
    assert len(aliases[logical]) == 1
    assert exo.manifest_discovery_name(logical, aliases) == aliases[logical][0]
for start in range(0, len(names), 512):
    selected = names[start:start+512]
    source = t.source(f'all_headers_{start}', ''.join(
        f'import kernel mulib.{name} as K{i};\n' for i, name in enumerate(selected))+'fn main(){}\n')
    result = run([t.compiler, '--no-prelude', '--kernel-imports', source])
    assert result.returncode == 0, result.stderr
    requests = json.loads(result.stdout)['requests']
    assert [r['module'] for r in requests] == ['mulib.'+n for n in selected]
    assert all(not r['unsafe'] for r in requests)

new = ('container-core', 'lprowse', 'qvm')
for name in new:
    assert modules[name]['macro'] is None
    assert inventory['coverage'][name]['archive_recipe'] is None
    assert inventory['coverage'][name]['discovery_only_reason']
    plan = house.plan(root, [name], discovery=True)
    assert name in plan['discovery_only'] and not plan['implementation_macros']
    assert not plan['native_sources'] and plan['safe_export'] is False
    assert plan['implementation_translation_units'] == 1
    assert str(root/modules[name]['header']) in plan['inputs']
    if name == 'qvm':
        assert {m['name'] for m in plan['modules']} == {'container-core', 'qvm'}
    for tool, action in (('on1x-contracts.py', 'build-plan'), ('on1x-exotract.py', 'build')):
        output = t.work/f'unpublished-{name}'
        extra = ['--output', output, '--exotract', t.work/'unused-sdk'] if action == 'build' else []
        result = run([sys.executable, t.root/'tools'/tool, action, name, *extra])
        assert result.returncode != 0 and not output.exists(), result
        diagnostic = 'unknown/duplicate requested library' if action == 'build-plan' else 'discovery-only'
        assert diagnostic in result.stderr, result
    logical = name.replace('-', '_')
    for unsafe, diagnostic in (('', 'ON1X-KERNEL-NOT-ADMITTED'),
                                ('unsafe ', 'ON1X-KERNEL-UNSAFE-UNSUPPORTED')):
        if name == 'container-core' and unsafe:
            # The audited inline member does not imply an archive ABI or
            # authorization for other members of the same module.
            source = t.source(name+'-inline',
                              f'{unsafe}import kernel mulib.{logical} as K;\n'
                              'fn main(){ unsafe { println(K.counter_add(2u64, 40u64)); } }\n')
            result = run([t.compiler, '--no-prelude', '--emit-c', '--kernel-c11', source])
            assert result.returncode == 0 and 'mulib_ct_counter_add' in result.stdout, result
            continue
        source = t.source(name+'-'+str(bool(unsafe)),
                          f'{unsafe}import kernel mulib.{logical} as K;\nfn main(){{}}\n')
        result = run([t.compiler, '--no-prelude', '--emit-c', '--kernel-c11', source])
        assert result.returncode != 0 and diagnostic in result.stderr and not result.stdout, result

# Internal-linkage dependencies can supply definitions to a switched consumer
# without acquiring a standalone archive ABI. Explicit selection still rejects,
# even when the exact same module is already present transitively.
for consumer, dependencies in (('container', {'container-core'}),
                               ('container-qvmboost', {'container-core', 'qvm'}),
                               ('prowse-control', {'lprowse'})):
    plan = house.plan(root, [consumer])
    assert dependencies <= {m['name'] for m in plan['modules']}
    assert dependencies <= plan['discovery_only'].keys()
    assert plan['implementation_translation_units'] == 1 and plan['safe_export'] is False
    for dependency in dependencies:
        assert modules[dependency]['macro'] is None
        try:
            house.plan(root, [consumer, dependency])
        except ValueError as error:
            assert 'discovery-only MuLIB kernel '+dependency in str(error), error
        else:
            raise AssertionError('explicit internal-linkage request acquired archive admission')
    if consumer == 'prowse-control':
        assert {'dl', 'm', 'pthread'} <= set(plan['link_libraries'])
        assert 'tools/embed_lprowse.py' in plan['generator_checks']
        assert any(f['path'] == 'library/lprowse/lprowse.lua' for f in plan['files'])
        assert next(f for f in plan['fixtures'] if f['name'] == 'lprowse')['kernels'] == [
            'prowse-control', 'lprowse']
        fixture = house.plan(root, [], ['lprowse'])
        assert {m['name'] for m in fixture['modules']} == {m['name'] for m in plan['modules']}
        assert fixture['implementation_macros'] == plan['implementation_macros']
        assert fixture['link_libraries'] == plan['link_libraries']

print(f'{len(headers)} canonical headers: full registry/native request coverage and bounded inline selection: PASS')
