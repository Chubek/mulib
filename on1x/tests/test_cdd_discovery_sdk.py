#!/usr/bin/env python3
"""CDD registration exposes real CDecl metadata, not callable bindings."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from support import run

exo, on1xc, root, work = map(lambda p: Path(p).resolve(), sys.argv[1:5])
clang = shutil.which('clang')
if not clang or not sys.platform.startswith('linux'):
    print('UNSUPPORTED: CDD discovery requires compatible clang resources and Linux')
    sys.exit(77)
resource = subprocess.check_output([clang, '-print-resource-dir'], text=True).strip()
cmd = [sys.executable, root/'tools/on1x-build.py']
opts = ['--no-prelude', '--exotract', exo, '--on1xc', on1xc, '--cc', sys.argv[5],
        '--resource-dir', resource, '--mulib-root', root.parent]
work.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as tmp:
    directory = Path(tmp)
    cdd = ['catalogue', 'registry', 'schema', 'query', 'plan', 'resolve', 'execute', 'diff']
    source = directory/'cdd.on1x'
    source.write_text(''.join(f'import kernel mulib.cdd_{n} as Cdd_{n};\n' for n in cdd) + 'fn main(){}')
    report = directory/'cdd.json'
    p = run([*cmd, '--inspect-kernels', *opts, source, '-o', report])
    assert p.returncode == 0, p
    data = json.loads(report.read_bytes())
    assert data['safe_export'] is False
    assert set(data['modules']) == {'mulib.cdd_'+n for n in cdd}
    for logical, module in data['modules'].items():
        name = logical.removeprefix('mulib.').replace('_', '-')
        assert module['exolib']['name'] == name and module['exolib']['parser'] == 'exoml/libclang'
        assert module['callable'] is False and module['safe_export'] is False and module['status'] == 'not_representable'
        assert module['contract'] is None and module['sidecar'] is None
        assert module['symbols'] and all(s['callable'] is False and 'checked accessors' in s['reason']
                                         for s in module['symbols'].values())
        graph = module['metadata']['cdecl']
        assert graph['header'] == str(root.parent/'include'/('mulib-'+name+'.h'))
        assert graph['complete'] is True and graph['safe_export'] is False and graph['target']['triple'] == data['target']
        assert all(not m['defined'] for m in graph['preprocessing']['final_macros'] if m['name'].startswith('MULIB_IMPL_'))
        assert str(root/'contracts-exotract/catalog.json') in {p['path'] for p in graph['inputs']}
        assert all(entry['safe'] is False for entry in module['metadata']['imports'])
    assert data['modules']['mulib.cdd_resolve']['exolib']['header_only']['implementation_defines'] == [
        'MULIB_IMPL_CDD_CATALOGUE', 'MULIB_IMPL_CDD_PLAN', 'MULIB_IMPL_CDD_RESOLVE']
    denied = directory/'not-published'
    source.write_text('import kernel mulib.cdd_unknown;fn main(){}')
    p = run([*cmd, '--inspect-kernels', *opts, source, '-o', denied])
    assert p.returncode != 0 and 'ON1X-KERNEL-PROFILE' in p.stderr and str(source)+':1:1:' in p.stderr, p
    source.write_text('import kernel mulib.cdd_catalogue;fn main(){}')
    p = run([*cmd, '--inspect-kernels', '--kernel-review-candidate', *opts, source, '-o', denied])
    assert p.returncode != 0 and 'ON1X-KERNEL-NOT-ADMITTED' in p.stderr and not denied.exists(), p
    for prefix, code in [('', 'NOT-ADMITTED'), ('unsafe ', 'UNSAFE-UNSUPPORTED')]:
        source.write_text(f'{prefix}import kernel mulib.cdd_catalogue;fn main(){{}}')
        p = run([*cmd, '--kernel-bind', *opts, source, '-o', denied])
        assert p.returncode != 0 and f'ON1X-KERNEL-{code}' in p.stderr and not denied.exists(), p
    assert {p.name for p in directory.iterdir()} == {'cdd.on1x', 'cdd.json'}
print('CDD canonical ExoLib/CDecl discovery and safe/unsafe admission rejection: PASS')
