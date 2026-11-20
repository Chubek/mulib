"""Build the real On1x fixture with only ephemeral integration-test approval."""
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
        print('UNAVAILABLE Shylock: requires native Linux x86-64, Clang resource headers, OpenSSL and ar')
        return 77
    here = Path(__file__).resolve().parent
    build = here.parents[2] / 'on1x/tools/on1x-build.py'
    resource = run('clang', '-print-resource-dir').decode().strip()
    if not (Path(resource) / 'include/stddef.h').is_file():
        print('UNAVAILABLE Shylock: Clang resource headers not found')
        return 77
    common = [sys.executable, '-B', build, '--no-prelude', '--on1xc', args.on1xc.resolve(),
              '--exotract', args.exotract.resolve(), '--cc', args.cc, '--resource-dir', resource,
              *(f'--cflag={flag}' for flag in args.cflag), here / 'main.on1x']
    args.artifacts.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='shylock-', dir=args.artifacts) as directory:
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
                                          review_reference='Ephemeral workload fixture; not an audit',
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

        trend = (here / 'trend.csv').read_bytes()
        flat = (here / 'flat.csv').read_bytes()
        quoted = result(trend)
        # EMA(alpha=1/2) over [10,12,14] is 12.5; Holt continues the exact
        # slope 2. Grid profit is (p-4)(40-p), maximized at p=22 in [4,22].
        assert quoted[:2] == ['ok', 'holt'] and quoted[5] == 'quote', quoted
        assert list(map(float.fromhex, quoted[2:5])) == [6.5, 0, 22], quoted
        assert list(map(float.fromhex, quoted[6:9])) == [22, 324, 18], quoted
        assert quoted[9:] == ['19'], quoted
        no_demand = result(flat)
        assert no_demand[:2] == ['ok', 'ema'] and no_demand[5:] == ['no-demand'], no_demand
        assert list(map(float.fromhex, no_demand[2:5])) == [1, 1, 10.25], no_demand

        # Alter only the holdout. The chosen model and its original predictions
        # must not change. The re-fit forecasts 18.125, below the unconstrained
        # optimum, so the quote must use the cap and select grid price 18.
        changed = result(trend.replace(b'14,1800\n15,2000', b'14,1600\n15,1600'))
        assert changed[:2] == ['ok', 'holt'], changed
        assert list(map(float.fromhex, changed[2:5])) == [3.5, 3, 18.125], changed
        assert changed[5] == 'quote' and list(map(float.fromhex, changed[6:9])) == [18, 308, 22], changed
        assert changed[9:] == ['15'], changed
        rejected = result(trend.replace(b',400\n', b',2300\n'))
        assert rejected[:5] == quoted[:5] and rejected[5:] == ['optimization-rejected'], rejected
        # A valid declining series causes a negative Holt projection,
        # which the bound kernel rejects. No forecast or quote gets published.
        descending = trend.replace(b'11,1200\n12,1400', b'11,800\n12,600')
        descending = descending.replace(b'13,1600\n14,1800\n15,2000', b'13,400\n14,200\n15,0')
        assert result(descending) == ['forecast-rejected']
        invalid = (b'', trend.replace(b'USD', b'EUR'), trend.replace(b'USD,2', b'USD,3'),
                   trend.replace(b'11,', b'10,'), trend.replace(b'13,', b'14,'),
                   trend.replace(b'1600', b'-1600'), trend.replace(b'1600', b'1234567'),
                   trend[:-1], flat + b'\n', flat + b'x' * 300)
        for wire in invalid:
            assert result(wire) == ['invalid-fixture'], wire
        # The canonical stdin text primitive rejects NUL before app parsing.
        for wire in (trend.replace(b'1600', b'16\x0000'), flat + b'\x00'):
            assert run(bundle / 'program', stdin=wire, reject='NUL-free text ABI') == b''
    print('PASS Shylock: On1x/HIR→C11, test-reviewed imports, analytical MAE and quote, rejection gates')
    return 0


if __name__ == '__main__':
    sys.exit(main())
