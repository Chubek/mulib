#!/usr/bin/env python3
"""Exercise the real forecast binding and its opt-in review gate."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

sdk, compiler, root, artifacts, cc, *flags = sys.argv[1:]
root, artifacts = Path(root), Path(artifacts)
artifacts.mkdir(parents=True, exist_ok=True)
if not sys.platform.startswith('linux') or not all(shutil.which(t) for t in ('clang', 'openssl')):
    print('UNSUPPORTED: forecasts need native Linux, Clang resource headers and OpenSSL')
    sys.exit(77)


def run(*cmd, reject=None, stdin=None):
    p = subprocess.run(list(map(str, cmd)), input=stdin, capture_output=True, text=True, timeout=120)
    if reject is None:
        assert p.returncode == 0, p
    else:
        assert p.returncode != 0 and reject in p.stderr, p
    return p


resource = run('clang', '-print-resource-dir').stdout.strip()
if not (Path(resource)/'include/stddef.h').is_file():
    sys.exit(77)
common = [sys.executable, root/'tools/on1x-build.py', '--no-prelude', '--on1xc', compiler,
          '--exotract', sdk, '--cc', cc, '--resource-dir', resource, *flags]
app = root/'examples/kernel/forecast.on1x'
with tempfile.TemporaryDirectory(dir=artifacts) as temp:
    d = Path(temp)
    subject, review, key, public, signature = (d/name for name in ('subject', 'review', 'key', 'public', 'sig'))
    run(*common, '--kernel-review-candidate', app, '-o', subject)
    candidate = json.loads(subject.read_bytes())
    assert candidate['profile'] == 'reviewed-histprice-f64/v1'
    assert set(candidate['symbols']) == {'ema', 'holt'}
    assert all(s['c_output_type_id'] for s in candidate['symbols'].values())
    review.write_text(json.dumps(dict(schema='on1x.kernel-review/v1', reviewer='TEST ONLY',
                                      review_reference='Ephemeral integration authority; not an audit',
                                      subject=candidate)))
    run('openssl', 'genpkey', '-algorithm', 'ED25519', '-out', key)
    run('openssl', 'pkey', '-in', key, '-pubout', '-outform', 'DER', '-out', public)
    run('openssl', 'pkeyutl', '-sign', '-inkey', key, '-rawin', '-in', review, '-out', signature)
    build = [*common, '--kernel-bind', '--kernel-review', review,
             '--kernel-review-key', public, '--kernel-review-signature', signature]
    denied = d/'denied'
    run(*common, '--kernel-bind', app, '-o', denied, reject='ON1X-KERNEL-NOT-ADMITTED')
    assert not denied.exists()
    bundle = d/'bundle'
    run(*build, app, '-o', bundle)
    p = run(bundle/'program')
    assert p.stdout == '0x1.4000000000000p+3\n0x1.2000000000000p+4\ntrue\n0\n', p
    assert run('ar', 't', bundle/'libmulib.a').stdout == 'mulib.o\n'
    interface = json.loads((bundle/'interface.json').read_bytes())
    plan = json.loads((bundle/'build-plan.json').read_bytes())
    assert interface['safe_export'] is True and interface['rejected']['seasonal']['status'] == 'unsupported'
    assert plan['implementation_macros'] == ['MULIB_IMPL_STATSPROC', 'MULIB_IMPL_HISTPRICE']
    assert all(s['status'] == 'admitted' for s in interface['exports'].values())

    sample = d/'sample.on1x'
    for body, diagnostic in (
        ('let v=array[1.0,2.0];let r=H.ema(v,0.0,1);', 'bounded slice<f64,N>'),
        ('let v=array[1,2];let r=H.ema(v[0:2],0.0,1);', 'expected f64'),
        ('let v=array[1.0,2.0];let r=H.ema(v[0:2],0.0,1u64);', 'expected i64'),
        ('let v=array[' + ','.join(['1.0'] * 257) + '];let r=H.ema(v[0:1],0.0,1);', 'array extent limit exceeded'),
        ('let v=array[1.0,2.0];let r=H.kalman(v[0:2],0.0,1);', 'ON1X-KERNEL-MEMBER'),
        ('let f=H.holt;', 'ON1X-KERNEL-DIRECT'),
        ('let a:H.Missing=1;', 'ON1X-KERNEL-MEMBER'),
    ):
        sample.write_text('import kernel mulib.histprice as H;\nfn main(){'+body+'}\n')
        run(compiler, '--emit-c', '--kernel-c11', '--kernel-stage', '--no-prelude', sample, reject=diagnostic)
    sample.write_text('unsafe import kernel mulib.histprice as H;\nfn main(){}')
    run(compiler, '--emit-c', '--kernel-c11', '--no-prelude', sample, reject='ON1X-KERNEL-UNSAFE-UNSUPPORTED')
    run(compiler, '--emit-c', '--kernel-c11', '--no-prelude', app, reject='ON1X-KERNEL-NOT-ADMITTED')

    # Calls from two scopes resolve the same private nominal forecast types.
    child = d/'child.on1x'
    child.write_text('import kernel mulib.histprice as Child;\n'
                     'fn get()->Child.Result<Child.Forecast,Child.Error>{'
                     'let a=array[2.0,4.0];return Child.ema(a[0:2],1.0,2);}\n')
    sample.write_text('import child as C;\nimport kernel mulib.histprice as Parent;\n'
                      'fn result()->Parent.Result<Parent.Forecast,Parent.Error>{return C.get();}\n'
                      'fn main(){match result() with {Ok(x)=>{println(x["point"]);},Err(_)=>{println(-1);}};}\n')
    run(*build, sample, '-o', d/'cross')
    assert run(d/'cross/program').stdout == '0x1.0000000000000p+2\n'
    sample.write_text('import kernel mulib.histprice as H;\nimport kernel mulib.priceopt as P;\n'
                      'fn bad(r:H.Result<H.Forecast,H.Error>)->P.Result<H.Forecast,P.Error>{return r;}\n'
                      'fn main(){}\n')
    run(compiler, '--emit-c', '--kernel-c11', '--kernel-stage', '--no-prelude', sample,
        reject='incompatible nominal types')

    # Unsupported CDecl drift stays rejected even if a new subject is requested.
    spec = importlib.util.spec_from_file_location('exo', root/'tools/on1x-exotract.py')
    exo = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(exo)
    original = json.loads((bundle/'discovery.json').read_bytes())
    for corrupt in ('offset', 'input', 'failure', 'retain'):
        report = copy.deepcopy(original)
        module = report['modules']['mulib.histprice']
        if corrupt == 'offset':
            graph = module['metadata']['cdecl']
            record = next(t for t in graph['types'] if t['spelling'] == 'struct mulib_histprice_forecast')
            record['fields'][0]['offset_bits'] = 8
        elif corrupt == 'input':
            graph = module['metadata']['cdecl']
            pointer = next(t for t in graph['types'] if t['spelling'] == 'const double *')
            pointee = next(t for t in graph['types'] if t['id'] == pointer['element'])
            pointee['const'] = False
        elif corrupt == 'failure':
            module['contract']['functions'][0]['errors']['out_valid_on_failure'] = True
        else:
            module['contract']['functions'][0]['effects']['effects'].append('escape')
        try:
            exo.review_candidate(report)
        except ValueError as error:
            assert ('ON1X-KERNEL-ABI' if corrupt in ('offset', 'input') else 'ON1X-KERNEL-NOT-REPRESENTABLE') in str(error)
        else:
            raise AssertionError('accepted '+corrupt)

    pricing = root/'examples/kernel/pricing/main.on1x'
    review_set, linked = d/'set.json', d/'linked'
    run(*build, pricing, '-o', linked, reject='does not match')
    assert not linked.exists()
    run(*common, '--kernel-review-candidate', pricing, '-o', review_set)
    subject_set = json.loads(review_set.read_bytes())
    assert subject_set['schema'] == 'on1x.kernel-review-subject-set/v1'
    assert set(subject_set['modules']) == {'mulib.histprice', 'mulib.priceopt'}
    assert set(subject_set['modules']['mulib.priceopt']['symbols']) == {'optimize', 'candidates'}
    review.write_text(json.dumps(dict(schema='on1x.kernel-review/v1', reviewer='TEST ONLY',
                                      review_reference='Ephemeral integration authority; not an audit', subject=subject_set)))
    run('openssl', 'pkeyutl', '-sign', '-inkey', key, '-rawin', '-in', review, '-out', signature)
    run(*build, pricing, '-o', linked)
    p = run(linked/'program')
    assert p.stdout == ('0x1.4000000000000p+3\n0x1.2000000000000p+4\ntrue\n'
                        '0x1.8000000000000p+3\n0x1.0000000000000p+6\n0x1.4000000000000p+3\n3\n0\n'), p
    oracle = d/'oracle'
    run(cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', root.parent/'include',
        *[flag.removeprefix('--cflag=') for flag in flags],
        root/'tests/fixtures/kernel_pricing_oracle.c', linked/'libmulib.a', '-lm', '-o', oracle)
    assert run(oracle).stdout == p.stdout
    edges = root/'tests/fixtures/kernel_numerics_edges.on1x'
    run(*build, edges, '-o', d/'edges')
    assert run(d/'edges/program', stdin='checks').stdout == '1\n2\n3\n4\nnumerical-edges\n'
    trap = run(d/'edges/program', stdin='trap', reject='slice bounds out of range')
    assert (trap.returncode, trap.stdout) == (70, '1\n6\n'), trap
    status_probe = d/'status-probe'
    run(cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', root.parent/'include',
        *[flag.removeprefix('--cflag=') for flag in flags],
        root/'tests/fixtures/kernel_numerics_status_probe.c', linked/'kernel-adapter.o', '-lm', '-o', status_probe)
    run(status_probe)
    assert run('ar', 't', linked/'libmulib.a').stdout == 'mulib.o\n'
    link_plan = json.loads((linked/'build-plan.json').read_bytes())
    assert link_plan['implementation_macros'] == ['MULIB_IMPL_STATSPROC', 'MULIB_IMPL_HISTPRICE', 'MULIB_IMPL_PRICEOPT']
    assert link_plan['link_libraries'] == ['m']
    assert (linked/'kernel-adapter.o').is_file()
    linked_interface = json.loads((linked/'interface.json').read_bytes())
    assert linked_interface['safe_export'] and all(m['safe_export'] for m in linked_interface['modules'].values())

    report = json.loads((linked/'discovery.json').read_bytes())
    abi = report['modules']['mulib.priceopt']['metadata']['cdecl']
    enum = next(t for t in abi['types'] if t['spelling'] == 'enum mulib_priceopt_objective')
    enum['enumerators'][0]['value'] = '999'
    try:
        exo.review_candidate(report)
    except ValueError as error:
        assert 'ON1X-KERNEL-ABI' in str(error), error
    else:
        raise AssertionError('accepted changed objective enum')
print('Reviewed forecasting and optimization: real libraries, one TU, differential and rejection gates passed')
