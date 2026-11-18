#!/usr/bin/env python3
"""Exercise the staged discovery path through real ExoLib and ExoML."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from support import run

exo, on1xc, root, work = map(lambda p: Path(p).resolve(), sys.argv[1:5])
cc = sys.argv[5]
cflags = [arg.removeprefix('--cflag=') for arg in sys.argv[6:]]
clang = shutil.which('clang')
if not clang or not sys.platform.startswith('linux'):
    print('UNSUPPORTED: KernelBind discovery test requires compatible clang resources and Linux')
    sys.exit(77)
resource = subprocess.check_output([clang, '-print-resource-dir'], text=True).strip()
fixture = root/'tests/fixtures/kernel_discovery.on1x'
work.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as directory:
    directory = Path(directory)
    report = directory/'discovery.json'
    cmd = [sys.executable, root/'tools/on1x-build.py', '--inspect-kernels', '--no-prelude',
           '--exotract', exo, '--on1xc', on1xc, '--cc', cc, '--resource-dir', resource]
    p = run([*cmd, fixture, '-o', report])
    assert p.returncode == 0, p
    original = report.read_bytes()
    data = json.loads(original)
    assert data['schema'] == 'on1x.kernel-discovery/v1' and data['safe_export'] is False
    assert len(data['requests']) == 5 and len(data['modules']) == 5
    for name, module in data['modules'].items():
        assert module['status'] == 'missing_review' and module['callable'] is False and module['safe_export'] is False
        assert module['symbols'] and all(s['callable'] is False for s in module['symbols'].values())
        assert module['metadata']['schema'] == 'moonstone.exotract.on1x/2'
        graph = module['metadata']['cdecl']
        assert graph['complete'] is True and graph['schema_version'] == 2 and graph['safe_export'] is False
        assert graph['target']['triple'] == data['target']
        assert graph['preprocessing']['final_snapshot_complete'] is True
        assert all(not m['defined'] for m in graph['preprocessing']['final_macros'] if m['name'].startswith('MULIB_IMPL_'))
        assert module['exolib']['header_only']['implementation_translation_units'] == 1
        assert graph['revision'] == 'sha256:'+hashlib.sha256(Path(graph['header']).read_bytes()).hexdigest()
        if name == 'mulib.json':
            assert module['contract']['bundle_provenance']['review_status'] == 'unreviewed'
        else:
            assert module['contract'] is None
    hist = data['modules']['mulib.histprice']
    assert hist['exolib']['header_only']['implementation_defines'] == ['MULIB_IMPL_STATSPROC','MULIB_IMPL_HISTPRICE']
    assert hist['exolib']['header_only']['dependencies'] == [str(root.parent/'include/mulib-statsproc.h')]
    graph = hist['metadata']['cdecl']
    holt = next(item for item in graph['declarations'] if item['name'] == 'mulib_histprice_holt')
    assert holt['kind'] == 'function' and holt['definition'] is False and len(holt['parameters']) == 7
    signature = next(typ for typ in graph['types'] if typ['id'] == holt['type'])
    assert signature['kind'] == 'function' and signature['variadic'] is False and len(signature['arguments']) == 7
    for path, sha in data['inputs'].items():
        assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == sha, path
    again = directory/'again.json'
    p = run([*cmd, fixture, '-o', again])
    assert p.returncode == 0 and again.read_bytes() == original, p
    p = run([*cmd, fixture, '-o', report])
    assert p.returncode != 0 and report.read_bytes() == original and 'already exists' in p.stderr, p

    unknown = directory/'unknown.on1x'
    unknown.write_text('import kernel mulib.unknown;fn main(){}')
    unsupported = directory/'not-published.json'
    p = run([*cmd, unknown, '-o', unsupported])
    assert p.returncode == 1 and 'ON1X-KERNEL-PROFILE' in p.stderr and str(unknown)+':1:1:' in p.stderr, p
    p = run([*cmd, fixture, '-o', unsupported, '--cflag=-DMULIB_IMPL_HISTPRICE'])
    assert p.returncode == 1 and 'implementation macros' in p.stderr, p
    shadow = directory/'shadow'
    shadow.mkdir()
    (shadow/'mulib-statsproc.h').symlink_to(root.parent/'include/mulib-statsproc.h')
    p = run([*cmd, fixture, '--mulib-path', shadow, '-o', unsupported])
    assert p.returncode == 1 and 'ambiguous MuLIB kernel header' in p.stderr, p
    assert not unsupported.exists()

    # A proof-shaped file in the source directory is not a compiler input.
    # Never edit the actual shipped sidecar to create a positive authorization.
    source = directory/'forgery.on1x'
    source.write_text('import kernel mulib.json as Json;\nunsafe import kernel mulib.json as Other;\nfn main(){}')
    (directory/'json.exocontract.json').write_text('{"safe_export":true,"review_status":"reviewed"}')
    p = run([*cmd, source, '-o', unsupported])
    assert p.returncode == 0, p
    dedup = json.loads(unsupported.read_bytes())
    assert len(dedup['requests']) == 2 and len(dedup['modules']) == 1 and dedup['requests'][1]['unsafe']
    found = dedup['modules']['mulib.json']
    assert found['contract']['bundle_provenance']['review_status'] == 'unreviewed'
    p = run([on1xc, '--emit-c', source])
    assert p.returncode == 1 and 'ON1X-KERNEL-NOT-ADMITTED' in p.stderr and not p.stdout, p
    assert not list(directory.glob('.kernel-discovery-*'))

    # An isolated copy of the installed helper's inputs allows corruption
    # tests without editing any shipped sidecars or authorizing the fixture.
    corrupt = directory/'corrupt-tools'
    for subdir, names in (
        ('tools', ['on1x-build.py', 'on1x-exotract.py', 'on1x-contracts.py']),
        ('stdlib/mulib', ['housekeeping.py', 'catalog.json']),
        ('contracts-exotract', ['kernel-discovery-v1.lua', 'catalog.json']),
        ('contracts-exoml', [p.name for p in (root/'contracts-exoml').glob('*.json')]),
    ):
        (corrupt/subdir).mkdir(parents=True)
        for name in names:
            shutil.copyfile(root/subdir/name, corrupt/subdir/name)
    candidate = corrupt/'contracts-exoml/json.exocontract.json'
    original_sidecar = candidate.read_text()
    cmd2 = [cmd[0], corrupt/'tools/on1x-build.py', *cmd[2:], '--mulib-root', root.parent]
    not_published = directory/'corrupt.json'
    reviewed = json.loads(original_sidecar)
    reviewed['bundle_provenance']['review_status'] = 'reviewed'
    wrong_symbol = json.loads(original_sidecar)
    wrong_symbol['functions'][0]['c_name'] += '_missing'
    for content, error in (
        (json.dumps(reviewed), 'seed review/revision is not source-backed'),
        (original_sidecar.replace('{', '{"schema_version":2,', 1),
         'duplicate JSON key'),
        (original_sidecar.replace('"sha256:', '"sha256:00', 1), 'seed review/revision is not source-backed'),
        (json.dumps(wrong_symbol), 'ExoML contract validation failed'),
    ):
        assert content != original_sidecar
        candidate.write_text(content)
        p = run([*cmd2, source, '-o', not_published])
        assert p.returncode == 1 and error in p.stderr and not not_published.exists(), p

    # Compare the CDecl inventory with a strict C11 separate-TU caller, and use
    # the *existing* Exotract archive flow, independently of KernelBind imports.
    archive = directory/'histprice'
    p = run([sys.executable, root/'tools/on1x-exotract.py', 'build', 'histprice',
             '--exotract', exo, '--compiler', cc, '--resource-dir', resource, '--output', archive,
             *[f'--cflag={flag}' for flag in cflags]])
    assert p.returncode == 0, p
    p = run(['ar', 't', archive/'libmulib.a'])
    assert p.returncode == 0 and p.stdout.splitlines() == ['mulib.o'], p
    client = directory/'holt-c'
    p = run([cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', *cflags,
             '-I', root.parent/'include', root/'tests/fixtures/kernel_histprice_boundary.c',
             archive/'libmulib.a', '-lm', '-o', client])
    assert p.returncode == 0, p
    assert run([client]).returncode == 0

    # Discovery is a real CDecl/ExoLib check, not admission of the projections
    # used by these migration candidates.
    migrations = root/'tests/fixtures/kernel_migrations'
    for relative, name, exports in (
        ('data/ini.on1x', 'ini', ['mulib_ini_parse']),
        ('prettyprint/render.on1x', 'prettysexpr', ['mulib_pretty_render_expr']),
        ('runtime/channel.on1x', 'channel', ['mulib_channel_create', 'mulib_channel_receive']),
        ('runtime/crypto.on1x', 'cryptohash', ['mulib_blake3', 'mulib_blake2s']),
        ('runtime/socket.on1x', 'socket', ['mulib_socket_pair', 'mulib_socket_receive']),
    ):
        report = directory/(name+'.json')
        p = run([*cmd, migrations/relative, '-o', report])
        assert p.returncode == 0, p
        module = json.loads(report.read_bytes())['modules']['mulib.'+name]
        assert module['exolib']['header_only']['implementation_translation_units'] == 1
        assert module['status'] == 'missing_review' and module['safe_export'] is False
        for symbol in exports:
            assert module['symbols'][symbol]['callable'] is False

    # Executable unsafe scalar profile. A separate C11 oracle uses the same
    # public prototypes and a separately compiled kernel implementation.
    clamp = root/'examples/kernel/clamp.on1x'
    app, lock = directory/'kernel-clamp', directory/'kernel-clamp.json'
    build_cmd = [cmd[0], cmd[1], '--kernel-bind', *cmd[3:], *[f'--cflag={flag}' for flag in cflags]]
    p = run([*build_cmd, clamp, '--mulib-plan', lock, '-o', app])
    assert p.returncode == 0, p
    result = run([app])
    assert result.returncode == 0 and result.stdout == '42\n9223372036854775807\n-9223372036854775808\n9223372036854775807\n-9223372036854775808\n6\n', result
    evidence = json.loads(lock.read_bytes())
    assert evidence['safe_export'] is False and evidence['plan']['implementation_translation_units'] == 1
    assert evidence['plan']['implementation_macros'] == ['MULIB_IMPL_CLAMP']
    assert 'mulib.c' in evidence['generated_inputs']
    interface = evidence['kernel_interface']
    assert interface['schema'] == 'on1x.kernel-interface/v1' and interface['safe_export'] is False
    assert set(interface['exports']) == {'add_i64', 'sub_i64'}
    assert all(v['effect'] == 'unsafe_foreign' and v['status'] == 'unsafe' and v['c_type_id']
               for v in interface['exports'].values())
    impl = directory/'clamp-impl.c'
    impl.write_text('#define MULIB_IMPL_CLAMP\n#include "mulib-clamp.h"\n')
    oracle = directory/'oracle'
    p = run([cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', *cflags,
             '-I', root.parent/'include', root/'tests/fixtures/kernel_clamp_oracle.c', impl, '-o', oracle])
    assert p.returncode == 0, p
    c_run = run([oracle])
    assert c_run.returncode == 0 and c_run.stdout == result.stdout, c_run
    duplicate = directory/'duplicate-impl.c'
    duplicate.write_bytes(impl.read_bytes())
    p = run([cc, '-std=c11', *cflags, '-I', root.parent/'include',
             root/'tests/fixtures/kernel_clamp_oracle.c', impl, duplicate,
             '-o', directory/'duplicate-oracle'])
    assert p.returncode != 0 and 'mulib_clamp_add_i64' in p.stderr, p

    safe = directory/'safe-clamp.on1x'
    safe.write_text(clamp.read_text().replace('unsafe import', 'import'))
    missing = directory/'not-built'
    p = run([*build_cmd, safe, '-o', missing])
    assert p.returncode == 1 and 'ON1X-KERNEL-NOT-ADMITTED' in p.stderr and not missing.exists(), p
    p = run([*build_cmd, clamp, '--cflag=-DMULIB_IMPL_CLAMP', '-o', missing])
    assert p.returncode != 0 and not missing.exists(), p
    previous = app.read_bytes()
    p = run([*build_cmd, clamp, '-o', app])
    assert p.returncode != 0 and 'fresh output' in p.stderr and app.read_bytes() == previous, p

    # Re-extract an incompatible declaration from a test-only alternate
    # checkout. The CDecl gate must fail even though the symbol still exists.
    changed = directory/'changed'
    changed.mkdir()
    shutil.copyfile(root.parent/'MANIFEST.yaml', changed/'MANIFEST.yaml')
    def link_or_copy(source, destination):
        try:
            Path(destination).hardlink_to(source)
        except OSError:
            shutil.copyfile(source, destination)
    shutil.copytree(root.parent/'manual', changed/'manual', copy_function=link_or_copy, symlinks=True)
    (changed/'include').mkdir()
    for header in (root.parent/'include').glob('mulib-*.h'):
        target = changed/'include'/header.name
        if header.name == 'mulib-clamp.h':
            original = header.read_text()
            drifted = original.replace('int64_t mulib_clamp_add_i64 (int64_t a, int64_t b);',
                                       'uint64_t mulib_clamp_add_i64 (int64_t a, int64_t b);')
            assert drifted != original
            target.write_text(drifted)
        else:
            link_or_copy(header, target)
    p = run([*build_cmd, clamp, '--mulib-root', changed, '-o', missing])
    assert p.returncode == 1 and 'ON1X-KERNEL-ABI' in p.stderr and not missing.exists(), p
