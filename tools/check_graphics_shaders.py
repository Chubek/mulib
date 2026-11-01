#!/usr/bin/env python3
"""Validate ShaderX's GLSL/HLSL output with an explicitly supplied glslang."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emitter', required=True)
    parser.add_argument('--validator', required=True)
    args = parser.parse_args()
    emitter = str(Path(args.emitter).resolve())
    validator = str(Path(args.validator).resolve())
    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-shaderx-', dir=parent) as directory:
        work = Path(directory)
        for stage, suffix in [('vertex', 'vert'), ('fragment', 'frag')]:
            for target in ['glsl', 'hlsl']:
                source = work / f'shader.{suffix}.{target}'
                subprocess.run([emitter, stage, str(source)], check=True, timeout=30)
                command = [validator, '-S', suffix]
                if target == 'hlsl':
                    command += ['-D', '-V', '-e', 'main', '-o', str(work / 'shader.spv')]
                subprocess.run(command + [str(source)], check=True, timeout=30)
    print('ShaderX: GLSL and HLSL vertex/fragment shaders accepted')


if __name__ == '__main__':
    main()
