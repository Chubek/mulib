#!/usr/bin/env python3
"""The whole static source registry reaches the real scanner, not automatic call admission."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from support import run


def checked(argv):
    result = run(argv)
    assert result.returncode == 0, result
    return result.stdout


def listing(on1x, root):
    return [sys.executable, on1x/'tools/on1x-exotract.py', '--root', root, 'list-kernels']


def driver(on1x, root, opts):
    return [sys.executable, on1x/'tools/on1x-build.py', '--mulib-root', root, *opts]


def catalog(on1x):
    return on1x/'contracts-exotract/kernel-auto.json'


def check_registry(on1x, root):
    data = json.loads(checked(listing(on1x, root)))
    assert data['schema'] == 'on1x.static-kernel-registry/v1' and data['safe_export'] is False
    for path in (catalog(on1x), root/'MANIFEST.yaml', on1x/'contracts-exotract/catalog.json'):
        assert data['inputs'][str(path)] == hashlib.sha256(path.read_bytes()).hexdigest()
    modules = data['modules']
    assert {name: row['declaration'] for name, row in modules.items()} == json.loads(
        catalog(on1x).read_bytes())['modules']
    return modules


def check_scanner(modules, on1xc, source):
    assert all(row['source_status'] == 'eligible' and row['source_candidates'] == [name]
               for name, row in modules.items())
    names = sorted(row['source_module'] for row in modules.values())
    assert names and len(names) == len(set(names))
    for start in range(0, len(names), 512):
        batch = names[start:start + 512]
        source.write_text(''.join(f'import kernel {name} as K{i};\n' for i, name in enumerate(batch))
                          +'fn main(){}\n')
        scanned = json.loads(checked([on1xc, '--no-prelude', '--kernel-imports', source]))
        assert scanned['schema'] == 'on1x.kernel-requests/v1'
        assert [(r['module'], r['alias']) for r in scanned['requests']] == [
            (name, f'K{i}') for i, name in enumerate(batch)]
        assert all(not r['unsafe'] and 'linkage' not in r for r in scanned['requests'])


def check_inspection(driver_args, source, report, registration):
    logical = registration['source_module']
    source.write_text(f'import kernel {logical} as K;\nfn main(){{}}\n')
    checked([*driver_args, '--inspect-kernels', source, '-o', report])
    data = json.loads(report.read_bytes())
    assert data['schema'] == 'on1x.kernel-discovery/v1' and data['safe_export'] is False
    assert set(data['modules']) == {logical}
    entry = data['modules'][logical]
    assert entry['automatic_registration'] == registration['declaration']
    assert entry['status'] == 'not_representable'
    assert entry['safe_export'] is False and entry['callable'] is False
    assert entry['contract'] is None and entry['sidecar'] is None
    assert entry['exolib']['parser'] == 'exoml/libclang'
    graph = entry['metadata']['cdecl']
    assert graph['complete'] is True and graph['safe_export'] is False
    assert graph['target']['triple'] == data['target']
    assert graph['header'].endswith('/'+registration['declaration']['header'])
    return entry


def denied(argv, *messages):
    result = run(argv)
    assert result.returncode != 0 and not result.stdout, result
    assert all(message in result.stderr for message in messages), result


def main():
    exotract, on1xc, on1x, work = (Path(p).resolve() for p in sys.argv[1:5])
    cc = sys.argv[5]
    clang = shutil.which('clang')
    if not clang or not sys.platform.startswith('linux'):
        print('UNSUPPORTED: static KernelBind inspection requires Linux and Clang resources')
        return 77
    resource = subprocess.check_output([clang, '-print-resource-dir'], text=True).strip()
    opts = ['--no-prelude', '--exotract', exotract, '--on1xc', on1xc, '--cc', cc, '--resource-dir', resource]
    root = on1x.parent
    modules = check_registry(on1x, root)
    inventory = json.loads(checked([sys.executable, on1x/'tools/on1x-contracts.py',
                                    '--root', root, 'list']))
    headers = {row['declaration']['header'] for row in modules.values()}
    reserved = set(inventory['reserved_headers'])
    assert not headers & reserved
    assert headers | reserved == {p.relative_to(root).as_posix() for p in (root/'include').glob('mulib-*.h')}
    assert {name for name, row in modules.items() if row['static_compile']['safe'] == 'requires_review'} == {
        'clamp', 'histprice', 'priceopt'}
    assert {name for name, row in modules.items() if row['static_compile']['unsafe'] == 'requires_checked_adapter'} == {
        'clamp', 'ipc-framed'}
    assert modules['clamp']['discovery_profile'] == 'handwritten'
    assert modules['ipc-framed']['discovery_profile'] == 'sandbox'
    assert modules['cdd-catalogue']['discovery_profile'] == 'automatic-legacy'
    assert modules['lprowse']['discovery_profile'] == 'automatic'
    assert modules['lprowse']['discovery_only_reason']
    assert modules['b+tree']['source_module'] == 'mulib.b_plus_tree'
    assert modules['2dgeom']['source_module'] == 'mulib.k_2dgeom'

    work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=work) as tmp:
        directory = Path(tmp)
        source = directory/'probe.on1x'
        check_scanner(modules, on1xc, source)
        real = check_inspection(driver(on1x, root, opts), source, directory/'real.json', modules['qvm'])
        assert real['symbols']['mulib_qvm_validate']['callable'] is False

        # Register a new, otherwise unprofiled C11 kernel with the real generator.
        # No copied registry, adapter, review or generated declaration is used.
        checkout = directory/'checkout'
        home = checkout/'on1x'
        for subdir, names in (
            ('tools', ('on1x-build.py', 'on1x_optimization.py', 'on1x-exotract.py', 'on1x-contracts.py')),
            ('stdlib/mulib', ('housekeeping.py',)),
            ('runtime', ('on1x_runtime.c', 'on1x_runtime.h', 'on1x_bounds.h')),
            ('contracts-exotract', ('catalog.json', 'kernel-discovery-v1.lua')),
        ):
            (home/subdir).mkdir(parents=True)
            for name in names:
                shutil.copyfile(on1x/subdir/name, home/subdir/name)
        (checkout/'include').mkdir()
        (checkout/'manual').mkdir()
        manifest = checkout/'MANIFEST.yaml'
        manifest.write_text('name: MuLIB\nmodules:\n')

        def register(name, macro):
            (checkout/'include'/f'mulib-{name}.h').write_text(
                f'int mulib_future_value(void);\n#ifdef {macro}\n'
                'int mulib_future_value(void) { return 42; }\n#endif\n')
            (checkout/'manual'/f'mulib-{name}.md').write_text('# Future C11 declarations\n')
            with manifest.open('a') as stream:
                stream.write(f'- name: {name}\n  header: include/mulib-{name}.h\n'
                             f'  manual: manual/mulib-{name}.md\n  status: implemented\n'
                             f'  macro: {macro}\n  dependencies: []\n')

        name = '7+round-test'
        logical = 'mulib.k_7_plus_round_test'
        register(name, 'MULIB_IMPL_FUTURE_VALUE')
        denied(listing(home, checkout), 'kernel-auto.json')
        generate = [sys.executable, root/'tools/extend-kernelbind.py', '--root', checkout]
        checked(generate)
        future_modules = check_registry(home, checkout)
        check_scanner(future_modules, on1xc, source)
        future = future_modules[name]
        assert future['source_module'] == logical and future['discovery_profile'] == 'automatic'
        assert future['static_compile'] == {'safe': 'unsupported', 'unsafe': 'unsupported'}
        command = driver(home, checkout, opts)
        entry = check_inspection(command, source, directory/'future.json', future)
        declarations = entry['metadata']['cdecl']['declarations']
        function, = (d for d in declarations if d['name'] == 'mulib_future_value')
        assert function['kind'] == 'function' and not function['definition'] and function['linkage'] == '4'
        assert not entry['exolib']['header_only']['implementation_defines']
        for prefix, diagnostic in (('', 'ON1X-KERNEL-NOT-ADMITTED'),
                                   ('unsafe ', 'ON1X-KERNEL-UNSAFE-UNSUPPORTED')):
            source.write_text(f'{prefix}import kernel {logical} as K;\nfn main(){{}}\n')
            output = directory/'denied'
            denied([*command, '--kernel-bind', source, '-o', output], diagnostic, logical)
            assert not output.exists()

        # The generator rejects colliding source spellings and leaves the
        # previous publication intact. Discovery rejects at the source too.
        collision = '7_plus_round_test'
        register(collision, 'MULIB_IMPL_COLLIDING_VALUE')
        denied(listing(home, checkout), 'catalog/MANIFEST registration drift')
        previous = catalog(home).read_bytes()
        denied(generate, 'ambiguous manifest alias', logical, f'{name}, {collision}')
        assert catalog(home).read_bytes() == previous
        output = directory/'ambiguous.json'
        denied([*command, '--inspect-kernels', source, '-o', output],
               'ON1X-KERNEL-PROFILE', 'ambiguous manifest alias', logical)
        assert not output.exists()

        # A registry from an older generator could contain both canonical
        # declarations; listing must not report either spelling as eligible.
        legacy = json.loads(previous)
        legacy['modules'][collision] = dict(future['declaration'], header=f'include/mulib-{collision}.h',
                                            macro='MULIB_IMPL_COLLIDING_VALUE')
        catalog(home).write_text(json.dumps(legacy))
        rows = check_registry(home, checkout)
        for key in (name, collision):
            row = rows[key]
            assert row['source_module'] == logical and row['source_status'] == 'ambiguous'
            assert row['source_candidates'] == [name, collision] and row['discovery_profile'] is None

    print(f'Static KernelBind: {len(modules)} source spellings scanned; real and new CDecls inspected without admission')
    return 0


if __name__ == '__main__':
    sys.exit(main())
