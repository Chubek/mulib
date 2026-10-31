#!/usr/bin/env python3
"""Run the Prowse local smoke script against an isolated headless Firefox profile."""
import argparse
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runner', required=True, type=Path)
    parser.add_argument('--firefox', default=shutil.which('firefox'))
    args = parser.parse_args()
    if not args.firefox:
        parser.error('Firefox unavailable')
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        port = listener.getsockname()[1]
    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='prowse-firefox-', dir=parent) as work:
        profile = Path(work)
        (profile / 'user.js').write_text(f'user_pref("marionette.port", {port});\n'
            'user_pref("browser.shell.checkDefaultBrowser", false);\n'
            'user_pref("browser.startup.homepage_override.mstone", "ignore");\n'
            'user_pref("datareporting.policy.dataSubmissionEnabled", false);\n')
        with (profile / 'browser.log').open('w+') as log:
            child = subprocess.Popen([args.firefox, '--headless', '--no-remote', '--marionette',
                                      '--profile', work, 'about:blank'], stdout=log, stderr=log,
                                     env={**os.environ, 'MOZ_HEADLESS': '1'})
            try:
                deadline = time.monotonic() + 40
                while True:
                    if child.poll() is not None or time.monotonic() > deadline:
                        log.seek(0)
                        raise RuntimeError('Firefox did not start:\n' + log.read())
                    try:
                        with socket.create_connection(('127.0.0.1', port), timeout=0.2) as client:
                            client.recv(1024)
                        break
                    except OSError:
                        time.sleep(0.1)
                result = subprocess.run([str(args.runner.resolve()), 'marionette',
                    f'marionette://127.0.0.1:{port}', str(ROOT / 'libraries/lprowse/examples/local-smoke.lua')],
                    capture_output=True, text=True, timeout=60)
                print(result.stdout, end='')
                if result.returncode:
                    raise RuntimeError(result.stderr or f'runner failed: {result.returncode}')
            finally:
                child.terminate()
                try:
                    child.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()


if __name__ == '__main__':
    main()
