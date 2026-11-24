#!/usr/bin/env python3
"""Raw static-inline C11 wrappers from the actual ExoLib/CDecl SDK."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile


cli = argparse.ArgumentParser()
cli.add_argument('exotract', type=Path)
cli.add_argument('on1xc', type=Path)
cli.add_argument('root', type=Path)
cli.add_argument('work', type=Path)
cli.add_argument('cc')
cli.add_argument('--cflag', action='append', default=[])
a = cli.parse_args()
clang = shutil.which('clang')
if not clang or sys.platform != 'linux' or platform.machine() != 'x86_64':
    print('UNSUPPORTED: inline wrapper test needs Linux x86-64 and compatible Clang resource headers')
    sys.exit(77)
resource = subprocess.check_output([clang, '-print-resource-dir'], text=True).strip()


def run(*argv):
    return subprocess.run([str(s) for s in argv], text=True, capture_output=True, timeout=180)


def ok(*argv):
    result = run(*argv)
    assert result.returncode == 0, result
    return result.stdout


def denied(message, *argv):
    result = run(*argv)
    assert result.returncode != 0 and message in result.stderr, result


tool = a.root/'tools/on1x-exotract.py'
build = a.root/'tools/on1x-build.py'
fixture = a.root/'tests/fixtures/inline_wrappers_client.c'
include = (a.root/'../kernels').resolve()
switch = 'MULIB_IMPL_FRACTION'
rescale = 'mulib_media_timebase_rescale'
prefix = 'on1x_inline_v1_'
selections = ['media-core:mulib_media_i64_add', 'media-timebase:mulib_media_timebase_gcd',
              f'media-timebase:{rescale}']
options = ['--exotract', a.exotract, '--compiler', a.cc, '--resource-dir', resource,
           *['--cflag='+f for f in a.cflag]]
flags = [a.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', *a.cflag]

a.work.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(dir=a.work) as tmp:
    work = Path(tmp)
    bundle, again, failed = (work/name for name in ('bundle', 'second', 'failed'))
    command = [sys.executable, tool, 'wrap-inline', *options]
    ok(*command, *selections, '--output', bundle)
    ok(*command, *reversed(selections), '--output', again)
    lock = json.loads((bundle/'lock.json').read_bytes())
    assert lock['schema'] == 'on1x.exotract-raw-inline/v1' and lock['separate_tu_link'] is True
    assert lock['safe_export'] is False and lock['kernelbind_callable'] is False
    names = {selection.split(':')[1] for selection in selections}
    external = set(lock['external_symbols'])
    assert {n for n in external if n.startswith(prefix)} == {prefix+n for n in names}
    assert not names & external
    assert ok('ar', 't', bundle/'libmulib.a') == 'mulib.o\n'
    plan = json.loads((bundle/'build-plan.json').read_bytes())
    assert plan['implementation_translation_units'] == 1
    assert plan['implementation_macros'].count(switch) == 1
    assert (bundle/'mulib.c').read_text().count(f'#define {switch}\n') == 1
    assert switch not in (bundle/'on1x-inline.h').read_text()
    for filename in ('on1x-inline.h', 'mulib.c', 'link-probe.c', 'build-plan.json',
                     'media-core.discovery.json', 'media-timebase.discovery.json'):
        assert (bundle/filename).read_bytes() == (again/filename).read_bytes(), filename
    for entry in lock['wrappers']:
        meta = json.loads((bundle/(entry['module']+'.discovery.json')).read_bytes())
        assert meta['status'] == 'not_representable' and meta['callable'] is False
        assert meta['safe_export'] is False and meta['contract'] is None
        graph = meta['metadata']['cdecl']
        assert graph['fingerprint'] == lock['cdecl_fingerprints'][entry['module']]
        decl, = [d for d in graph['declarations'] if d['id'] == entry['declaration_id']]
        assert decl['name'] == entry['c_name'] and decl['type'] == entry['c_type_id']
        assert decl['inline'] and decl['definition'] and decl['linkage'] == '2'
    for path, expected in lock['inputs'].items():
        assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == expected, path
    for path, expected in lock['artifacts'].items():
        assert hashlib.sha256((bundle/path).read_bytes()).hexdigest() == expected, path
    assert str(kernels/'mulib-fraction.h') in lock['inputs']

    # The separately compiled caller has unresolved references until the one
    # wrapper object is linked. Run it against manual timing/overflow oracles.
    obj, exe = work/'client.o', work/'client'
    ok(*flags, '-I', bundle, '-I', include, '-c', fixture, '-o', obj)
    denied(prefix+rescale, *flags, obj, '-o', exe)
    ok(*flags, obj, bundle/'libmulib.a', '-o', exe)
    ok(exe)
    denied(prefix, *flags, obj, bundle/'mulib.o', again/'mulib.o', '-o', work/'duplicate')

    # Same-width member signedness changes are caught independent of layout.
    # Same-width pointer-parameter drift is caught by exact prototype checks.
    # This is a fresh consumer object, not the TU that generated the wrapper.
    mutated = work/'mutated'
    mutated.mkdir()
    for name in ('mulib-media-timebase.h', 'mulib-media-core.h', 'mulib-fraction.h'):
        shutil.copyfile(include/name, mutated/name)
    member = mutated/'mulib-fraction.h'
    original = member.read_text()
    assert 'int64_t num, den;' in original
    member.write_text(original.replace('int64_t num, den;', 'uint64_t num; int64_t den;', 1))
    denied('inline ABI member type: struct mulib_fraction.num',
           *flags, '-I', bundle, '-I', mutated, '-c', bundle/'link-probe.c', '-o', work/'bad.o')
    member.write_text(original)
    signature = mutated/'mulib-media-timebase.h'
    original = signature.read_text()
    assert 'int64_t *out' in original
    signature.write_text(original.replace('int64_t *out', 'uint64_t *out', 1))
    denied(f'inline ABI prototype: {rescale}', *flags, '-I', bundle, '-I', mutated,
           '-c', bundle/'link-probe.c', '-o', work/'bad.o')
    definition = f'static inline bool {rescale}'
    assert definition in original
    signature.write_text(original.replace(definition, 'static inline __attribute__((ms_abi)) bool '+rescale, 1))
    denied(f'inline ABI prototype: {rescale}', *flags, '-I', bundle, '-I', mutated,
           '-c', bundle/'link-probe.c', '-o', work/'bad.o')

    for arguments, error in (
        ([selections[0], selections[0]], 'duplicate wrap-inline symbol'),
        (['fraction:mulib_fraction_make'], 'requires selected MANIFEST static-inline'),
        (['media-core:mulib_media_ranges_disjoint'], 'unsupported CDecl value type: const void'),
        (['media-core:mulib_media_timebase_rescale'], 'EXOTRACT-REQUIRED-MISSING'),
        ([selections[0], '--cflag=-DMULIB_IMPL_FRACTION'], 'accepts only documented'),
        ([selections[0], '--cflag=-m32'], 'accepts only documented'),
        ([selections[0], '--ar', 'false'], 'false failed'),
    ):
        denied(error, *command, *arguments, '--output', failed)
        assert not failed.exists()
    old_lock = (bundle/'lock.json').read_bytes()
    denied('output already exists', *command, *selections, '--output', bundle)
    assert (bundle/'lock.json').read_bytes() == old_lock
    assert not list(work.glob('.exotract-inline-*'))

    # The emitted raw symbols/lock cannot satisfy either KernelBind spelling.
    source = work/'app.on1x'
    for keyword, error in (('', 'NOT-ADMITTED'), ('unsafe ', 'UNSAFE-UNSUPPORTED')):
        source.write_text(f'{keyword}import kernel mulib.media_timebase as Time;\nfn main() {{}}\n')
        denied('ON1X-KERNEL-'+error, sys.executable, build, source, '--on1xc', a.on1xc,
               '--kernel-bind', '--no-prelude', '--exotract', a.exotract, '--cc', a.cc,
               '--resource-dir', resource, '-o', failed)
    assert not failed.exists()
print('raw static-inline CDecl wrappers: deterministic one-TU build, ABI drift rejection and separate C11 execution PASS')
