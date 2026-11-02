#!/usr/bin/env python3
"""Validate an actually compiled Mune systemd profile with systemd's parser."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emitter', required=True)
    parser.add_argument('--validator', required=True)
    args = parser.parse_args()
    unit = subprocess.run([args.emitter, '--systemd'], check=True,
                          capture_output=True, text=True).stdout
    with tempfile.TemporaryDirectory(prefix='mulib-systemd-',
                                    dir='/tmp/opencode' if Path('/tmp/opencode').is_dir() else None) as work:
        path = Path(work) / 'profile-test.service'
        path.write_text(unit)
        result = subprocess.run([args.validator, 'verify', str(path)],
                                capture_output=True, text=True)
        if result.returncode or result.stderr.strip():
            raise SystemExit(result.stderr or result.stdout or 'systemd verification failed')
    print('MuLIB Mune systemd profile: native parser verification passed')


if __name__ == '__main__':
    main()
