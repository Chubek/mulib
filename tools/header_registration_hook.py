#!/usr/bin/env python3
"""Register newly staged public headers using only inputs in the commit index."""
import io
import os
from pathlib import Path, PurePosixPath
import stat
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
HEADERS = ':(top,glob)kernels/mulib-*.h'
OUTPUTS = ('MANIFEST.yaml', 'on1x/contracts-exotract/kernel-auto.json')
FRAGMENTS = ('MANIFEST.yaml.additions', 'MANIFESTS.yaml.additions')
GENERATOR = 'tools/sync_inventory.py'
REGULAR_MODES = {b'100644', b'100755'}


def git(*args, data=None, env=None):
    result = subprocess.run(['git', '--no-replace-objects', *args], cwd=ROOT, input=data,
                            env=env, stdout=subprocess.PIPE)
    if result.returncode:
        raise ValueError(f'Git could not {args[0]}; resolve the Git error and retry')
    return result.stdout


def public_header(path):
    filename = PurePosixPath(path)
    return filename.parent.as_posix() == 'kernels' and filename.match('mulib-*.h')


def snapshot_input(path):
    name = PurePosixPath(path)
    return (path in (*OUTPUTS, GENERATOR)
            or public_header(path)
            or name.parts[0] == 'manual'
            or (name.parent.as_posix() == 'examples' and name.match('mulib-*.c'))
            or (name.parent.as_posix() == 'examples/modules' and name.suffix == '.c')
            or (name.parent.as_posix() == 'tests'
                and (name.match('test_*.c') or name.match('test-*.c'))))


def tree_entries(tree):
    entries = {}
    records = git('ls-tree', '-r', '-z', tree, '--', *OUTPUTS, *FRAGMENTS,
                  GENERATOR, 'kernels/', 'manual/', 'examples/', 'tests/')
    for record in records.split(b'\0'):
        if record:
            info, name = record.split(b'\t', 1)
            mode, _, oid = info.split()
            path = os.fsdecode(name)
            if path in FRAGMENTS:
                raise ValueError(f'{path} is in the staged snapshot; run '
                                 'python3 tools/sync_inventory.py manually and stage '
                                 'its results instead of staging the additions fragment')
            if snapshot_input(path):
                if mode not in REGULAR_MODES:
                    if public_header(path) or path in OUTPUTS:
                        raise ValueError(f'{path} must be a regular staged file')
                    continue  # A symlink must not satisfy an authoritative input.
                if any(part in {'.', '..'} or '\\' in part for part in path.split('/')):
                    raise ValueError(f'unsupported staged path: {path!r}')
                entries[path] = (mode, oid)
    return entries


def materialize(root, entries):
    # Read raw blobs: checkout/checkout-index would run user-defined smudge
    # filters and could make generator input differ from the committed bytes.
    data = b''.join(oid + b'\n' for _, oid in entries.values())
    raw = io.BytesIO(git('cat-file', '--batch', data=data))
    before = {}
    for path, (_, oid) in entries.items():
        header = raw.readline().split()
        if len(header) != 3 or header[:2] != [oid, b'blob']:
            raise ValueError(f'could not read staged blob: {path}')
        contents = raw.read(int(header[2]))
        if raw.read(1) != b'\n':
            raise ValueError(f'incomplete staged blob: {path}')
        target = root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(contents)
        if path in OUTPUTS:
            before[path] = contents
    for path in OUTPUTS:
        (root / path).parent.mkdir(parents=True, exist_ok=True)
    return before


def working_mode(path, expected):
    """Reject local changes (including hidden index flags) before publication."""
    target = ROOT / path
    for parent in target.parents:
        if parent == ROOT:
            break
        if not stat.S_ISDIR(parent.lstat().st_mode):
            raise ValueError(f'unsafe path: {parent.relative_to(ROOT)} is not a real directory')
    try:
        mode = target.lstat().st_mode
    except FileNotFoundError:
        if expected is None:
            return 0o644
    else:
        if expected is not None and stat.S_ISREG(mode) and target.read_bytes() == expected:
            return stat.S_IMODE(mode)
    raise ValueError(f'{path} differs from the staged snapshot; stage or set aside '
                     'your local changes, then retry')


def check_index():
    env = os.environ.copy()
    env.pop('GIT_INDEX_FILE', None)
    args = ('rev-parse', '--git-path', 'index')
    default = (ROOT / os.fsdecode(git(*args, env=env).rstrip(b'\n'))).resolve()
    current = (ROOT / os.fsdecode(git(*args).rstrip(b'\n'))).resolve()
    # Git uses index.lock for `commit -a` and `commit --include`; that
    # index replaces the default index if the commit succeeds.
    if current == default or current == default.with_name(default.name + '.lock'):
        return
    if current.name.startswith('next-index-') and current.name.endswith('.lock'):
        raise ValueError('automatic registration cannot update a path-limited commit; '
                         'stage the desired files and run git commit without path arguments')
    raise ValueError('automatic registration cannot use an alternate Git index; '
                     'unset GIT_INDEX_FILE, stage the desired files in the default index '
                     'and retry')


def publish(tree, entries, before, changes):
    check_index()
    flags = git('ls-files', '-v', '-z', '--', *changes).split(b'\0')
    if any(entry and not entry.startswith(b'H ') for entry in flags):
        raise ValueError('a generated path is marked assume-unchanged or skip-worktree; '
                         'clear that flag before retrying this commit')
    # Check every changing destination before updating any of them. Bytewise
    # comparison deliberately does not run clean filters or trust Git stat data.
    modes = {path: working_mode(path, before.get(path)) for path in changes}
    if git('write-tree').strip() != tree:
        raise ValueError('the Git index changed during registration; retry the commit')

    prepared = {}
    index_info = []
    try:
        for path, contents in changes.items():
            mode = entries[path][0] if path in entries else b'100644'
            oid = git('hash-object', '-w', '--no-filters', '--stdin', data=contents).strip()
            index_info.append(mode + b' ' + oid + b'\t' + os.fsencode(path) + b'\0')
            with tempfile.NamedTemporaryFile(dir=(ROOT / path).parent, delete=False) as temp:
                prepared[path] = Path(temp.name)
                temp.write(contents)
            prepared[path].chmod(modes[path])
        if git('write-tree').strip() != tree:
            raise ValueError('the Git index changed during registration; retry the commit')
        for path in changes:
            if working_mode(path, before.get(path)) != modes[path]:
                raise ValueError(f'{path} permissions changed during registration; retry the commit')
        for path, temp in prepared.items():
            os.replace(temp, ROOT / path)
        # A single update-index transaction stages only the exact generated
        # blobs, not unstaged bytes and not anything written by clean filters.
        git('update-index', '-z', '--index-info', data=b''.join(index_info))
    finally:
        for temp in prepared.values():
            temp.unlink(missing_ok=True)


def run():
    staged = git('diff', '--cached', '--name-only', '--no-renames', '--no-ext-diff',
                 '--diff-filter=A', '--ita-invisible-in-index', '-z', '--', HEADERS)
    if not staged:
        return
    tree = git('write-tree').strip()
    entries = tree_entries(tree.decode('ascii'))
    with tempfile.TemporaryDirectory(prefix='mulib-registration-') as directory:
        root = Path(directory)
        before = materialize(root, entries)
        # Only the local tool is executable. The temporary tree is staged data,
        # not a checkout containing a script the hook should run.
        result = subprocess.run(
            [sys.executable, '-I', '-B', str(ROOT / GENERATOR),
             '--root', str(root), '--metadata-only'], cwd=ROOT)
        if result.returncode:
            raise ValueError('registration failed; fix and stage the required header/metadata '
                             'inputs, then retry the commit')
        after = {path: (root / path).read_bytes() for path in OUTPUTS}
    changes = {path: contents for path, contents in after.items() if contents != before.get(path)}
    if changes:
        publish(tree, entries, before, changes)
        print('MuLIB pre-commit: registered staged headers in ' + ', '.join(changes), file=sys.stderr)


if __name__ == '__main__':
    try:
        run()
    except (OSError, ValueError) as error:
        sys.exit(f'MuLIB pre-commit: {error}')
