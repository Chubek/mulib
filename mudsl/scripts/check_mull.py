#!/usr/bin/env python3
"""Execute the same MuLL fixtures and book examples against both embeddings."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cpp', required=True)
    parser.add_argument('--d')
    args = parser.parse_args()
    binaries = [Path(p).resolve() for p in (args.cpp, args.d) if p]
    cases = 0
    for source in sorted((ROOT / 'tests/mull').glob('*.mull')):
        expected = source.with_suffix('.expected')
        error = source.with_suffix('.error')
        assert expected.exists() != error.exists(), source
        outputs = []
        for binary in binaries:
            result = subprocess.run([binary, ROOT / 'mull/stdlib', source],
                                    capture_output=True, text=True, timeout=15)
            if expected.exists():
                assert result.returncode == 0, (source, result.stderr)
                assert result.stdout == expected.read_text(), (source, result.stdout)
                outputs.append(result.stdout)
            else:
                assert result.returncode == 1, (source, result.returncode, result.stderr)
                assert error.read_text().strip() in result.stderr, (source, result.stderr)
        assert len(set(outputs)) <= 1, source
        cases += 1
    with tempfile.TemporaryDirectory(prefix='mull-book-') as directory:
        for chapter in sorted((ROOT / 'book/chapters').glob('*.ltx')):
            for index, code in enumerate(re.findall(r'\\begin\{mullcode\}(.*?)\\end\{mullcode\}', chapter.read_text(), re.S)):
                source = Path(directory) / f'{chapter.stem}-{index}.mull'
                source.write_text(code)
                outputs = []
                for binary in binaries:
                    result = subprocess.run([binary, ROOT / 'mull/stdlib', source],
                                            capture_output=True, text=True, timeout=15)
                    assert result.returncode == 0, (chapter, index, result.stderr)
                    outputs.append(result.stdout)
                assert len(set(outputs)) <= 1, (chapter, index, outputs)
                cases += 1
    print(f'{cases} MuLL fixtures/book examples passed on {len(binaries)} runtime(s)')


if __name__ == '__main__':
    main()
