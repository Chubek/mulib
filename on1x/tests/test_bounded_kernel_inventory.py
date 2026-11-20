"""Registration is complete metadata, never automatic foreign admission."""
import importlib.util
from pathlib import Path
import re
import sys

from support import NativeTests, run

t = NativeTests()
root = t.root.parent.resolve()
spec = importlib.util.spec_from_file_location('bounded_house', t.root/'stdlib/mulib/housekeeping.py')
house = importlib.util.module_from_spec(spec)
spec.loader.exec_module(house)
contracts = house.contracts
spec = importlib.util.spec_from_file_location('bounded_exo', t.root/'tools/on1x-exotract.py')
exo = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exo)
inventory = contracts.inventory(root)
modules = inventory['modules']
aliases = exo.manifest_aliases(modules)
sys.path.insert(0, str(root/'tools'))
from media_inventory import inventory as media_inventory

rows = [record['header'][6:-2] for record in media_inventory(root)]
assert len(rows) == len(set(rows)) == 194
assert set(rows) <= modules.keys()
assert set(inventory['auxiliary_headers']) == {
    'include/mulib-container-core.h', 'include/mulib-qvm.h', 'include/mulib-lprowse.h',
}
for name in rows:
    module = modules[name]
    assert module['macro'] is None and module['implementation'] == 'static-inline'
    coverage = inventory['coverage'][name]
    assert coverage['semantic_sidecar'] is None and coverage['archive_recipe'] is None
    assert coverage['discovery_only_reason'] and not coverage['snapshot_profile']
    includes = set(re.findall(r'^#include "mulib-([^"]+)\.h"', (root/module['header']).read_text(), re.M))
    assert set(module['manifest_dependencies']) == includes, name
    assert (root/module['manual']).is_file()
    assert exo.manifest_discovery_name(name.replace('-', '_'), aliases) == name

assert exo.manifest_discovery_name('media_unknown', aliases) is None
assert exo.manifest_discovery_name('vec', aliases) == 'vec'
for canonical, collision in (('media-core', 'media_core'), ('b+tree', 'b_plus_tree'),
                             ('cowb+tree', 'cowb_plus_tree'), ('2dgeom', 'k_2dgeom')):
    assert exo.manifest_discovery_name(collision, aliases) == canonical
    ambiguous = exo.manifest_aliases([canonical, collision])
    try:
        exo.manifest_discovery_name(collision, ambiguous)
    except ValueError as error:
        assert 'ambiguous manifest alias' in str(error)
    else:
        raise AssertionError('ambiguous manifest alias was silently resolved')

recipe = house.plan(root, rows, discovery=True)
assert set(rows) <= set(recipe['discovery_only'])
assert recipe['implementation_macros'] == [m['macro'] for m in recipe['modules'] if m['macro']]
assert len(recipe['implementation_macros']) == len(set(recipe['implementation_macros']))
assert not recipe['native_sources']
assert recipe['implementation_translation_units'] == 1 and recipe['safe_export'] is False
assert set(rows) <= {m['name'] for m in recipe['modules']}
for path, digest in recipe['inputs'].items():
    assert contracts.digest(Path(path)) == digest
try:
    house.plan(root, rows)
except ValueError as error:
    assert 'discovery-only MuLIB kernel' in str(error)
else:
    raise AssertionError('static-inline-only archive plan was admitted')

for name in ('media-timebase', 'compute-vulkan', 'kernel-validate'):
    output = t.work/('denied-'+name)
    p = run([sys.executable, t.root/'tools/on1x-exotract.py', 'build', name,
             '--output', output, '--exotract', t.work/'unused-sdk'])
    assert p.returncode != 0 and 'discovery-only MuLIB kernel' in p.stderr and not output.exists(), p
print('193 supplied kernels plus shared audio core registered; only three intentional auxiliaries: PASS')
