"""KernelBind must link and probe the same Cephyr DSO that it publishes."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

exo, compiler, cc = sys.argv[1:]
root = Path(__file__).resolve().parents[1]
if not shutil.which('clang') or not shutil.which('openssl'):
    sys.exit(77)
resource = subprocess.check_output(['clang', '-print-resource-dir'], text=True).strip()
base = [sys.executable, root/'tools/on1x-build.py', '--no-prelude',
        '--on1xc', compiler, '--exotract', exo, '--cc', cc, '--resource-dir', resource]

def run(*cmd, ok=True):
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    if ok:
        assert result.returncode == 0, result.stderr
    else:
        assert result.returncode != 0, result.stdout
    return result

with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    src, raw, safe = [work/n for n in ('main.on1x', 'raw', 'safe')]
    original = root/'examples/kernel/clamp.on1x'
    static = work/'static'
    run(*base, '--kernel-bind', original, '-o', static)
    expected = run(static).stdout
    def source(name):
        src.write_text((root/'examples/kernel'/name).read_text().replace('import kernel ', 'import kernel[dynlib] '))
    source('clamp.on1x')
    run(*base, '--kernel-bind', src, '-o', raw)
    moved = raw.rename(work/'relocated')
    assert run(moved/'program').stdout == expected
    soname = 'libon1x_kernels.so'
    assert soname in run('readelf', '-d', moved/'program').stdout
    (moved/soname).unlink()
    assert soname in run(moved/'program', ok=False).stderr
    src.write_text(src.read_text() + '\nunsafe import kernel mulib.clamp as AlsoStatic;\n')
    assert 'conflicting static and dynlib' in run(*base, '--kernel-bind', src, '-o', raw, ok=False).stderr
    source('clamp_safe.on1x')
    run(*base, '--kernel-bind', src, '-o', safe, ok=False)
    assert not safe.exists()
    subject, review, pem, key, sig = [work/n for n in ('subject', 'review', 'pem', 'der', 'sig')]
    run(*base, '--kernel-review-candidate', src, '-o', subject)
    review.write_text(json.dumps(dict(schema='on1x.kernel-review/v1', reviewer='Ephemeral test authority',
        review_reference='TEST ONLY: not a production review', subject=json.loads(subject.read_bytes()))))
    run('openssl', 'genpkey', '-algorithm', 'ED25519', '-out', pem)
    run('openssl', 'pkey', '-in', pem, '-pubout', '-outform', 'DER', '-out', key)
    run('openssl', 'pkeyutl', '-sign', '-inkey', pem, '-rawin', '-in', review, '-out', sig)
    run(*base, '--kernel-bind', src, '--kernel-review', review, '--kernel-review-key', key,
        '--kernel-review-signature', sig, '-o', safe)
    assert run(safe/'program').stdout == expected
    admission = json.loads((safe/'admission.json').read_bytes())
    digest = hashlib.sha256((safe/soname).read_bytes()).hexdigest()
    assert admission['artifacts'][soname] == json.loads(subject.read_bytes())['dynlib']['sha256'] == digest
    assert admission['executable_gate']['passed'] and admission['executable_gate']['comparisons'] == 2670
