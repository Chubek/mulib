"""Qualify offline catalogue selection using the reviewed static PriceOpt C11 binding."""

import argparse
from fractions import Fraction
from hashlib import sha256
import json
import os
from pathlib import Path
import platform
import random
import shutil
import subprocess
import sys
import tempfile


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
MODULE = 'mulib.priceopt'
PROFILE = 'reviewed-priceopt-f64/v1'
SOURCE = '''\
import kernel mulib.priceopt as P;

fn emit(name: string, answer: P.Result<P.Decision, P.Error>) {
    println(name);
    match answer with {
        Ok(row) => {
            println("selected");
            println(row["price"]);
            println(row["score"]);
            println(row["expected_demand"]);
            println(row["evaluations"]);
        },
        Err(Rejected()) => { println("rejected"); }
    };
}

fn main() {
    let limits = record<P.Constraints>{
        min_price: 3.0, max_price: 22.0, unit_cost: 4.0, min_unit_margin: 2.0,
        previous_price: 12.0, max_relative_change: 0.5, tick: 2.0
    };
    let profit = variant<P.Objective>(Profit, record{});
    let revenue = variant<P.Objective>(Revenue, record{});
'''


def run(env, cwd, *command, reject=None):
    argv = [str(item) for item in command]
    proc = subprocess.run(argv, env=env, cwd=cwd, capture_output=True, text=True, timeout=90)
    if reject is not None:
        assert proc.returncode > 0 and reject in proc.stderr, (argv, proc)
    else:
        assert proc.returncode == 0, (argv, proc)
    return proc.stdout


def expected(prices, units, objective):
    # All inputs are dyadic. For the chosen constraints the margin and movement
    # bands intersect at [6, 18]; only even integral prices are eligible.
    # Minimize (-score, price), using exact arithmetic rather than MuLIB's search.
    if not prices or len(prices) != len(units) or min(prices + units) < 0:
        return None
    offset = 4 if objective == 'profit' else 0
    scores = [(u * (offset - p), p, u) for p, u in zip(prices, units)
              if p in range(6, 19, 2)]
    if not scores:
        return None
    minus_score, price, demand = min(scores, key=lambda row: row[:2])
    return price, -minus_score, demand, len(scores)


def corpus():
    yield 'catalogue', [18, 12, 11, 8, 4, 20], [5, 8, 999, 14, 999, 999]
    yield 'tie-high-first', [12, 8, 18], [7, 14, 0]
    yield 'tie-low-first', [8, 12, 18], [14, 7, 0]
    yield 'band-endpoints', [18, 6, Fraction(15, 2)], [2, 20, 999]
    yield 'zero-demand', [18, 6, 8], [0, 0, 0]
    yield 'no-eligible-price', [4, 9, 20], [20, 30, 40]
    yield 'late-invalid-demand', [10, 12], [8, -1]
    yield 'invalid-outside-band', [10, -2], [8, 20]
    yield 'length-mismatch', [6, 8], [20]
    yield 'empty', [], []
    yield 'full-capacity', [6] * 255 + [18], [0] * 255 + [4]
    rng = random.Random(0xCA7A10)
    for i in range(56):
        size = rng.randrange(1, 20)
        yield (f'seeded-{i:02}', [Fraction(rng.randrange(4, 49), 2) for _ in range(size)],
               [Fraction(rng.randrange(0, 61), 2) for _ in range(size)])


def programme(path):
    def array(seq):
        return 'array[' + ', '.join(str(float(v)) for v in seq) + ']'

    lines = [SOURCE]
    cases = {}
    for i, (name, prices, units) in enumerate(corpus()):
        # Invalid padding on either side is *not* in these snapshots. Differing
        # offsets/capacities make swapping lengths or checking padding observable.
        po, uo = (0, 0) if name == 'full-capacity' else (1, 2)
        p = [-99] * po + prices + [-99] * po
        u = [-99] * uo + units + [-99] * uo
        lines += [f'    let p{i} = {array(p)};', f'    let u{i} = {array(u)};']
        for objective in ('profit', 'revenue'):
            case = f'{name}:{objective}'
            lines.append(f'    emit("{case}", P.candidates(p{i}[{po}:{po + len(prices)}], '
                         f'u{i}[{uo}:{uo + len(units)}], limits, {objective}));')
            cases[case] = expected(prices, units, objective)
    path.write_text('\n'.join([*lines, '}', '']), encoding='ascii')
    return cases


def check_output(stdout, cases):
    lines = iter(stdout.splitlines())
    for name, result in cases.items():
        assert next(lines, None) == name, name
        status = next(lines, None)
        if result is None:
            assert status == 'rejected', (name, status)
        else:
            assert status == 'selected', (name, status)
            actual = tuple(Fraction.from_float(float.fromhex(next(lines))) for _ in range(3))
            actual += (int(next(lines)),)
            assert actual == result, (name, actual, result)
    assert next(lines, None) is None, 'unexpected trailing application output'


def check_contract(bundle, subject):
    module = json.loads((bundle / 'discovery.json').read_bytes())['modules'][MODULE]
    contract = module['contract']
    canonical = json.dumps(contract, sort_keys=True, separators=(',', ':')) + '\n'
    assert sha256(canonical.encode('ascii')).hexdigest() == subject['contract_sha256']
    assert contract['bundle_provenance']['review_status'] == 'unreviewed'
    fn = next(f for f in contract['functions'] if f['c_name'] == 'mulib_priceopt_candidates')
    params = fn['parameters']
    assert [(p['ownership'], p['length_parameter']) for p in params] == [
        ('borrowed_immutable', 2), ('borrowed_immutable', 2), ('scalar', None),
        ('borrowed_immutable', None), ('scalar', None), ('out', None)]
    assert all(params[i]['lifetime']['region_name'] == 'call' for i in (0, 1, 3, 5))
    assert fn['errors']['kind'] == 'boolean_return' and fn['errors']['out_valid_on_failure'] is False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--on1xc', type=Path, required=True)
    parser.add_argument('--exotract', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--cflag', action='append', default=[])
    args = parser.parse_args()
    tools = ('clang', 'openssl', 'ar')
    if (sys.platform != 'linux' or platform.machine() != 'x86_64'
            or not all(map(shutil.which, tools))):
        print('UNAVAILABLE PriceOpt: needs native Linux x86-64, Clang resource headers, OpenSSL and ar')
        return 77
    compiler, extractor = (path.resolve(strict=True) for path in (args.on1xc, args.exotract))
    # Do not use ambient package roots, review/runtime choices, or interposed libraries.
    env = {k: v for k, v in os.environ.items() if not k.startswith(('ON1X_', 'MULIB_', 'LD_'))}
    args.artifacts.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='price-candidates-', dir=args.artifacts) as tmp:
        work = Path(tmp).resolve()
        env['TMPDIR'] = str(work)

        def invoke(*command, **kw):
            return run(env, work, *command, **kw)

        resource = Path(invoke('clang', '-print-resource-dir').strip())
        if not (resource / 'include/stddef.h').is_file():
            print('UNAVAILABLE PriceOpt: Clang resource headers not found')
            return 77
        source = work / 'catalogue.on1x'
        cases = programme(source)
        common = [sys.executable, '-B', ROOT / 'tools/on1x-build.py', source, '--no-prelude',
                  '--on1xc', compiler, '--exotract', extractor, '--cc', args.cc,
                  '--resource-dir', resource, *(f'--cflag={flag}' for flag in args.cflag)]
        # An ordinary compiler invocation or a source-level unsafe spelling
        # cannot authorize this reviewed profile.
        denied = work / 'denied'
        for mode in ('--emit-ir', '--run-vm'):
            invoke(compiler, mode, '--no-prelude', source, reject='ON1X-KERNEL-NOT-ADMITTED')
        unsafe = work / 'unsafe.on1x'
        unsafe.write_text('unsafe import kernel mulib.priceopt as P;\nfn main() {}\n', encoding='ascii')
        invoke(compiler, '--emit-c', '--kernel-c11', '--no-prelude', unsafe, '-o', denied,
               reject='ON1X-KERNEL-UNSAFE-UNSUPPORTED')
        assert not denied.exists()

        discovery = work / 'inspection.json'
        invoke(*common, '--inspect-kernels', '-o', discovery)
        report = json.loads(discovery.read_bytes())
        symbol = report['modules'][MODULE]['symbols']['mulib_priceopt_candidates']
        assert symbol['callable'] is False, symbol

        subject_file, review, signature, bundle = [
            work / name for name in ('subject.json', 'review.json', 'signature', 'bundle')]
        invoke(*common, '--kernel-review-candidate', '-o', subject_file)
        subject = json.loads(subject_file.read_bytes())
        assert (subject['schema'], subject['module'], subject['profile']) == (
            'on1x.kernel-review-subject/v1', MODULE, PROFILE)
        assert set(subject['symbols']) == {'candidates', 'optimize'}
        assert subject['symbols']['candidates']['status'] == 'missing_review'
        invoke(*common, '--kernel-bind', '-o', denied, reject='ON1X-KERNEL-NOT-ADMITTED')
        assert not denied.exists()

        review.write_text(json.dumps(dict(schema='on1x.kernel-review/v1', reviewer='TEST ONLY',
                                          review_reference='Ephemeral authority; not an audit',
                                          subject=subject)), encoding='ascii')
        key, public, other_key, other_public = [
            work / name for name in ('key.pem', 'key.der', 'other.pem', 'other.der')]
        for private, der in ((key, public), (other_key, other_public)):
            invoke('openssl', 'genpkey', '-algorithm', 'ED25519', '-out', private)
            invoke('openssl', 'pkey', '-in', private, '-pubout', '-outform', 'DER', '-out', der)
        invoke('openssl', 'pkeyutl', '-sign', '-inkey', key, '-rawin', '-in', review, '-out', signature)
        key.unlink()
        other_key.unlink()
        reviewed = [*common, '--kernel-bind', '--kernel-review', review, '--kernel-review-signature', signature]
        invoke(*reviewed, '--kernel-review-key', other_public, '-o', denied,
               reject='independent review signature verification failed')
        assert not denied.exists()
        authorized = [*reviewed, '--kernel-review-key', public]
        invoke(*authorized, '--cflag=-g', '-o', denied,
               reject='independent review does not match the current binding inputs')
        assert not denied.exists()
        invoke(*authorized, '-o', bundle)
        check_contract(bundle, subject)

        plan = json.loads((bundle / 'build-plan.json').read_bytes())
        interface = json.loads((bundle / 'interface.json').read_bytes())
        admission = json.loads((bundle / 'admission.json').read_bytes())
        assert plan['implementation_macros'] == ['MULIB_IMPL_PRICEOPT']
        assert plan['link_libraries'] == ['m']
        assert invoke('ar', 't', bundle / 'libmulib.a') == 'mulib.o\n'
        assert interface['profile'] == PROFILE and interface['safe_export'] is True
        export = interface['exports']['candidates']
        assert export['status'] == 'admitted' and export['result'] == 'Result<Decision,Error>'
        assert interface['rejected']['ucb']['status'] == 'unsupported'
        assert admission['review']['subject'] == subject and admission['executable_gate']['passed'] is True
        for name in ('program', 'kernel-adapter.o', 'libmulib.a', 'discovery.json', 'review.json'):
            digest = sha256((bundle / name).read_bytes()).hexdigest()
            assert digest == admission['artifacts'][name], name
            print(f'{name} sha256={digest}')

        child = work / 'empty-cwd'
        child.mkdir()
        check_output(run({}, child, bundle / 'program'), cases)
        probe = work / 'abi-probe'
        invoke(args.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
               *args.cflag, '-I', ROOT.parent / 'kernels', HERE / 'abi_probe.c',
               bundle / 'kernel-adapter.o', bundle / 'libmulib.a', '-lm', '-o', probe)
        invoke(probe)
        rejected = sum(result is None for result in cases.values())
        print(f'PASS PriceOpt: {len(cases)} exact-oracle C11 cases ({rejected} rejected);'
              ' independent test authority and real archive/adapter ABI and status probes')
    return 0


if __name__ == '__main__':
    sys.exit(main())
