#!/usr/bin/env python3
"""Exercise real sandbox ExoLib evidence, one C11 TU, and closed admission."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


def run(args):
    return subprocess.run([str(x) for x in args], capture_output=True, text=True, timeout=150)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


exo, on1xc, root, work = (Path(p).resolve() for p in sys.argv[1:5])
cc = sys.argv[5]
flags = [arg.removeprefix('--cflag=') for arg in sys.argv[6:]]
clang = shutil.which('clang')
target_result = run([cc, '-dumpmachine'])
assert target_result.returncode == 0, target_result
target = target_result.stdout.strip()
if not clang or not sys.platform.startswith('linux') or not re.fullmatch(
    r'(?:x86_64|aarch64)(?:-[a-z0-9_]+)?-linux-gnu', target
):
    print('UNSUPPORTED: sandbox SDK test requires Linux x86-64/AArch64 GNU and compatible Clang resources')
    sys.exit(77)
resource = run([clang, '-print-resource-dir']).stdout.strip()
assert (Path(resource)/'include/stddef.h').is_file(), resource
work.mkdir(parents=True, exist_ok=True)
fixture = root/'tests/fixtures/kernel_sandbox.on1x'
catalog = json.loads((root/'contracts-exotract/catalog.json').read_text())['profiles']
names = ('sandbox-profile', 'seccomp', 'landlock', 'namespace', 'cgroup2', 'ipc-framed',
         'capbroker', 'sandbox-process')
helper = [sys.executable, root/'tools/on1x-build.py', '--no-prelude',
          '--exotract', exo, '--on1xc', on1xc, '--cc', cc, '--resource-dir', resource]
flag_args = ['--cflag='+flag for flag in flags]

with tempfile.TemporaryDirectory(dir=work) as tmp:
    tmp = Path(tmp)
    report = tmp/'discovery.json'
    result = run([*helper, '--inspect-kernels', fixture, '-o', report, *flag_args])
    assert result.returncode == 0, result
    data = json.loads(report.read_text())
    assert data['safe_export'] is False and data['target'] == target
    assert set(data['modules']) == {'mulib.'+name.replace('-', '_') for name in names}
    for name in names:
        module = data['modules']['mulib.'+name.replace('-', '_')]
        required = set(catalog[name]['required_symbols'])
        assert module['safe_export'] is False and module['callable'] is False
        assert module['status'] == 'not_representable'
        assert set(module['symbols']) == required and all(not s['callable'] for s in module['symbols'].values())
        metadata = module['metadata']
        assert metadata['requires_review'] and metadata['unsafe_by_default']
        assert all(entry['safe'] is False for entry in metadata['imports'])
        cdecl = metadata['cdecl']
        assert cdecl['schema_version'] == 2 and cdecl['complete'] and cdecl['safe_export'] is False
        assert cdecl['target']['triple'] == target and cdecl['target']['pointer_bits'] == 64
        assert cdecl['revision'] == 'sha256:'+sha(cdecl['header']) and re.fullmatch('[0-9a-f]{64}', cdecl['fingerprint'])
        assert str(root/'contracts-exoml'/catalog[name]['sandbox_contract']) in {
            item['path'] for item in cdecl['inputs']}
        active = {m['name'] for m in cdecl['preprocessing']['final_macros'] if m['defined']}
        assert {'_POSIX_C_SOURCE', '_GNU_SOURCE'} <= active and not any(m.startswith('MULIB_IMPL_') for m in active)
        profile = module['exolib']['header_only']
        assert module['exolib']['parser'] == 'exoml/libclang' and not module['exolib']['externally_imported']
        assert profile['implementation_translation_units'] == 1
        assert profile['implementation_defines'].count('MULIB_IMPL_SANDBOX_PROFILE') == 1
        assert module['contract']['bundle_provenance']['review_status'] == 'unreviewed'
    assert all(sha(path) == digest for path, digest in data['inputs'].items())

    archive = tmp/'archive'
    command = [sys.executable, root/'tools/on1x-exotract.py', 'build', 'sandbox-process', 'capbroker',
               '--output', archive, '--compiler', cc, '--resource-dir', resource, '--exotract', exo]
    result = run([*command, *flag_args])
    assert result.returncode == 0, result
    plan = json.loads((archive/'build-plan.json').read_text())
    lock = json.loads((archive/'lock.json').read_text())
    assert lock['safe_export'] is False and lock['compiler_target'] == target
    assert set(lock['sandbox_profiles']) == set(names)
    assert all(sha(archive/path) == digest for path, digest in lock['artifacts'].items())
    assert plan['implementation_translation_units'] == 1
    assert len(plan['implementation_macros']) == len(names) == len(set(plan['implementation_macros']))
    members = run(['ar', 't', archive/'libmulib.a'])
    assert members.returncode == 0 and members.stdout.splitlines() == ['mulib.o'], members
    surfaces = json.loads((archive/'symbols.json').read_text())['libraries']
    for name in names:
        saved = json.loads((archive/(name+'.sandbox-evidence.json')).read_text())
        expected = data['modules']['mulib.'+name.replace('-', '_')]
        assert saved == expected, name
        assert set(surfaces[name]['archive_exports']) == set(saved['symbols'])
        assert not surfaces[name]['declaration_only']
    exe = tmp/'c11-client'
    result = run([cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', *flags,
                  '-I', archive, '-I', root.parent/'kernels', root/'tests/fixtures/kernel_sandbox_archive.c',
                  archive/'libmulib.a', '-o', exe])
    assert result.returncode == 0, result
    result = run([exe])
    assert result.returncode == 0, result
    layouts = []
    for name, field in (('sandbox_profile', 'max_broker_bytes'), ('sandbox_process', 'group'),
                        ('capbroker_op', 'handler')):
        module = data['modules']['mulib.'+name.removesuffix('_op')]
        types = {t['id']: t for t in module['metadata']['cdecl']['types']}
        record, = (t for t in types.values() if t['spelling'] == 'struct mulib_'+name)
        member, = (f for f in record['fields'] if f['name'] == field)
        assert record['kind'] == 'record' and member['bit_width'] is None
        layouts.append((record['size'], record['alignment'], member['offset_bits']))
        if field == 'handler':
            pointer = types[types[member['type']]['canonical_id']]
            assert pointer['kind'] == 'pointer'
            callback = types[pointer['element']]
            assert callback['kind'] == 'function' and callback['prototype'] and not callback['variadic']
            assert callback['calling_convention'] == 1 and len(callback['arguments']) == 7
    assert [tuple(map(int, line.split())) for line in result.stdout.splitlines()] == layouts
    result = run([*command, *flag_args])
    assert result.returncode == 1 and 'output already exists' in result.stderr, result
    assert all(sha(archive/path) == digest for path, digest in lock['artifacts'].items())

    rejected = tmp/'rejected'
    for extra, diagnostic in (
        (['--kernel-bind'], 'ON1X-KERNEL-NOT-ADMITTED'),
        (['--inspect-kernels', '--kernel-review-candidate'], 'ON1X-KERNEL-NOT-ADMITTED'),
        (['--inspect-kernels', '--cflag=-DMULIB_IMPL_SANDBOX_PROCESS'], 'implementation macros'),
        (['--inspect-kernels', '--cflag=-U_GNU_SOURCE'], 'declaration features are fixed'),
    ):
        result = run([*helper, fixture, '-o', rejected, *extra])
        assert result.returncode == 1 and diagnostic in result.stderr and not rejected.exists(), result
    unsafe = tmp/'unsafe.on1x'
    unsafe.write_text('unsafe import kernel mulib.sandbox_process as Worker;\nfn main() {}\n')
    # A source-adjacent sidecar may not affect review or turn discovery into a call.
    (tmp/'sandbox-process.exocontract.json').write_text('{"safe_export":true,"review_status":"verified"}')
    result = run([*helper, '--kernel-bind', unsafe, '-o', rejected])
    assert result.returncode == 1 and 'ON1X-KERNEL-UNSAFE-UNSUPPORTED' in result.stderr and not rejected.exists(), result
    result = run([*helper, '--inspect-kernels', fixture, '--exotract', tmp/'absent-exotract', '-o', rejected])
    assert result.returncode == 1 and 'executable unavailable' in result.stderr and not rejected.exists(), result

    # An isolated tool copy can use a stale sidecar without touching the real input.
    copied = tmp/'copy'
    for directory, files in (
        ('tools', ['on1x-exotract.py', 'on1x-contracts.py']),
        ('stdlib/mulib', ['housekeeping.py', 'catalog.json']),
        ('contracts-exotract', ['catalog.json', 'kernel-discovery-v1.lua']),
        ('contracts-exoml/sandbox', ['sandbox-profile.exocontract.json']),
    ):
        (copied/directory).mkdir(parents=True)
        for name in files:
            shutil.copyfile(root/directory/name, copied/directory/name)
    sidecar = copied/'contracts-exoml/sandbox/sandbox-profile.exocontract.json'
    original = json.loads(sidecar.read_text())
    for which, diagnostic in (('revision', 'sidecar header revision drift'),
                              ('review_status', 'unreviewed provenance mismatch')):
        forged = json.loads(json.dumps(original))
        forged['bundle_provenance'][which] = 'sha256:'+'0'*64 if which == 'revision' else 'reviewed'
        sidecar.write_text(json.dumps(forged))
        result = run([sys.executable, copied/'tools/on1x-exotract.py', '--root', root.parent,
                      'build', 'sandbox-profile', '--output', rejected, '--compiler', cc,
                      '--resource-dir', resource, '--exotract', exo])
        assert result.returncode == 1 and diagnostic in result.stderr and not rejected.exists(), result
    assert not list(tmp.glob('.exotract-*')) and not list(tmp.glob('.kernel-discovery-*'))

print('Sandbox ExoLib/CDecl, unreviewed ExoML, one-TU C11 client, drift and admission rejection PASS')
