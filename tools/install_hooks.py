#!/usr/bin/env python3
"""Install the MuLIB pre-commit dispatcher without replacing existing hooks."""
import os
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
DISPATCHER = b"""#!/bin/sh
# MuLIB managed hook (tools/install_hooks.py).
# Git runs this from the working tree root even if the hook directory is shared.
if [ -x .githooks/pre-commit ]; then
    exec ./.githooks/pre-commit "$@"
fi
"""


def git(*args):
    return subprocess.run(['git', *args], cwd=ROOT, stdout=subprocess.PIPE, check=False)


def git_path(result):
    return ROOT / os.fsdecode(result.stdout.rstrip(b'\n'))


def install():
    if any(name in os.environ for name in ('GIT_DIR', 'GIT_COMMON_DIR', 'GIT_WORK_TREE')):
        raise ValueError('unset Git directory/worktree overrides before installing')
    top = git('rev-parse', '--show-toplevel')
    if top.returncode != 0 or git_path(top).resolve() != ROOT:
        raise ValueError('run this installer from a MuLIB Git checkout')
    checked_in = ROOT / '.githooks/pre-commit'
    if not checked_in.is_file() or not os.access(checked_in, os.X_OK):
        raise ValueError('.githooks/pre-commit is missing or not executable')
    configured = git('config', '--get', 'core.hooksPath')
    if configured.returncode == 0:
        if git_path(configured).resolve() == checked_in.parent:
            print('MuLIB pre-commit hook is already enabled via core.hooksPath.')
            return
        raise ValueError('core.hooksPath is already configured; to preserve your hooks, '
                         'add .githooks/pre-commit to your existing hook chain')
    if configured.returncode != 1:
        raise ValueError('could not read core.hooksPath')
    location = git('rev-parse', '--git-path', 'hooks/pre-commit')
    if location.returncode != 0:
        raise ValueError('could not locate the Git pre-commit hook')
    target = git_path(location)
    if target.parent.is_symlink():
        raise ValueError(f'{target.parent} is a symlink; to preserve your hooks, '
                         'add .githooks/pre-commit to your existing hook chain')
    target.parent.mkdir(parents=True, exist_ok=True)
    try:
        with target.open('xb') as stream:
            stream.write(DISPATCHER)
        target.chmod(0o755)
    except FileExistsError:
        if target.is_symlink() or not target.is_file() or target.read_bytes() != DISPATCHER:
            raise ValueError(f'{target} already exists; to preserve it, add '
                             '.githooks/pre-commit to your existing hook chain') from None
        if not os.access(target, os.X_OK):
            raise ValueError(f'{target} is not executable; review it before enabling')
        print(f'MuLIB pre-commit hook is already installed at {target}.')
        return
    print(f'Installed MuLIB pre-commit hook at {target}.')


if __name__ == '__main__':
    try:
        install()
    except (OSError, ValueError) as error:
        sys.exit(f'install_hooks: {error}')
