#!/usr/bin/env python3
"""Run official Unicode normalization, grapheme and bidi-character corpora.

Data is never fetched implicitly. Supply --data-dir, or explicitly --download
with the Unicode version matching the installed ICU. Downloaded Unicode data
retains its upstream copyright/license headers and is not bundled with MuLIB.
"""
import argparse
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
CORPORA = {'normalization': 'NormalizationTest.txt',
           'grapheme': 'auxiliary/GraphemeBreakTest.txt',
           'bidi': 'BidiCharacterTest.txt'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data-dir', type=Path, required=True)
    parser.add_argument('--download', action='store_true')
    parser.add_argument('--version', default='17.0.0')
    parser.add_argument('--sanitizers', action='store_true')
    parser.add_argument('--compare-native', action='store_true',
                        help='also compare every bidi result with directly linked ICU; does not waive corpus failures')
    parser.add_argument('--suite', choices=list(CORPORA), action='append')
    args = parser.parse_args()
    if not re.fullmatch(r'\d+\.\d+\.\d+', args.version):
        parser.error('version must be a Unicode release such as 17.0.0')
    suites = args.suite or list(CORPORA)
    for suite in suites:
        path = args.data_dir / CORPORA[suite]
        if args.download and not path.exists():
            url = f'https://www.unicode.org/Public/{args.version}/ucd/{CORPORA[suite]}'
            path.parent.mkdir(parents=True, exist_ok=True)
            with urllib.request.urlopen(url, timeout=120) as response:
                data = response.read(32 * 1024 * 1024 + 1)
            if len(data) > 32 * 1024 * 1024:
                parser.error('corpus exceeds download limit')
            if args.version.encode() not in data[:4096] or not data.rstrip().endswith(b'# EOF'):
                parser.error(f'{url}: missing version header or end marker')
            temporary = None
            try:
                with tempfile.NamedTemporaryFile(dir=path.parent, prefix=path.name + '.', delete=False) as output:
                    temporary = Path(output.name)
                    output.write(data)
                os.replace(temporary, path)
                temporary = None
            finally:
                if temporary is not None:
                    temporary.unlink(missing_ok=True)
        if not path.is_file():
            parser.error(f'missing corpus: {path}; supply data or explicitly request --download')
        with path.open('rb') as source:
            data = source.read(32 * 1024 * 1024 + 1)
        if len(data) > 32 * 1024 * 1024:
            parser.error(f'{path}: corpus exceeds size limit')
        if args.version.encode() not in data[:4096]:
            parser.error(f'{path}: header does not identify requested Unicode {args.version}')
        if not data.rstrip().endswith(b'# EOF'):
            parser.error(f'{path}: missing end marker (possibly truncated corpus)')
    flags = ['-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I' + str(ROOT / 'include')]
    if args.sanitizers:
        flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    links = ['-lm'] if os.name != 'nt' else []
    if os.sys.platform.startswith(('linux', 'freebsd')):
        links += ['-ldl']
    if args.compare_native:
        if not shutil.which('pkg-config'):
            parser.error('--compare-native requires pkg-config and ICU development packages')
        flags += ['-DMULIB_TEXT_CONFORMANCE_NATIVE']
        native = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'icu-uc'], text=True))
        links += native
    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-unicode-', dir=parent) as directory:
        binary = Path(directory) / 'unicode-conformance'
        subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + flags +
                       [str(ROOT / 'tests/native/unicode_conformance.c')] + links +
                       ['-o', str(binary)], check=True, timeout=120)
        for suite in suites:
            result = subprocess.run([str(binary), suite, str(args.data_dir / CORPORA[suite])], check=False, timeout=300)
            if result.returncode:
                raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
