#!/usr/bin/env python3
"""Create a minimal Satie EUF source SDK with the explicit upstream CMake patch.

Copies actual headers and allocator implementation; never changes the source
checkout. No fetching. The destination must not already exist.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
cli = argparse.ArgumentParser(description=__doc__)
cli.add_argument('--source', type=Path, default=root/'third_party/libstone/third_party/satie')
cli.add_argument('--output', type=Path, required=True)
args = cli.parse_args()
source, output = args.source.resolve(), args.output.resolve()

if output.exists():
    cli.error('destination already exists')
patch = root/'upstream/satie-euf-target.patch'
required = ['CMakeLists.txt', 'src/SatieMemory.cpp', 'include', 'third_party/memtkx/include']
for name in required:
    if not (source/name).exists():
        cli.error('missing source: '+str(source/name))
output.mkdir(parents=True)
for name in required:
    src, dst = source/name, output/name
    if src.is_dir():
        shutil.copytree(src, dst)
    else:
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dst)
hashes = {str(p.relative_to(output)): hashlib.sha256(p.read_bytes()).hexdigest()
          for p in sorted(output.rglob('*')) if p.is_file()}


def apply(*flags):
    # An out-of-source build may still be inside MuLIB's worktree. Without a
    # discovery ceiling, git apply silently skips the patch's root paths there.
    env = os.environ.copy()
    for key in ('GIT_DIR', 'GIT_WORK_TREE', 'GIT_COMMON_DIR', 'GIT_INDEX_FILE', 'GIT_PREFIX'):
        env.pop(key, None)
    env['GIT_CEILING_DIRECTORIES'] = str(output.parent)
    subprocess.run(['git', 'apply', *flags, str(patch)], cwd=output, env=env, check=True)


apply('--check')
apply()
apply('--reverse', '--check')
(output/'on1x-sdk-provenance.json').write_text(json.dumps({
    'source': str(source), 'original_sha256': hashes,
    'patch_sha256': hashlib.sha256(patch.read_bytes()).hexdigest(),
    'patched_cmake_sha256': hashlib.sha256((output/'CMakeLists.txt').read_bytes()).hexdigest(),
}, indent=2, sort_keys=True)+'\n')
print(output)
