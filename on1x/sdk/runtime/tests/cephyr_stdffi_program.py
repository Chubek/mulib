"""Build ordinary direct C11 output and execute the Cephyr-backed stdffi calls."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

compiler, cc, work = sys.argv[1:]
driver = Path(__file__).resolve().parents[2] / 'tools/on1x-build.py'
with tempfile.TemporaryDirectory(prefix='cephyr-stdffi-', dir=work) as temp:
    root = Path(temp)
    source, binary, emitted = (root / path for path in ('main.on1x', 'main', 'main.c'))
    build = [sys.executable, driver, source, '--on1xc', compiler, '--cc', cc]
    source.write_text('''\
import foreign stdffi.clamp as Kernel;
fn main() {
    let saturate = Kernel.add_i64;
    println(saturate(9223372036854775807, 1));
    println(Kernel.sub_i64(-9223372036854775808, 1));
    println(Kernel.i64(53, -3, 4));
}
''', encoding='ascii')
    subprocess.run([*build, '--emit-only', '-o', emitted], check=True, timeout=45)
    text = emitted.read_text(encoding='ascii')
    for name in ('ADD', 'SUB', 'CLAMP'):
        assert f'on1x_kernel_rt_builtin_i64(ON1X_KERNEL_RT_{name}_I64' in text
    subprocess.run([*build, '-o', binary], check=True, timeout=45)
    # Neither config discovery nor a host compiler on PATH may participate.
    (root / 'Cephyr.toml').write_text('not valid TOML\n', encoding='ascii')
    env = dict(os.environ, PATH='/nonexistent', CEPHYR_CONFIG='/nonexistent')
    result = subprocess.run([binary], cwd=root, env=env, capture_output=True, text=True, timeout=30)
    assert (result.returncode, result.stdout) == (0, '9223372036854775807\n-9223372036854775808\n4\n'), result

    source.write_text('''\
import foreign stdffi.clamp;
fn mark(x:i64)->i64 { println(x); return x; }
fn main() { println(clamp.i64(mark(1), mark(3), mark(2))); }
''', encoding='ascii')
    subprocess.run([*build, '-o', binary], check=True, timeout=45)
    result = subprocess.run([binary], cwd=root, env=env, capture_output=True, text=True, timeout=30)
    assert (result.returncode, result.stdout) == (70, '1\n3\n2\n'), result
    assert 'assertion failed' in result.stderr, result

    # The standalone embedding target has its own admission boundary. A
    # different C body with the same prototype cannot be embedded or replace
    # the previous successful output, even if on1xc was never built.
    sdk = Path(__file__).resolve().parents[1]
    original = (sdk.parent.parent / 'kernels/mulib-clamp.h').read_bytes()
    header, output = root / 'mulib-clamp.h', root / 'embedded.inc'
    header.write_bytes(original)
    command = ['cmake', f'-Dclamp={header}', f'-Dreview={sdk.parent}/hir/FFIHandler/reviewed.json',
               f'-Dconfig={work}/cephyr-config/cephyr-baseline.toml', f'-Doutput={output}',
               '-P', sdk / 'cmake/EmbedCephyrKernels.cmake']
    subprocess.run(command, check=True)
    prior = output.read_bytes()
    header.write_bytes(original.replace(b'return a + b;', b'return a - b;'))
    rejection = subprocess.run(command, capture_output=True, text=True)
    assert rejection.returncode and 'differs from the reviewed SHA-256' in rejection.stderr
    assert output.read_bytes() == prior
