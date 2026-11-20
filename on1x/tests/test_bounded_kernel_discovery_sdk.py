#!/usr/bin/env python3
"""Real compiler/ExoLib discovery for every new family; calls fail closed."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from support import run

exo, on1xc, root, work = (Path(p).resolve() for p in sys.argv[1:5])
clang = shutil.which('clang')
if not clang or not sys.platform.startswith('linux'):
    print('UNSUPPORTED: bounded discovery requires compatible clang resources and Linux')
    sys.exit(77)
cc = sys.argv[5]
resource = subprocess.check_output([clang, '-print-resource-dir'], text=True).strip()
names = ('media-timebase', 'nle-edit', 'img-blend', 'color-lut3d', 'nodegraph-compile',
         'mesh-primitives', 'geom-nurbs', 'anim-curve', 'physics-world', 'render-bvhbuild',
         'compute-cpu', 'audio-mixer', 'kernel-validate', 'asset-catalog', 'project-journal')
cmd = [sys.executable, root/'tools/on1x-build.py']
opts = ['--no-prelude', '--exotract', exo, '--on1xc', on1xc, '--cc', cc,
        '--resource-dir', resource, '--mulib-root', root.parent]
work.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as tmp:
    directory = Path(tmp)
    source = directory/'bounded.on1x'
    source.write_text(''.join(f'import kernel mulib.{n.replace("-", "_")} as K{i};\n'
                              for i, n in enumerate(names))+'fn main(){}\n')
    report = directory/'bounded.json'
    p = subprocess.run([str(a) for a in [*cmd, '--inspect-kernels', *opts, source, '-o', report]],
                       capture_output=True, text=True, timeout=300)
    assert p.returncode == 0, p
    data = json.loads(report.read_bytes())
    assert data['safe_export'] is False
    assert set(data['modules']) == {'mulib.'+n.replace('-', '_') for n in names}
    for name in names:
        module = data['modules']['mulib.'+name.replace('-', '_')]
        assert module['status'] == 'not_representable' and module['callable'] is False
        assert module['safe_export'] is False and module['contract'] is None and module['sidecar'] is None
        assert module['symbols'] and all(not s['callable'] and 'static-inline' in s['reason']
                                         for s in module['symbols'].values())
        exolib = module['exolib']
        assert exolib['parser'] == 'exoml/libclang' and exolib['externally_imported'] is False
        assert exolib['header_only']['implementation_translation_units'] == 1
        # A canonical type include can bring a mature dependency's switch into
        # the hypothetical one-TU recipe; none belongs to a static-inline module.
        assert all(not macro.startswith('MULIB_IMPL_'+name.split('-')[0].upper()+'_')
                   for macro in exolib['header_only']['implementation_defines'])
        graph = module['metadata']['cdecl']
        header = root.parent/'include'/('mulib-'+name+'.h')
        assert graph['complete'] is True and graph['safe_export'] is False
        assert graph['target']['triple'] == data['target']
        assert graph['revision'] == 'sha256:'+hashlib.sha256(header.read_bytes()).hexdigest()
        functions = [d for d in graph['declarations'] if d['kind'] == 'function']
        assert functions and all(d['source']['file'] == str(header) and d['definition']
                                 and d['inline'] and d['linkage'] == '2' for d in functions)
        assert all(not m['defined'] for m in graph['preprocessing']['final_macros']
                   if m['name'].startswith('MULIB_IMPL_'))
        assert all(i['safe'] is False for i in module['metadata']['imports'])
    for path, digest in data['inputs'].items():
        assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == digest, path

    denied = directory/'not-published'
    for prefix, code in (('', 'NOT-ADMITTED'), ('unsafe ', 'UNSAFE-UNSUPPORTED')):
        source.write_text(prefix+'import kernel mulib.media_timebase as T;fn main(){}\n')
        p = run([*cmd, '--kernel-bind', *opts, source, '-o', denied])
        assert p.returncode != 0 and 'ON1X-KERNEL-'+code in p.stderr and not denied.exists(), p
    source.write_text('import kernel mulib.media_unknown;fn main(){}\n')
    p = run([*cmd, '--inspect-kernels', *opts, source, '-o', denied])
    assert p.returncode != 0 and 'ON1X-KERNEL-PROFILE' in p.stderr and not denied.exists(), p
    assert {p.name for p in directory.iterdir()} == {'bounded.on1x', 'bounded.json'}
print('15 bounded kernel families: real internal-linkage CDecl discovery; safe/unsafe calls rejected: PASS')
