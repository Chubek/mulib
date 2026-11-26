"""Real KernelBind DSO builds, closure placement and signed review gates."""
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile

exo, compiler, cc = sys.argv[1:]
root = Path(__file__).resolve().parents[1]
examples = root/'examples/kernel'
if (sys.platform != 'linux' or platform.machine() != 'x86_64'
        or not all(shutil.which(tool) for tool in ('clang', 'openssl', 'readelf', 'nm'))):
    sys.exit(77)
resource = subprocess.check_output(['clang', '-print-resource-dir'], text=True).strip()
base = [sys.executable, root/'tools/on1x-build.py', '--no-prelude',
        '--on1xc', compiler, '--exotract', exo, '--cc', cc, '--resource-dir', resource]
host = [*base, '--kernel-dynlib-compiler=host']
has_cephyr = Path(compiler).resolve().with_name('on1x-cephyr-kernel').is_file()
selected = base if has_cephyr else host
soname = 'libon1x_kernels.so'

def run(*cmd, ok=True):
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    if ok:
        assert result.returncode == 0, result.stderr
    else:
        assert result.returncode != 0, result.stdout
    return result

def load(path):
    return json.loads(path.read_bytes())

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def dynamic(source):
    return source.read_text().replace('import kernel ', 'import kernel[dynlib] ')

with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    src, raw, safe = [work/n for n in ('main.on1x', 'raw', 'safe')]
    original = examples/'clamp.on1x'
    static = work/'static'
    run(*base, '--kernel-bind', original, '-o', static)
    expected = run(static).stdout
    src.write_text(dynamic(original))
    if not has_cephyr:
        p = run(*base, '--kernel-bind', src, '-o', raw, ok=False)
        assert 'ON1X_WITH_CEPHYR' in p.stderr and not raw.exists(), p
    run(*selected, '--kernel-bind', src, '-o', raw)
    moved = raw.rename(work/'relocated')
    assert run(moved/'program').stdout == expected
    assert soname in run('readelf', '-d', moved/'program').stdout
    (moved/soname).unlink()
    assert soname in run(moved/'program', ok=False).stderr
    src.write_text(src.read_text() + '\nunsafe import kernel mulib.clamp as AlsoStatic;\n')
    assert 'conflicting static and dynlib' in run(*base, '--kernel-bind', src, '-o', raw, ok=False).stderr
    assert 'requires at least one' in run(*host, '--kernel-bind', original, '-o', raw, ok=False).stderr
    src.write_text(dynamic(examples/'clamp_safe.on1x'))
    assert 'ON1X-KERNEL-NOT-ADMITTED' in run(*selected, '--kernel-bind', src, '-o', safe, ok=False).stderr
    assert not safe.exists()
    subject, review, pem, key, sig = [work/n for n in ('subject', 'review', 'pem', 'der', 'sig')]
    run('openssl', 'genpkey', '-algorithm', 'ED25519', '-out', pem)
    run('openssl', 'pkey', '-in', pem, '-pubout', '-outform', 'DER', '-out', key)

    def sign(subject):
        review.write_text(json.dumps(dict(schema='on1x.kernel-review/v1', reviewer='Ephemeral test authority',
            review_reference='TEST ONLY: not a production review', subject=subject)))
        run('openssl', 'pkeyutl', '-sign', '-inkey', pem, '-rawin', '-in', review, '-out', sig)

    reviewed = ['--kernel-bind', '--kernel-review', review, '--kernel-review-key', key,
                '--kernel-review-signature', sig]
    run(*selected, '--kernel-review-candidate', src, '-o', subject)
    sign(load(subject))
    run(*selected, *reviewed, src, '-o', safe)
    assert run(safe/'program').stdout == expected
    admission = load(safe/'admission.json')
    assert load(subject)['dynlib']['backend'] == ('cephyr' if has_cephyr else 'host')
    assert admission['artifacts'][soname] == load(subject)['dynlib']['sha256'] == digest(safe/soname)
    assert admission['executable_gate']['passed'] and admission['executable_gate']['comparisons'] == 2670
    if has_cephyr:
        p = run(*host, *reviewed, src, '-o', raw, ok=False)
        assert 'does not match' in p.stderr and not raw.exists(), p

    # The unsafe framed C11 resource adapter has a static ABI only.
    framed = examples/'framed_session.on1x'
    framed_static = work/'framed-static'
    run(*base, '--kernel-bind', framed, '-o', framed_static)
    assert run(framed_static).stdout == '42\n297\n'
    src.write_text(dynamic(framed))
    for driver in (base, host) if has_cephyr else (host,):
        p = run(*driver, '--kernel-bind', src, '-o', raw, ok=False)
        assert 'ON1X-FFI-ABI: mulib.ipc_framed has no dynlib resource ABI' in p.stderr, p
        assert not raw.exists(), p

    # Host PIC never admits discovery-only kernels.
    src.write_text('unsafe import kernel[dynlib] mulib.money as Money;\nfn main(){}\n')
    p = run(*host, '--kernel-bind', src, '-o', raw, ok=False)
    assert 'ON1X-KERNEL-UNSAFE-UNSUPPORTED' in p.stderr and not raw.exists(), p

    # Exercise all-dynamic and split plans using the existing pricing example
    # with an ordinary module dependency that itself imports histprice.
    pricing = examples/'pricing'
    app, forecasts = work/'pricing.on1x', work/'forecasts.on1x'
    app.write_text((pricing/'main.on1x').read_text())
    forecasts.write_text((pricing/'forecasts.on1x').read_text())
    price_subject, price_static = work/'price-static.json', work/'price-static'
    run(*base, '--kernel-review-candidate', app, '-o', price_subject)
    sign(load(price_subject))
    run(*base, *reviewed, app, '-o', price_static)
    pricing_stdout = run(price_static/'program').stdout
    assert pricing_stdout.endswith('0x1.4000000000000p+3\n3\n0\n')
    forecasts.write_text(dynamic(pricing/'forecasts.on1x'))
    app.write_text(dynamic(pricing/'main.on1x'))
    rejected = work/'rejected'
    p = run(*base, '--kernel-review-candidate', app, '-o', rejected, ok=False)
    assert ('Cephyr' in p.stderr or 'ON1X_WITH_CEPHYR' in p.stderr) and \
        '--kernel-dynlib-compiler=host' in p.stderr and not rejected.exists(), p

    for split in (False, True):
        if split:
            app.write_text((pricing/'main.on1x').read_text())
        candidate_path = work/f'price-{split}.json'
        run(*host, '--kernel-review-candidate', app, '-o', candidate_path)
        candidate = load(candidate_path)
        modules = candidate['modules']
        identity = modules['mulib.histprice']['dynlib']
        assert identity == modules['mulib.priceopt']['dynlib']
        assert identity['backend'] == 'host' and identity['link_libraries'] == ['m']
        macros = ['MULIB_IMPL_STATSPROC', 'MULIB_IMPL_HISTPRICE']
        assert identity['implementation_macros'] == macros + ([] if split else ['MULIB_IMPL_PRICEOPT'])
        assert identity['modules'] == ['mulib.histprice'] + ([] if split else ['mulib.priceopt'])
        if not split:
            candidate_again = work/'price-again.json'
            run(*host, '--kernel-review-candidate', app, '-o', candidate_again)
            assert candidate_path.read_bytes() == candidate_again.read_bytes()
            sign(load(price_subject))
            p = run(*host, *reviewed, app, '-o', rejected, ok=False)
            assert 'does not match' in p.stderr and not rejected.exists(), p
            tampered = load(candidate_path)
            for member in tampered['modules'].values():
                member['dynlib']['sha256'] = '0'*64
            sign(tampered)
            p = run(*host, *reviewed, app, '-o', rejected, ok=False)
            assert 'does not match' in p.stderr and not rejected.exists(), p
        sign(candidate)
        bundle = work/f'price-{split}'
        run(*host, *reviewed, app, '-o', bundle)
        bundle = bundle.rename(work/f'price-deploy-{split}')
        assert run(bundle/'program').stdout == pricing_stdout
        admission = load(bundle/'admission.json')
        assert admission['artifacts'][soname] == identity['sha256'] == digest(bundle/soname)
        assert admission['executable_gate']['passed'] and len(admission['executable_gate']['profiles']) == 2
        plan = load(bundle/'build-plan.json')
        partitions = plan['implementation_partitions']
        assert plan['implementation_translation_units'] == (2 if split else 1)
        assert partitions['dynlib']['implementation_macros'] == identity['implementation_macros']
        assert partitions['dynlib']['link_libraries'] == ['m']
        tags = run('readelf', '-d', bundle/soname).stdout
        assert 'libm.so' in tags and '(SONAME)' in tags and soname in tags and 'TEXTREL' not in tags
        program_tags = run('readelf', '-d', bundle/'program').stdout
        assert soname in program_tags and ('libm.so' in program_tags) is split
        defined = run('nm', '-D', '--defined-only', bundle/soname).stdout
        assert 'mulib_histprice_ema' in defined and 'mulib_statsproc_' in defined
        assert ('mulib_priceopt_optimize' in defined) is not split
        archive = bundle/'libmulib.a'
        assert archive.exists() is split
        if split:
            assert partitions['static']['implementation_macros'] == ['MULIB_IMPL_PRICEOPT']
            static_symbols = run('nm', '--defined-only', archive).stdout
            assert 'mulib_priceopt_optimize' in static_symbols
            assert not any(symbol in static_symbols for symbol in ('mulib_histprice_ema', 'mulib_statsproc_'))
        else:
            assert set(partitions) == {'dynlib'}
