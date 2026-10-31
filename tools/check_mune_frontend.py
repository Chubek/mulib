#!/usr/bin/env python3
"""Compare Mune's non-executing frontend with an installed Lua 5.4 compiler."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frontend', required=True)
    parser.add_argument('--lua', default='lua5.4')
    args = parser.parse_args()
    version = subprocess.check_output([args.lua, '-e', 'io.write(_VERSION)'], timeout=10)
    if version != b'Lua 5.4':
        parser.error('the reference must implement Lua 5.4')
    corpus = subprocess.check_output([args.frontend, '--corpus'], timeout=30).split(b'\0')
    reference = "local f,e=load(io.read('*a'),'test','t'); if not f then io.stderr:write(e); os.exit(1) end"
    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-mune-reference-', dir=parent) as work:
        source = Path(work) / 'input.lua'
        for index, case in enumerate(corpus[:-1]):
            source.write_bytes(case[1:])
            lua = subprocess.run([args.lua, '-e', reference], input=case[1:],
                                 capture_output=True, timeout=10)
            mune = subprocess.run([args.frontend, '--check', str(source)],
                                  capture_output=True, timeout=10)
            accepted = case[:1] == b'1'
            if (lua.returncode == 0) != accepted or (mune.returncode == 0) != accepted:
                raise RuntimeError(f'case {index}: {case[1:]!r}\nLua: {lua.stderr!r}\nMune: {mune.stderr!r}')
    print(f'Mune/Lua 5.4 frontend equivalence: {len(corpus) - 1} cases passed')


if __name__ == '__main__':
    main()
