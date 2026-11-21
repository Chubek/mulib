#!/usr/bin/env python3
"""Automatic registration drives real CDecl discovery, never admission."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from support import run

exotract, on1xc, on1x, work = (Path(p).resolve() for p in sys.argv[1:5])
cc = sys.argv[5]
clang = shutil.which('clang')
if not clang or not sys.platform.startswith('linux'):
    print('UNSUPPORTED: automatic KernelBind discovery requires clang resources and Linux')
    sys.exit(77)
resource = subprocess.check_output([clang, '-print-resource-dir'], text=True).strip()
opts = ['--no-prelude', '--exotract', exotract, '--on1xc', on1xc, '--cc', cc, '--resource-dir', resource]
work.mkdir(parents=True, exist_ok=True)


def check_report(data, catalog, manifest):
    assert data['schema'] == 'on1x.kernel-discovery/v1' and data['safe_export'] is False
    for path in (catalog, manifest):
        assert data['inputs'][str(path)] == hashlib.sha256(path.read_bytes()).hexdigest()
    for module in data['modules'].values():
        assert module['status'] == 'not_representable'
        assert module['safe_export'] is False and module['callable'] is False
        assert module['contract'] is None and module['sidecar'] is None
        assert module['exolib']['parser'] == 'exoml/libclang'
        assert module['exolib']['header_only']['implementation_defines'] == []
        graph = module['metadata']['cdecl']
        assert graph['complete'] is True and graph['safe_export'] is False
        assert graph['target']['triple'] == data['target']
        assert {str(catalog), str(manifest)} <= {row['path'] for row in graph['inputs']}
        assert graph['preprocessing']['final_snapshot_complete'] is True
        assert not any(m['defined'] and m['name'].startswith('MULIB_IMPL_')
                       for m in graph['preprocessing']['final_macros'])
        assert all(row['safe'] is False for row in module['metadata']['imports'])
        assert all(row['callable'] is False for row in module['symbols'].values())


with tempfile.TemporaryDirectory(dir=work) as tmp:
    directory = Path(tmp)
    source = directory/'probe.on1x'
    report = directory/'probe.json'
    command = [sys.executable, on1x/'tools/on1x-build.py', *opts, '--mulib-root', on1x.parent]
    names = {'vec': 'vec', 'parsec': 'parsec', 'yantlr': 'yantlr', 'ed25519': 'ed25519',
              'k_2dgeom': '2dgeom', 'b_plus_tree': 'b+tree', 'cowb_plus_tree': 'cowb+tree',
              'container_core': 'container-core', 'qvm': 'qvm', 'lprowse': 'lprowse'}
    source.write_text(''.join(f'import kernel mulib.{name} as K{i};\n' for i, name in enumerate(names))
                      +'fn main(){}\n')
    result = run([*command, '--inspect-kernels', source, '-o', report])
    assert result.returncode == 0, result
    data = json.loads(report.read_bytes())
    check_report(data, on1x/'contracts-exotract/kernel-auto.json', on1x.parent/'MANIFEST.yaml')
    assert set(data['modules']) == {'mulib.'+name for name in names}
    for name, canonical in names.items():
        discovered = data['modules']['mulib.'+name]
        assert discovered['exolib']['name'] == canonical
        if canonical != 'lprowse':
            assert discovered['symbols']
        assert Path(discovered['metadata']['cdecl']['header']).name == f'mulib-{canonical}.h'
    # Inline functions and embedded data are real declarations, never fabricated
    # external archive symbols or an automatically callable native namespace.
    for name, symbol in (('container_core', 'mulib_ct_add_size'), ('qvm', 'mulib_qvm_validate')):
        graph = data['modules']['mulib.'+name]['metadata']['cdecl']
        declaration = next(d for d in graph['declarations'] if d['name'] == symbol)
        assert declaration['kind'] == 'function' and declaration['definition']
        assert declaration['linkage'] == '2'
    qvm_inputs = data['modules']['mulib.qvm']['metadata']['cdecl']['inputs']
    assert str(on1x.parent/'include/mulib-container-core.h') in {p['path'] for p in qvm_inputs}
    embedded = data['modules']['mulib.lprowse']
    assert embedded['automatic_registration']['implementation'] == 'static-data'
    assert 'macro' not in embedded['automatic_registration']
    assert not embedded['symbols']
    assert any(d['name'] == 'mulib_lprowse_source_' for d in embedded['metadata']['cdecl']['declarations'])
    for name in ('container_core', 'qvm', 'lprowse'):
        for unsafe, diagnostic in (('', 'ON1X-KERNEL-NOT-ADMITTED'),
                                   ('unsafe ', 'ON1X-KERNEL-UNSAFE-UNSUPPORTED')):
            source.write_text(f'{unsafe}import kernel mulib.{name} as K;\nfn main(){{}}\n')
            rejected = directory/f'{name}-{bool(unsafe)}'
            result = run([*command, '--kernel-bind', source, '-o', rejected])
            assert result.returncode != 0 and diagnostic in result.stderr, result
            assert not rejected.exists()
    ed25519 = data['modules']['mulib.ed25519']
    assert ed25519['automatic_registration']['optional_dependency'].startswith('OpenSSL Crypto')
    assert not any('openssl/' in row['path'] for row in ed25519['metadata']['cdecl']['inputs'])
    assert not any('library/yantlr/' in row['path']
                   for row in data['modules']['mulib.yantlr']['metadata']['cdecl']['inputs'])

    # Run the actual generator and build helper on a minimal future checkout.
    # No handwritten registration, fixture, archive or sidecar is copied in.
    checkout = directory/'future'
    tools_home = checkout/'on1x'
    for subdir, names in (
        ('tools', ('on1x-build.py', 'on1x-exotract.py', 'on1x-contracts.py')),
        ('runtime', ('on1x_runtime.c', 'on1x_runtime.h')),
        ('stdlib/mulib', ('housekeeping.py',)),
        ('contracts-exotract', ('catalog.json', 'kernel-discovery-v1.lua')),
    ):
        (tools_home/subdir).mkdir(parents=True)
        for name in names:
            shutil.copyfile(on1x/subdir/name, tools_home/subdir/name)
    (checkout/'include').mkdir()
    (checkout/'manual').mkdir()
    manifest = checkout/'MANIFEST.yaml'
    manifest.write_text('name: MuLIB\nmodules:\n')
    catalog = tools_home/'contracts-exotract/kernel-auto.json'
    command = [sys.executable, tools_home/'tools/on1x-build.py', *opts, '--mulib-root', checkout]
    denied = directory/'denied.json'
    generate = [sys.executable, on1x.parent/'tools/extend-kernelbind.py', '--root', checkout]

    def register(name, header, *, dependencies=(), bounded=False, optional=None):
        (checkout/'include'/f'mulib-{name}.h').write_text(header)
        (checkout/'manual'/f'mulib-{name}.md').write_text('# Test declarations\n')
        implementation = ('implementation: static-inline\n  status: bounded' if bounded else
                          'macro: MULIB_IMPL_'+name.upper().replace('-', '_')+'\n  status: implemented')
        with manifest.open('a') as out:
            out.write(f'- name: {name}\n  header: include/mulib-{name}.h\n'
                      f'  manual: manual/mulib-{name}.md\n  {implementation}\n'
                      f'  dependencies: [{", ".join(dependencies)}]\n')
            if optional:
                out.write(f'  optional_dependency: {optional}\n')

    register('future-types', 'typedef struct future_point { int x; } future_point;\n')
    register('future-inline', 'static inline int future_zero(void) { return 0; }\n', bounded=True)
    register('future-kernel',
             '#include "mulib-future-types.h"\n'
             '#ifdef ENABLE_FUTURE_PROVIDER\n#include "future-provider-not-installed.h"\n#endif\n'
             '#ifdef MULIB_IMPL_FUTURE_KERNEL\n#error implementation must not be compiled\n#endif\n'
             'int future_read(const future_point *point);\n'
             'static inline int future_inline_value(void) { return 3; }\n',
             dependencies=('future-types',), optional='Future Provider SDK')
    with manifest.open('a') as out:
        out.write('- name: future-reserved\n  header: include/mulib-future-reserved.h\n'
                  '  status: reserved\n  dependencies: []\n')
    (checkout/'include/mulib-auxiliary.h').write_text('int not_registered(void);\n')
    source.write_text('import kernel mulib.future_kernel as F;\n'
                      'import kernel mulib.future_types as T;\n'
                      'import kernel mulib.future_inline as I;\nfn main(){}\n')
    result = run(generate)
    assert result.returncode != 0 and 'unregistered kernel header' in result.stderr, result
    assert not catalog.exists(), 'incomplete header coverage published a catalog'
    (checkout/'include/mulib-auxiliary.h').unlink()
    result = run(generate)
    assert result.returncode == 0, result
    future = directory/'future.json'
    result = run([*command, '--inspect-kernels', source, '-o', future])
    assert result.returncode == 0, result
    data = json.loads(future.read_bytes())
    check_report(data, catalog, manifest)
    modules = data['modules']
    assert modules['mulib.future_types']['symbols'] == {}
    assert any(d['name'] == 'future_point' and d['kind'] == 'typedef'
               for d in modules['mulib.future_types']['metadata']['cdecl']['declarations'])
    graph = modules['mulib.future_kernel']['metadata']['cdecl']
    functions = {d['name']: d for d in graph['declarations'] if d['kind'] == 'function'}
    assert functions['future_read']['linkage'] == '4' and not functions['future_read']['definition']
    assert functions['future_inline_value']['linkage'] == '2' and functions['future_inline_value']['definition']
    assert modules['mulib.future_inline']['automatic_registration']['implementation'] == 'static-inline'
    assert modules['mulib.future_kernel']['automatic_registration']['optional_dependency'] == 'Future Provider SDK'
    assert str(checkout/'include/mulib-future-types.h') in {row['path'] for row in graph['inputs']}

    def fail(mode, text, expected, *extra):
        source.write_text(text+'fn main(){}\n')
        result = run([*command, mode, source, '-o', denied, *extra])
        assert result.returncode != 0 and not denied.exists(), result
        assert all(value in result.stderr for value in expected), result
        assert not list(directory.glob('.kernel-discovery-*'))

    future_import = 'import kernel mulib.future_kernel;\n'
    fail('--inspect-kernels', future_import,
         ('ON1X-KERNEL-DISCOVERY', 'UNSUPPORTED', 'Future Provider SDK', 'future-provider-not-installed.h'),
         '--cflag=-DENABLE_FUTURE_PROVIDER')
    fail('--inspect-kernels', future_import, ('implementation macros',), '--cflag=-DMULIB_IMPL_FUTURE_KERNEL')
    fail('--kernel-bind', future_import, ('ON1X-KERNEL-NOT-ADMITTED',))
    fail('--kernel-bind', 'unsafe '+future_import, ('ON1X-KERNEL-UNSAFE-UNSUPPORTED',))
    fail('--kernel-review-candidate', future_import, ('ON1X-KERNEL-NOT-ADMITTED',))
    fail('--inspect-kernels', 'import kernel mulib.auxiliary;\n', ('ON1X-KERNEL-PROFILE', 'no discovery profile'))
    fail('--inspect-kernels', 'import kernel mulib.future_reserved;\n', ('ON1X-KERNEL-PROFILE', 'no discovery profile'))
    shadow = directory/'shadow'
    shadow.mkdir()
    (shadow/'mulib-future-types.h').symlink_to(checkout/'include/mulib-future-types.h')
    fail('--inspect-kernels', future_import, ('ambiguous MuLIB kernel header',), '--mulib-path', shadow)

    valid = catalog.read_bytes()
    wrong = json.loads(valid)
    wrong['modules']['future-kernel']['dependencies'] = []
    catalog.write_text(json.dumps(wrong))
    fail('--inspect-kernels', future_import, ('ON1X-KERNEL-PROFILE', 'catalog/MANIFEST entry drift'))
    catalog.write_bytes(valid.replace(b'"safe_export": false', b'"safe_export": true'))
    fail('--inspect-kernels', future_import, ('unsupported/unsafe automatic kernel catalog',))
    catalog.write_bytes(valid)

    # Simulate a catalog edit after the driver has validated and pinned it,
    # but before the real SDK reads its provenance files.
    mutate_sdk = directory/'mutate-catalog'
    mutate_sdk.write_text(f'#!{sys.executable}\n'
                          'import os, sys\n'
                          f'with open({str(catalog)!r}, "ab") as out: out.write(b"\\n")\n'
                          f'os.execv({str(exotract)!r}, [{str(exotract)!r}, *sys.argv[1:]])\n')
    mutate_sdk.chmod(0o755)
    fail('--inspect-kernels', future_import, ('KernelBind discovery input drift', 'kernel-auto.json'),
         '--exotract', mutate_sdk)
    catalog.write_bytes(valid)

    register('future-new', 'int another_future(void);\n')
    fail('--inspect-kernels', 'import kernel mulib.future_new;\n',
         ('ON1X-KERNEL-PROFILE', 'catalog/MANIFEST registration drift'))
    result = run(generate)
    assert result.returncode == 0, result
    register('future_kernel', 'int no_silent_selection(void);\n')
    # Give the colliding registration a different switch so generation can succeed.
    manifest.write_text(manifest.read_text().replace(
        '  macro: MULIB_IMPL_FUTURE_KERNEL\n  status: implemented\n  dependencies: []',
        '  macro: MULIB_IMPL_COLLIDING_FUTURE\n  status: implemented\n  dependencies: []'))
    result = run(generate)
    assert result.returncode == 0, result
    fail('--inspect-kernels', future_import, ('ambiguous manifest alias', 'future-kernel, future_kernel'))
    register('ipc-framed', 'int handwritten_profile(void);\n')
    register('ipc_framed', 'int no_silent_handwritten_selection(void);\n')
    fail('--inspect-kernels', 'import kernel mulib.ipc_framed;\n',
         ('ambiguous manifest alias', 'ipc-framed, ipc_framed'))

print('Automatic KernelBind discovery: present and future CDecl metadata, no callable admission: PASS')
