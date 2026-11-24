#!/usr/bin/env python3
"""Real SDK/admission integration. Ephemeral keys are TEST authorities, not a shipped review."""
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

exotract, compiler, root, work, cc, *cflags = sys.argv[1:]
cflags = [flag.removeprefix('--cflag=') for flag in cflags]
root, work = Path(root).resolve(), Path(work).resolve()
work.mkdir(parents=True, exist_ok=True)
resource = subprocess.run(['clang', '-print-resource-dir'], capture_output=True, text=True).stdout.strip()
if not sys.platform.startswith('linux') or not shutil.which('openssl') or not Path(resource, 'include/stddef.h').is_file():
    print('UNSUPPORTED: reviewed KernelBind requires Linux, OpenSSL and Clang resource headers')
    sys.exit(77)


def run(args, ok=True):
    p = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=120)
    if ok:
        assert p.returncode == 0, p
    return p


def load(path):
    return json.loads(path.read_bytes())


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


with tempfile.TemporaryDirectory(dir=work) as temporary:
    d = Path(temporary)
    app = root/'examples/kernel/clamp_safe.on1x'
    common = [sys.executable, root/'tools/on1x-build.py', '--no-prelude',
              '--on1xc', compiler, '--exotract', exotract, '--cc', cc, '--resource-dir', resource,
              *['--cflag='+f for f in cflags]]
    subject = d/'subject.json'
    run([*common, '--kernel-review-candidate', app, '-o', subject])
    duplicate = d/'subject-again.json'
    run([*common, '--kernel-review-candidate', app, '-o', duplicate])
    assert subject.read_bytes() == duplicate.read_bytes(), 'review identity must be reproducible'
    assert load(subject)['schema'] == 'on1x.kernel-review-subject/v1'

    private, public, signature, review = [d/name for name in ('test-key.pem', 'test-public.der', 'review.sig', 'review.json')]
    run(['openssl', 'genpkey', '-algorithm', 'ED25519', '-out', private])
    run(['openssl', 'pkey', '-in', private, '-pubout', '-outform', 'DER', '-out', public])
    review.write_text(json.dumps(dict(schema='on1x.kernel-review/v1', reviewer='Ephemeral test authority',
                                     review_reference='TEST ONLY: admission protocol, not an independent source audit',
                                     subject=load(subject))))

    def sign():
        run(['openssl', 'pkeyutl', '-sign', '-inkey', private, '-rawin', '-in', review, '-out', signature])

    sign()
    build = [*common, '--kernel-bind', '--kernel-review', review,
             '--kernel-review-key', public, '--kernel-review-signature', signature]
    bundle = d/'bundle'
    run([*build, app, '-o', bundle])
    result = run([bundle/'program'])
    expected = '42\n9223372036854775807\n-9223372036854775808\n9223372036854775807\n-9223372036854775808\n6\n0\n10\n4\n-7\n'
    assert result.stdout == expected, result
    admission, interface = load(bundle/'admission.json'), load(bundle/'interface.json')
    assert admission['safe_export'] is True and admission['schema'] == 'on1x.kernel-admission/v1'
    assert admission['executable_gate']['comparisons'] == 2670 and admission['executable_gate']['passed']
    assert all(digest(bundle/name) == h for name, h in admission['artifacts'].items())
    assert interface['profile'] == 'reviewed-clamp-i64/v2'
    assert interface['safe_export'] is True and set(interface['exports']) == {'add_i64', 'sub_i64', 'i64'}
    assert load(subject)['symbols']['i64']['precondition'] == interface['exports']['i64']['precondition']
    assert all(e['status'] == 'admitted' and e['effect'] == 'foreign_call' for e in interface['exports'].values())
    assert load(bundle/'build-plan.json')['implementation_macros'] == ['MULIB_IMPL_CLAMP']
    assert run(['ar', 't', bundle/'libmulib.a']).stdout == 'mulib.o\n'
    assert 'extern fn' not in app.read_text() and 'unsafe' not in app.read_text()
    assert 'mulib-clamp.h' not in (bundle/'main.c').read_text()
    assert '_Generic(&mulib_clamp_add_i64' in (bundle/'kernel-adapter.c').read_text()
    # The deployment product uses no SDK, libclang, Lua, or C++ runtime.
    linked = run(['ldd', bundle/'program']).stdout.lower()
    assert not any(name in linked for name in ('libclang', 'liblua', 'libstdc++', 'libexoml', 'libexotract'))
    oracle = d/'oracle'
    strict = [cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', *cflags,
              '-I', root.parent/'kernels']
    run([*strict, root/'tests/fixtures/kernel_clamp_oracle.c', bundle/'libmulib.a', '-o', oracle])
    assert run([oracle]).stdout == result.stdout
    raw = d/'raw-clamp'
    run([*common, '--kernel-bind', root/'examples/kernel/clamp.on1x', '-o', raw])
    assert run([raw]).stdout == expected
    # Reversed bounds are rejected before the foreign call, after evaluating
    # each argument once and in source order, in both admission modes.
    for unsafe in (False, True):
        invalid = d/f'invalid-interval-{unsafe}.on1x'
        call = 'println(C.i64(arg(8),arg(2),arg(1)));'
        invalid.write_text(('unsafe ' if unsafe else '') + 'import kernel mulib.clamp as C;\n'
                           'fn arg(x:i64)->i64 {println(x);return x;}\n'
                           'fn main(){' + ('unsafe {' + call + '}' if unsafe else call) + '}')
        output = d/f'bad-interval-{unsafe}'
        run([*(common+['--kernel-bind'] if unsafe else build), invalid, '-o', output])
        p = run([output if unsafe else output/'program'], ok=False)
        assert p.returncode == 70 and p.stdout == '8\n2\n1\n' and 'assertion failed' in p.stderr, p
    # A staged compiler caller alone cannot link to the canonical archive.
    p = run([*strict, '-I', root/'runtime', bundle/'main.c', root/'runtime/on1x_runtime.c',
             bundle/'libmulib.a', '-o', d/'unadmitted'], ok=False)
    assert p.returncode and 'on1x_kernel_scalar_v1_' in p.stderr, p
    # Duplicate implementation TU must be rejected by the linker.
    p = run([*strict, root/'tests/fixtures/kernel_clamp_oracle.c',
             bundle/'mulib.c', bundle/'mulib.c', '-o', d/'duplicate'], ok=False)
    assert p.returncode and 'mulib_clamp_add_i64' in p.stderr, p

    # Review is for the binding, not a particular application's aliases/source.
    dependency = d/'arithmetic.on1x'
    dependency.write_text('import kernel mulib.clamp as Arithmetic;\n'
                          'fn answer()->i64{return Arithmetic.add_i64(40,2);}\n')
    consumer = d/'consumer.on1x'
    consumer.write_text('import arithmetic as A;\nimport kernel mulib.clamp as C;\n'
                        'fn arg(x:i64)->i64{println(x);return x;}\n'
                        'fn main(){println(A.answer());println(C.add_i64(arg(40),arg(2)));'
                        'let f=fn()->i64{return C.sub_i64(44,2);};println(f());}\n')
    run([*build, consumer, '-o', d/'multi'])
    assert run([d/'multi/program']).stdout == '42\n40\n2\n42\n42\n'
    mixed = d/'mixed.on1x'
    mixed.write_text('import kernel mulib.clamp as Reviewed;\n'
                     'unsafe import kernel mulib.clamp as Raw;\n'
                     'fn main(){let result=unsafe { Raw.sub_i64(43,1) };'
                     'println(Reviewed.add_i64(result,0));}\n')
    mixed_bundle = d/'mixed-bundle'
    run([*build, mixed, '-o', mixed_bundle])
    assert run([mixed_bundle/'program']).stdout == '42\n'
    assert run(['ar', 't', mixed_bundle/'libmulib.a']).stdout == 'mulib.o\n'
    raw_interface = load(mixed_bundle/'unsafe-interface.json')
    assert raw_interface['safe_export'] is False and load(mixed_bundle/'admission.json')['contains_unsafe_imports']
    assert [r['alias'] for r in raw_interface['imports']] == ['Raw']
    assert [r['alias'] for r in load(mixed_bundle/'interface.json')['imports']] == ['Reviewed']
    assert all(v['status']=='unsafe' and v['effect']=='unsafe_foreign' for v in raw_interface['exports'].values())
    mixed.write_text(mixed.read_text().replace('unsafe { Raw.sub_i64(43,1) }', 'Raw.sub_i64(43,1)'))
    p = run([*build, mixed, '-o', d/'mixed-rejected'], ok=False)
    assert p.returncode and 'ON1X-KERNEL-CONTEXT' in p.stderr and not (d/'mixed-rejected').exists(), p
    # No publication overwrites, even an empty directory.
    empty = d/'existing'
    empty.mkdir()
    for output in (bundle, empty):
        p = run([*build, app, '-o', output], ok=False)
        assert p.returncode and 'fresh output' in p.stderr, p
    assert digest(bundle/'program') == admission['artifacts']['program']

    def denied(command, diagnostic):
        destination = d/'must-not-publish'
        p = run([*command, app, '-o', destination], ok=False)
        assert p.returncode and diagnostic in p.stderr and not destination.exists(), p
        assert not list(d.glob('.kernel-admission-*'))
        return p

    denied([*common, '--kernel-bind'], 'ON1X-KERNEL-NOT-ADMITTED')
    denied([*build, '--cflag=-DMULIB_IMPL_CLAMP'], 'permits only')
    saved = review.read_bytes()
    review.write_bytes(saved.replace(b'Ephemeral', b'Untrusted'))
    denied(build, 'signature verification failed')
    review.write_bytes(saved)
    other = d/'other.pem'
    run(['openssl', 'genpkey', '-algorithm', 'ED25519', '-out', other])
    other_public = d/'other.der'
    run(['openssl', 'pkey', '-in', other, '-pubout', '-outform', 'DER', '-out', other_public])
    denied([*build, '--kernel-review-key', other_public], 'signature verification failed')
    denied([*build, '--kernel-review-key', private], 'Ed25519 DER public key')
    # Even a genuine signature cannot approve stale/different inputs.
    data = load(review)
    data['subject']['profile'] = 'reviewed-clamp-i64/v1'
    del data['subject']['symbols']['i64']
    review.write_text(json.dumps(data))
    sign()
    denied(build, 'does not match')
    data = json.loads(saved)
    data['subject']['header_revision'] = 'sha256:'+'0'*64
    review.write_text(json.dumps(data))
    sign()
    denied(build, 'does not match')
    review.write_text(saved.decode().replace('"schema":', '"schema":"forged","schema":', 1))
    sign()
    denied(build, 'duplicate JSON key')
    review.write_bytes(saved)
    sign()
    denied([*build, '--cflag=-O1'], 'does not match')

    # Probe compiler-side member, type, arity, macro and escape boundaries.
    for source, error in (
        ('println(C.add_i64(true,2));', 'expected i64, got bool'),
        ('println(C.add_i64(1));', 'wrong number of arguments'),
        ('println(C.unknown(1,2,3));', 'ON1X-KERNEL-MEMBER'),
        ('println(C.i64(1,2));', 'wrong number of arguments'),
        ('let f=C.add_i64;', 'ON1X-KERNEL-DIRECT'),
        ('defer println(C.add_i64(1,2));', 'ON1X-KERNEL-PROFILE'),
    ):
        bad = d/'bad.on1x'
        bad.write_text('import kernel mulib.clamp as C;\nfn main(){'+source+'}\n')
        p = run([compiler, '--no-prelude', '--emit-c', '--kernel-c11', '--kernel-stage', bad], ok=False)
        assert p.returncode and error in p.stderr and not p.stdout, p
    p = run([compiler, '--no-prelude', '--emit-c', '--kernel-c11', app], ok=False)
    assert p.returncode and 'ON1X-KERNEL-NOT-ADMITTED' in p.stderr, p
    for flag in ('--memory-check', '--verify-types', '--emit-ir'):
        p = run([compiler, '--emit-c', '--kernel-c11', '--kernel-stage', flag, app], ok=False)
        assert p.returncode, p

    # Mutated canonical metadata must never acquire representability by omission.
    spec = importlib.util.spec_from_file_location('exo', root/'tools/on1x-exotract.py')
    exo = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(exo)
    report = load(bundle/'discovery.json')
    sidecar = load(root/'contracts-exoml/clamp.exocontract.json')
    sidecar['functions'][0]['invented_obligation'] = 'must never be discarded'
    try:
        exo.require_lossless_contract(sidecar, report['modules']['mulib.clamp']['contract'])
    except ValueError as error:
        assert 'discarded ExoML fields' in str(error)
    else:
        raise AssertionError('accepted a sidecar field silently discarded by the SDK')
    for change in (
        lambda f: f['effects'].update(unknown=True),
        lambda f: f['effects'].update(effects=['allocate']),
        lambda f: f['return'].update(kind='owned'),
        lambda f: f['parameters'][0].update(ownership='borrowed_immutable'),
        lambda f: f['parameters'][0].update(length_parameter=1),
        lambda f: f.update(callbacks=[{}]),
        lambda f: f.update(unrecognized_obligation=True),
        lambda f: f['concurrency'].update(thread_safety='thread_affine'),
        lambda f: f['errors'].update(kind='int_code'),
    ):
        bad = copy.deepcopy(report)
        change(bad['modules']['mulib.clamp']['contract']['functions'][0])
        try:
            exo.review_candidate(bad)
        except ValueError as error:
            assert 'NOT-REPRESENTABLE' in str(error), error
        else:
            raise AssertionError('accepted unsupported contract')
    # A signed review never widens ABI representability.
    for change in (
        lambda f: f.update(variadic=True),
        lambda f: f.update(calling_convention=2),
        lambda f: f['arguments'].pop(),
    ):
        bad = copy.deepcopy(report)
        graph = bad['modules']['mulib.clamp']['metadata']['cdecl']
        declaration = next(f for f in graph['declarations'] if f['name'] == 'mulib_clamp_i64')
        function = next(t for t in graph['types'] if t['id'] == declaration['type'])
        change(function)
        try:
            exo.clamp_scalar_interface(bad, unsafe=False)
        except ValueError as error:
            assert 'ON1X-KERNEL-ABI' in str(error), error
        else:
            raise AssertionError('accepted incompatible CDecl')

    # Fail the actual executable gate using an explicitly trusted TEST tool.
    # The normal compiler still preprocesses/extracts/compiles every input.
    failing_cc = d/'test-compiler'
    failing_cc.write_text(f'#!{sys.executable}\n'
                          'import pathlib, subprocess, sys\n'
                          f'code=subprocess.call([{str(Path(shutil.which(cc)).resolve())!r},*sys.argv[1:]])\n'
                          'if code==0 and any(a.endswith("/kernel-probe.c") for a in sys.argv):\n'
                          ' p=pathlib.Path(sys.argv[sys.argv.index("-o")+1]);p.write_text("#!/bin/sh\\nexit 37\\n");p.chmod(0o700)\n'
                          'sys.exit(code)\n')
    failing_cc.chmod(0o700)
    failure_subject = d/'failure-subject.json'
    run([*common, '--cc', failing_cc, '--kernel-review-candidate', app, '-o', failure_subject])
    data = json.loads(saved)
    data['subject'] = load(failure_subject)
    review.write_text(json.dumps(data))
    sign()
    assert denied([*build, '--cc', failing_cc], '').returncode == 37

    # Adding a higher-priority module during compiler lowering cannot produce
    # a bundle whose source lock omits the module actually compiled.
    graph = d/'graph'
    fallback = graph/'fallback'
    fallback.mkdir(parents=True)
    entry = graph/'main.on1x'
    entry.write_text('import arithmetic as A;fn main(){println(A.answer());}\n')
    lower = fallback/'arithmetic.on1x'
    lower.write_text('import kernel mulib.clamp as C;fn answer()->i64{return C.add_i64(40,2);}\n')
    shadow = graph/'arithmetic.on1x'
    on1xc_wrapper = d/'test-on1xc'
    on1xc_wrapper.write_text(f'#!{sys.executable}\n'
                            'import pathlib,subprocess,sys\n'
                            'if "--emit-c" in sys.argv:\n'
                            f' pathlib.Path({str(shadow)!r}).write_text({lower.read_text()!r})\n'
                            f'sys.exit(subprocess.call([{str(Path(compiler).resolve())!r},*sys.argv[1:]]))\n')
    on1xc_wrapper.chmod(0o700)
    graph_subject = d/'graph-subject.json'
    graph_flags = ['--on1xc', on1xc_wrapper, '-I', fallback]
    run([*common, *graph_flags, '--kernel-review-candidate', entry, '-o', graph_subject])
    data['subject'] = load(graph_subject)
    review.write_text(json.dumps(data))
    sign()
    destination = d/'graph-must-not-publish'
    p = run([*build, *graph_flags, entry, '-o', destination], ok=False)
    assert p.returncode and 'source graph drift' in p.stderr and not destination.exists(), p

    # An implementation dependency changed during compilation must not replace
    # an already signed input pin with a fresh (unreviewed) digest.
    changed = d/'changed-root'
    changed.mkdir()
    shutil.copyfile(root.parent/'MANIFEST.yaml', changed/'MANIFEST.yaml')
    def link_or_copy(source, target):
        if Path(source).name == 'mulib-clamp.h':
            return shutil.copyfile(source, target)
        try:
            os.link(source, target)
        except OSError:
            shutil.copyfile(source, target)
    for name in ('kernels', 'manual'):
        shutil.copytree(root.parent/name, changed/name, copy_function=link_or_copy, symlinks=True)
    drifting_cc = d/'drifting-compiler'
    drifting_cc.write_text(f'#!{sys.executable}\n'
                          'import pathlib,subprocess,sys\n'
                          'if "-c" in sys.argv and any(a.endswith("/mulib.c") for a in sys.argv):\n'
                          f' p=pathlib.Path({str(changed/"kernels/mulib-clamp.h")!r});p.write_bytes(p.read_bytes()+b"\\n")\n'
                          f'sys.exit(subprocess.call([{str(Path(shutil.which(cc)).resolve())!r},*sys.argv[1:]]))\n')
    drifting_cc.chmod(0o700)
    drift_subject = d/'drift-subject.json'
    drift_flags = ['--mulib-root', changed, '--cc', drifting_cc]
    run([*common, *drift_flags, '--kernel-review-candidate', app, '-o', drift_subject])
    data['subject'] = load(drift_subject)
    review.write_text(json.dumps(data))
    sign()
    denied([*build, *drift_flags], 'native input drift')
    print('Reviewed scalar protocol: real SDK, signed TEST authority, C11 execution and fail-closed gates passed')
