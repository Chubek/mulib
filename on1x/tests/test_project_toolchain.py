"""Hosted project tool selections through the public on1xc driver."""
import argparse
import json
from pathlib import Path
import subprocess
import struct
import tempfile


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--on1xc', type=Path, required=True)
parser.add_argument('--cc', default='cc')
parser.add_argument('--cephyr-prefix', type=Path)
args = parser.parse_args()
compiler = args.on1xc.resolve()


def invoke(*words, error=None):
    result = subprocess.run([compiler, *words], cwd=root, text=True, capture_output=True, timeout=60)
    if error is not None:
        assert result.returncode != 0 and error in result.stderr, result
    else:
        assert result.returncode == 0, result
    return result.stdout


BASE = 'on1xfile 1\nkind executable\nname "sample"\ndirectory "src"\ntarget "sample" "src/main.on1x"\n'
HOST = 'compiler host\nassembler host\nlinker host\n'
ROLES = 'on1xfile 1\nsource "main.on1x"\n'

with tempfile.TemporaryDirectory(prefix='on1x-native-tools-') as temporary:
    root = Path(temporary)
    (root / 'src').mkdir()
    manifest = root / 'On1xfile'
    roles = root / 'src/On1xfile.m'
    source = root / 'src/main.on1x'
    empty = root / 'missing native prefix'
    output = root / 'output'
    manifest.write_text(BASE)
    roles.write_text(ROLES)
    source.write_text('fn main() -> i64 { return 7; }\n')
    selected = json.loads(invoke('aux', 'plan', '--cephyr-prefix', empty))['backend']['toolchain']
    assert (selected['compiler'], selected['assembler'], selected['linker']) == ('cephyr', 'sandas', 'yellowstone')
    assert selected['missing'] and selected['runtime'] == 'cephyr-static-amd64'
    invoke('exec', 'build', '--cephyr-prefix', empty, '--output-dir', output,
           error='native toolchain unavailable')
    assert not output.exists()

    for name, text in (('src/On1xfile', 'on1xfile 1\nassembler host\n'),
                       ('src/On1xfile.i', 'on1xfile 1\nlinker host\n')):
        nested = root / name
        nested.write_text(text)
        invoke('aux', 'plan', '--cc', args.cc, error='only allowed in the root project On1xfile')
        nested.unlink()
    roles.write_text(ROLES + 'assembler-arg "--noexecstack"\n')
    invoke('aux', 'plan', '--cc', args.cc, error='only allowed in the root project On1xfile')
    roles.write_text(ROLES)
    (root / 'dependency').mkdir()
    (root / 'dependency/On1xfile').write_text('on1xfile 1\nkind library\nname "dep"\ncompiler host\n')
    manifest.write_text(BASE + 'dependency "dep" local "dependency"\n')
    invoke('aux', 'plan', '--cc', args.cc, error='only allowed in the root project On1xfile')

    for argument in ('--dynamic', '--elf-linker=/usr/bin/ld', '--script-text=SECTIONS{}', '@file', '--output=elsewhere'):
        manifest.write_text(BASE + 'linker-arg ' + json.dumps(argument) + '\n')
        invoke('aux', 'format', manifest, error='unsupported')
    manifest.write_text(BASE + HOST + 'compiler cephyr\n')
    invoke('aux', 'format', manifest, error='duplicate')

    # First-class argument boundaries: both spaces and the comma reach the ELF
    # RUNPATH unchanged. Shell expansion must not create the sentinel.
    sentinel = root / 'not executed'
    literal = '$ORIGIN/a b,c;$(touch "' + str(sentinel) + '")'
    manifest.write_text(BASE + HOST + 'assembler-arg "--fatal-warnings"\n'
                        'linker-arg "-rpath"\nlinker-arg ' + json.dumps(literal) + '\n')
    flags = ('--assembler-arg=--noexecstack', '--linker-arg=-z', '--linker-arg=now')
    selected = json.loads(invoke('aux', 'plan', '--cc', args.cc, *flags))['backend']['toolchain']
    assert selected['assembler-arg'] == ['--fatal-warnings', '--noexecstack']
    assert selected['linker-arg'] == ['-rpath', literal, '-z', 'now']
    invoke('exec', 'build', '--cc', args.cc, '--output-dir', output, *flags)
    binary = output / 'sample'
    assert subprocess.run([binary], check=False).returncode == 7
    assert literal.encode() + b'\0' in binary.read_bytes() and not sentinel.exists()
    invoke('aux', 'build', '--cc', args.cc, '--compiler=cephyr', '--cephyr-prefix', empty,
           error='native toolchain unavailable')

    if args.cephyr_prefix:
        manifest.write_text(BASE)
        native = ('--cephyr-prefix', args.cephyr_prefix.resolve(), '--linker-arg=--gc-sections')
        plan = json.loads(invoke('aux', 'plan', *native))
        assert not plan['backend']['toolchain']['missing']
        assert {'cephyr', 'sandas', 'yellowstone', 'crt', 'libc', 'bank'} <= plan['tools'].keys()
        invoke('exec', 'build', '--output-dir', output, *native)
        native_image = binary.read_bytes()
        assert native_image[:7] == b'\x7fELF\x02\x01\x01'
        assert native_image[16:20] == b'\x02\x00\x3e\x00'
        segments, = struct.unpack_from('<Q', native_image, 32)
        stride, count = struct.unpack_from('<HH', native_image, 54)
        assert all(struct.unpack_from('<I', native_image, segments + i * stride)[0] != 3
                   for i in range(count))  # No PT_INTERP or implicit loader.
        assert subprocess.run([binary], check=False).returncode == 7

        source.write_text('fn main() { println(7); }\n')
        invoke('exec', 'build', '--output-dir', output, *native, error='no host fallback')
        assert binary.read_bytes() == native_image
        invoke('exec', 'build', '--cc', args.cc, '--output-dir', output)
        assert subprocess.check_output([binary]) == b'7\n'
