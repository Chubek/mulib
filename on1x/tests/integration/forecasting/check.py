"""Test forecasting, constrained optimization and ephemeral foreign admission."""
import argparse
import json
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile


def run(*command, stdin=None, reject=None):
    proc = subprocess.run([str(item) for item in command], input=stdin,
                          capture_output=True, timeout=120)
    if reject is not None:
        assert proc.returncode and reject.encode() in proc.stderr, proc
    else:
        assert proc.returncode == 0, proc
    return proc.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--on1xc', type=Path, required=True)
    parser.add_argument('--exotract', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--cflag', action='append', default=[])
    args = parser.parse_args()
    tools = ('clang', 'openssl', 'ar')
    if sys.platform != 'linux' or platform.machine() != 'x86_64' or not all(map(shutil.which, tools)):
        print('UNAVAILABLE forecasting integration: requires native Linux x86-64, Clang resource headers, OpenSSL and ar')
        return 77
    here = Path(__file__).resolve().parent
    build = here.parents[2] / 'tools/on1x-build.py'
    resource = run('clang', '-print-resource-dir').decode().strip()
    if not (Path(resource) / 'include/stddef.h').is_file():
        print('UNAVAILABLE forecasting integration: Clang resource headers not found')
        return 77
    common = [sys.executable, '-B', build, '--no-prelude', '--on1xc', args.on1xc.resolve(),
              '--exotract', args.exotract.resolve(), '--cc', args.cc, '--resource-dir', resource,
              *(f'--cflag={flag}' for flag in args.cflag), here / 'main.on1x']
    args.artifacts.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='forecasting-', dir=args.artifacts) as directory:
        d = Path(directory).resolve()
        subject, review, key, public, sig, bundle = [
            d / name for name in ('subject.json', 'review.json', 'private.pem', 'public.der',
                                  'review.sig', 'bundle')]
        run(*common, '--kernel-review-candidate', '-o', subject)
        candidate = json.loads(subject.read_bytes())
        assert candidate['schema'] == 'on1x.kernel-review-subject-set/v1'
        assert set(candidate['modules']) == {'mulib.histprice', 'mulib.priceopt'}
        run(*common, '--kernel-bind', '-o', bundle, reject='ON1X-KERNEL-NOT-ADMITTED')
        assert not bundle.exists()

        review.write_text(json.dumps(dict(schema='on1x.kernel-review/v1', reviewer='TEST ONLY',
                                          review_reference='Ephemeral foreign-admission test; not an audit',
                                          subject=candidate)), encoding='utf-8')
        run('openssl', 'genpkey', '-algorithm', 'ED25519', '-out', key)
        run('openssl', 'pkey', '-in', key, '-pubout', '-outform', 'DER', '-out', public)
        run('openssl', 'pkeyutl', '-sign', '-inkey', key, '-rawin', '-in', review, '-out', sig)
        key.unlink()
        run(*common, '--kernel-bind', '--kernel-review', review, '--kernel-review-key', public,
            '--kernel-review-signature', sig, '-o', bundle)
        interface = json.loads((bundle / 'interface.json').read_bytes())
        assert interface['safe_export'] is True
        assert set(interface['modules']) == {'mulib.histprice', 'mulib.priceopt'}
        for module, names in (('mulib.histprice', ['ema', 'holt']), ('mulib.priceopt', ['optimize'])):
            for name in names:
                assert interface['modules'][module]['exports'][name]['status'] == 'admitted'
        assert run('ar', 't', bundle / 'libmulib.a') == b'mulib.o\n'

        def result(text):
            return run(bundle / 'program', stdin=text).decode().splitlines()

        trend = result(b'trend')
        # EMA(alpha=1/2) over [10,12,14] is 12.5; Holt continues slope 2.
        # Independently, (x-4)(40-x) is maximized at x=22 over [4,30].
        assert trend[:2] == ['ok', 'holt'] and trend[5] == 'optimum', trend
        assert list(map(float.fromhex, trend[2:5])) == [6.5, 0, 22], trend
        assert list(map(float.fromhex, trend[6:9])) == [22, 324, 18], trend
        assert trend[9:] == ['27'], trend
        flat = result(b'flat')
        assert flat[:2] == ['ok', 'ema'] and flat[5:] == trend[5:], flat
        assert list(map(float.fromhex, flat[2:5])) == [1, 1, 10.25], flat
        # Holdout data changes neither model selection nor the independent
        # optimization result. Refit happens only after the frozen evaluation.
        changed = result(b'holdout')
        assert changed[:2] == ['ok', 'holt'], changed
        assert list(map(float.fromhex, changed[2:5])) == [3.5, 3, 18.125], changed
        assert changed[5:] == trend[5:], changed
        capped = result(b'cap')
        assert capped[:5] == trend[:5] and capped[5] == 'optimum', capped
        assert list(map(float.fromhex, capped[6:9])) == [18, 308, 22], capped
        assert capped[9:] == ['15'], capped
        rejected = result(b'infeasible')
        assert rejected[:5] == trend[:5] and rejected[5:] == ['optimization-rejected'], rejected
        assert result(b'descending') == ['forecast-rejected']
        for wire in (b'', b'unknown', b'trend\n', b'x' * 300):
            assert result(wire) == ['invalid-case'], wire
        # The canonical stdin primitive rejects NUL before source evaluation.
        for wire in (b'tr\x00end', b'flat\x00'):
            assert run(bundle / 'program', stdin=wire, reject='NUL-free text ABI') == b''
    print('PASS forecasting: On1x/HIR→C11, test-reviewed imports, analytical MAE, constrained optimum, rejection gates')
    return 0


if __name__ == '__main__':
    sys.exit(main())
