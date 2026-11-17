"""Real CDecl -> review-gated immutable string ABI -> checked On1x/C11.

Review flags below are deliberately synthetic test inputs, not a review of any
MuLIB adapter. No production contract is marked reviewed by this test.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import random
import shutil
import sys
import tempfile
from support import run

cli = argparse.ArgumentParser()
for name in ['compiler', 'root', 'work']:
    cli.add_argument(name, type=Path)
cli.add_argument('cc')
cli.add_argument('bind', type=Path)
cli.add_argument('--cflag', action='append', default=[])
t = cli.parse_args()
t.root = t.root.resolve()
t.work.mkdir(parents=True, exist_ok=True)

def good(args):
    p = run(args)
    assert p.returncode == 0, (args, p.returncode, p.stdout, p.stderr)
    return p.stdout

with tempfile.TemporaryDirectory(prefix='strings-', dir=t.work) as tmp:
    work = Path(tmp).resolve()
    header = work/'contract_strings.h'
    shutil.copyfile(t.root/'tests/fixtures/contract_strings.h', header)
    impl = t.root/'tests/fixtures/contract_strings.c'
    revision = 'sha256:'+hashlib.sha256(header.read_bytes()).hexdigest()
    extract = [t.bind, 'extract', header, '--revision', revision]
    clang = shutil.which('clang')
    if clang:
        extract += ['--resource-dir', good([clang, '-print-resource-dir']).strip()]
    decl = work/'strings.cdecl.json'
    decl.write_text(good(extract))
    unit = json.loads(decl.read_text())
    provenance = dict(header=header.name, revision=revision, author='SYNTHETIC TEST REVIEW FLAGS',
                      review_status='unreviewed', evidence_method='ABI regression fixture only')
    functions = []
    for d in unit['declarations']:
        if d['kind'] != 'function':
            continue
        fn = dict(c_name=d['name'], parameters=[], errors=dict(kind='none'),
                  effects=dict(effects=['read'], unknown=False),
                  concurrency=dict(thread_safety='single_thread', blocking_behavior='synchronous', suspension_safe=False),
                  provenance=copy.deepcopy(provenance))
        fn['return'] = (dict(kind='borrow', nullable=False, lifetime=dict(origin='global_static'))
                        if d['name']=='contract_static_text' else dict(kind='value', nullable=False))
        for i, p in enumerate(d['parameters']):
            arg = dict(index=i, name=p['name'], ownership='scalar', nullable=False)
            if d['name'] != 'contract_static_text':
                arg.update(ownership='borrowed_immutable', encoding='nul_terminated_bytes',
                           lifetime=dict(origin='explicit_region', region_name='call'))
            fn['parameters'].append(arg)
        functions.append(fn)
    seed = dict(schema_version=2, library='string_fixture', headers=[header.name], state='annotated',
                bundle_provenance=provenance, functions=functions)
    def dump(name, value):
        path = work/(name+'.json')
        path.write_text(json.dumps(value)+'\n')
        return path
    pinned = json.loads(good([t.bind, 'pin', decl, dump('seed', seed)]))
    def gate(name, bundle, implementation=impl, declarations=decl):
        return [t.bind, 'emit-safe', declarations, dump(name, bundle), work/name,
                '--on1xc', t.compiler, '--cc', t.cc, '--workdir', work/(name+'-gates'),
                '--impl', implementation, *sum((['--cflag', f] for f in t.cflag), [])]
    p = run(gate('unreviewed', pinned))
    assert p.returncode == 3 and 'review metadata gate failed' in p.stderr, p.stderr
    reviewed = copy.deepcopy(pinned)
    reviewed['state'] = 'reviewed'
    reviewed['bundle_provenance']['review_status'] = 'reviewed'
    for f in reviewed['functions']:
        f['provenance']['review_status'] = 'reviewed'
    good(gate('strings', reviewed))
    manifest = json.loads((work/'strings.manifest.json').read_text())
    assert manifest['profile'] == 'reviewed-immutable-string/v4', manifest
    admitted = work/'strings.admitted-extern.txt'
    assert admitted.read_text().startswith('on1x.admitted-extern/v4\n')
    assert 'borrowed_string' in admitted.read_text() and 'static_string' in admitted.read_text()
    assert all(f['safe_profile_abi_supported'] for f in manifest['eligibility']['functions'])
    assert not any(f['safe_scalar_abi_supported'] for f in manifest['eligibility']['functions'])

    # Pointer layout metadata must be checked, not silently mapped to host ABI.
    misaligned = copy.deepcopy(unit)
    for ty in misaligned['types']:
        if ty['kind'] == 'pointer':
            ty['alignment'] = 1 << 32
    p = run(gate('pointer-alignment', reviewed, declarations=dump('misaligned-cdecl', misaligned)))
    assert p.returncode == 3 and 'target compile gate failed' in p.stderr, p.stderr

    multiline = work/'multiline-implementation.c'
    multiline.write_text(impl.read_text().replace('return "static text";', 'return "line\\nnext";'))
    good(gate('multiline', reviewed, multiline))
    observed = json.loads((work/'multiline.manifest.json').read_text())['gates']['runtime_acceptance']['observed']
    assert next(f['value'] for f in observed if f['name']=='contract_static_text') == '9', observed

    consumer = work/'consumer.on1x'
    consumer.write_text('import '+json.dumps(str(work/'strings.on1x'))+';\n'+
                        (t.root/'tests/fixtures/contract_strings.on1x').read_text())
    generated = work/'consumer.c'
    good([t.compiler, '--emit-c', '--memory-check', '--admitted-extern', admitted, consumer, '-o', generated])
    denied = run([t.compiler, '--check-memory', consumer])
    assert denied.returncode != 0 and 'not covered by admitted-extern evidence' in denied.stderr, denied.stderr
    expected = '0\n3\n\nstatic text\nline\nnext\n11\n'
    # Byte-length oracle, including UTF-8 as bytes (not a Unicode-index claim).
    rng = random.Random(0xC57A)
    corpus = ['', 'é', 'hello\nworld', '"quoted"', '\\']
    corpus += [''.join(rng.choice(' abCD09') for _ in range(rng.randrange(100))) for _ in range(20)]
    body = ''.join('println(on1x_safe_contract_text_size('+json.dumps(s, ensure_ascii=False)+'));' for s in corpus)
    consumer.write_text(consumer.read_text().replace('fn main(){', 'fn main(){'+body))
    expected = ''.join(str(len(s.encode('utf-8')))+'\n' for s in corpus)+expected
    good([t.compiler, '--emit-c', '--memory-check', '--admitted-extern', admitted, consumer, '-o', generated])
    strict = [t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', work,
              '-I', t.root/'runtime']
    for tag, implementation, opt in [('c', impl, '-O0'), ('cpp', work/'impl.o', '-O2')]:
        if tag == 'cpp':
            cxx = shutil.which('clang++' if 'clang' in Path(t.cc).name else 'g++')
            assert cxx, 'C++ compiler required for separate-TU ABI test'
            good([cxx, '-x', 'c++', '-std=c++20', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                  '-c', impl, '-o', implementation, *t.cflag])
        executable = work/('consumer-'+tag)
        good([*strict, opt, generated, work/'strings.c', implementation, t.root/'runtime/on1x_runtime.c',
              *t.cflag, '-o', executable])
        assert good([executable]) == expected
    helper = work/'consumer-helper'
    good([sys.executable, t.root/'tools/on1x-build.py', consumer, '--on1xc', t.compiler,
          '--cc', t.cc, '--memory-check', '--admitted-extern', admitted,
          '--extra-c', work/'strings.c', '--extra-c', impl, '--cflag=-I'+str(work),
          *['--cflag='+flag for flag in t.cflag], '-o', helper])
    assert good([helper]) == expected
    unsupported = run([t.compiler, '--emit-ir', consumer])
    assert unsupported.returncode != 0 and 'ON1X-MIR-UNSUPPORTED' in unsupported.stderr, unsupported.stderr

    invalid_source = work/'wrong-type.on1x'
    invalid_source.write_text('import '+json.dumps(str(work/'strings.on1x'))+';fn main(){on1x_safe_contract_text_size(1);}')
    p = run([t.compiler, '--check', invalid_source])
    assert p.returncode != 0 and 'error:' in p.stderr, p.stderr
    # C callers exercise null and exact byte limits without pretending that On1x
    # can manufacture null strings. No out-of-bounds scan: storage includes NUL.
    name = 'on1x_ffi_'+unit['fingerprint']+'_contract_text_size'
    for tag, length, expected_exit in [('at-limit', 2097152, 0), ('over-limit', 2097153, 72), ('null-input', 0, 72)]:
        source = work/(tag+'.c')
        argument = '0' if tag=='null-input' else 'text'
        source.write_text('#include <stdint.h>\n#include <string.h>\n'+
                          'int64_t '+name+'(const char *);\nint main(void){'+
                          ('' if tag=='null-input' else f'static char text[{length+1}];memset(text,120,{length});')+
                          '(void)'+name+'('+argument+');return 0;}\n')
        executable = work/tag
        good([*strict, source, work/'strings.c', impl, *t.cflag, '-o', executable])
        p = run([executable])
        assert p.returncode == expected_exit, (tag, p.stderr)
        if expected_exit:
            assert 'foreign string null or byte limit exceeded' in p.stderr

    # Wrong pointer qualifier/type/layout must fail compilation against real headers.
    original = header.read_text()
    for name, before, after in [('mutable', 'const char *text', 'char *text'),
                                ('pointee', 'const char *text', 'const unsigned char *text'),
                                ('result', 'const char *contract_static_text', 'char *contract_static_text')]:
        wrong = work/name
        wrong.mkdir()
        (wrong/header.name).write_text(original.replace(before, after))
        p = run([t.cc, '-std=c11', '-I', wrong, '-c', work/'strings.c', '-o', wrong/'bad.o'])
        assert p.returncode != 0 and 'drift' in p.stderr, (name, p.stderr)

    # Full-bundle rejection, never a partial safe export.
    for name, change in [
        ('retained', lambda f: f['parameters'][0]['lifetime'].update(region_name='registration')),
        ('mutable_borrow', lambda f: f['parameters'][0].update(ownership='borrowed_mutable')),
        ('encoding', lambda f: f['parameters'][0].update(encoding='utf16')),
        ('nullable_input', lambda f: f['parameters'][0].update(nullable=True)),
        ('unknown_effect', lambda f: f['effects'].update(unknown=True)),
        ('escape', lambda f: f['effects'].update(effects=['escape'])),
        ('allocation', lambda f: f['effects'].update(effects=['allocate'])),
        ('invalidation', lambda f: f.update(invalidations=[dict(kind='next_call', description='Not an immutable result')])),
    ]:
        invalid = copy.deepcopy(reviewed)
        change(next(f for f in invalid['functions'] if f['c_name']=='contract_text_size'))
        p = run(gate(name, invalid))
        assert p.returncode in [2, 3] and not (work/(name+'.on1x')).exists(), (name, p.stderr)
    for name, change in [
        ('nullable_result', lambda r: r.update(nullable=True)),
        ('input_alias', lambda r: r.update(lifetime=dict(origin='explicit_region', region_name='call'))),
        ('owned_result', lambda r: r.update(kind='owned', lifetime=None, destroy_with='free', deallocator_family='malloc')),
    ]:
        invalid = copy.deepcopy(reviewed)
        change(next(f for f in invalid['functions'] if f['c_name']=='contract_static_text')['return'])
        p = run(gate(name, invalid))
        assert p.returncode in [2, 3] and not (work/(name+'.on1x')).exists(), (name, p.stderr)

    # A reviewed claim cannot bypass the executable null/limit boundary checks.
    for name, replacement in [('null', 'return 0;'),
                              ('oversized', 'static char huge[4194305]; memset(huge, 120, sizeof huge); return huge;')]:
        broken = work/(name+'-implementation.c')
        broken.write_text(impl.read_text().replace('return "static text";', replacement))
        p = run(gate(name, reviewed, broken))
        assert p.returncode == 3 and 'target runtime gate failed' in p.stderr, (name, p.stderr)
        assert not any((work/(name+suffix)).exists() for suffix in ['.c','.on1x','.manifest.json','.admitted-extern.txt'])
    # v1-v3 evidence cannot acquire string permission, and qualifiers are directional.
    for name, text in [('legacy', admitted.read_text().replace('/v4\n', '/v3\n', 1)),
                       ('direction', admitted.read_text().replace('borrowed_string', 'static_string')),
                       ('unqualified', admitted.read_text().replace('borrowed_string', 'string'))]:
        artifact = work/(name+'.txt')
        artifact.write_text(text)
        p = run([t.compiler, '--check-memory', '--admitted-extern', artifact, consumer])
        assert p.returncode != 0 and 'ON1X-ADMISSION' in p.stderr, (name, p.stderr)
print('Reviewed immutable string ABI gates, C/C++ execution and fail-closed negatives PASS')
