#!/usr/bin/env python3
"""Build a declared On1x project without running dependency build programs."""
import argparse
import ctypes
import errno
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
POLICY = ROOT / 'stdlib/on1xfile'
MAX_MANIFEST = 64 * 1024
MAX_SOURCE = 2 * 1024 * 1024
MAX_INPUT = 64 * 1024 * 1024
MAX_FILES = 4096
MAX_PROCESS = 64 * 1024 * 1024
MAX_GIT_MEMORY = 512 * 1024 * 1024
MAX_GIT_OBJECT_FILES = 32768
MAX_GIT_OBJECT_BYTES = 128 * 1024 * 1024
GIT_FETCH_TIMEOUT = 30
GIT_VALIDATION_TIMEOUT = 30
NAME = re.compile(r'[A-Za-z_][A-Za-z0-9_-]{0,63}\Z')
IDENT = re.compile(r'[A-Za-z_][A-Za-z0-9_]{0,63}\Z')
PART = re.compile(r'[A-Za-z_0-9][A-Za-z_0-9. -]{0,127}\Z')
TOKEN = re.compile(r'"(?:[^"\\\r\n]|\\.)*"|[A-Za-z0-9_-]+')
QMR_TAG = re.compile(rb'\{[{%#]')
ENV = {'PATH': '/usr/bin:/bin', 'LC_ALL': 'C', 'TZ': 'UTC',
       'HOME': '/nonexistent', 'SOURCE_DATE_EPOCH': '0', 'PYTHONDONTWRITEBYTECODE': '1'}
GIT_ENV = dict(ENV, GIT_CONFIG_NOSYSTEM='1', GIT_CONFIG_GLOBAL='/dev/null',
               GIT_ALLOW_PROTOCOL='', GIT_NO_LAZY_FETCH='1', GIT_TERMINAL_PROMPT='0')


def fail(message):
    raise ValueError(str(message))


def digest(data):
    return hashlib.sha256(data).hexdigest()


def name(value, pattern=NAME):
    if not pattern.fullmatch(value):
        fail(f'invalid name {value!r}')
    return value


def local(value, *, dot=False, single=False):
    parts = value.split('/')
    if dot and value == '.':
        return Path('.')
    if len(value) > 1024 or len(parts) > 32 or (single and len(parts) != 1) or any(
            not PART.fullmatch(part) for part in parts):
        fail(f'expected a normalized local path, got {value!r}')
    return Path(value)


def read(path, limit):
    # No following the final component, and no blocking on pipes. Parent paths
    # are checked separately; this is not a concurrent-worktree security lock.
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(fd, 'rb') as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_size > limit:
            fail(f'{path}: expected a regular file of at most {limit} bytes')
        data = stream.read(limit + 1)
        if len(data) > limit:
            fail(f'{path}: file exceeds {limit} bytes')
        return data


def contained(root, relative):
    path = root
    for part in relative.parts:
        path /= part
        if path.is_symlink():
            fail(f'{path}: symlinks are not allowed in declared inputs')
    return path


def rows(payload, origin):
    if len(payload) > MAX_MANIFEST or b'\x00' in payload:
        fail(f'{origin}: manifest byte budget exceeded or contains NUL')
    text = payload.decode('utf-8')
    first = True
    for number, line in enumerate(text.split('\n'), 1):
        words, pos = [], 0
        at = f'{origin}:{number}'
        while pos < len(line):
            if line[pos] in ' \t\r':
                pos += 1
                continue
            if line[pos] == '#':
                break
            match = TOKEN.match(line, pos)
            if not match:
                fail(f'{at}: expected a word or quoted string')
            word = match[0]
            quoted = word.startswith('"')
            try:
                value = json.loads(word) if quoted else word
                value.encode('utf-8')
            except (UnicodeError, ValueError) as error:
                fail(f'{at}: invalid string: {error}')
            if '\x00' in value or len(value) > 4096:
                fail(f'{at}: invalid or overlong field')
            words.append((value, quoted))
            pos = match.end()
            if pos != len(line) and line[pos] not in ' \t\r#':
                fail(f'{at}: fields must be separated')
        if not words:
            continue
        if first:
            if words != [('on1xfile', False), ('1', False)]:
                fail(f'{at}: expected on1xfile 1')
            first = False
        elif words[0][1]:
            fail(f'{at}: expected a directive word')
        else:
            yield at, words[0][0], words[1:]
    if first:
        fail(f'{origin}: missing on1xfile 1')


def fields(args, signature):
    if len(args) != len(signature) or any(q != (kind == '"') for (_, q), kind in zip(args, signature)):
        fail('missing, extra or incorrectly quoted fields')
    return [value for value, _ in args]


def unique(items, key, value):
    if key in items:
        fail(f'duplicate declaration {key!s}')
    items[key] = value


def remote_url(value):
    # Admit a conservative, unambiguous subset, without URL credentials,
    # escaped path segments, options, or Git's <helper>:: and scp syntax.
    label = r'[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?'
    match = re.fullmatch(rf'https://({label}(?:\.{label})*)(?::([0-9]{{1,5}}))?'
                         r'(/[A-Za-z0-9._~-]+(?:/[A-Za-z0-9._~-]+)*)', value)
    if (len(value) > 2048 or not match or len(match[1]) > 253
            or match[2] and not 1 <= int(match[2]) <= 65535
            or any(part in ('.', '..') for part in match[3].split('/'))):
        fail('git-remote requires an HTTPS repository URL without credentials, escapes, query or fragment')
    return value


def root_manifest(payload, origin, dependency=False):
    options, targets, deps, remotes, defines, profiles, kernels, commands = {}, {}, {}, {}, {}, {}, {}, {}
    for at, verb, args in rows(payload, origin):
        try:
            if verb == 'kind':
                value, = fields(args, 'w')
                if value not in ('executable', 'complex', 'library'):
                    fail(f'unsupported project kind {value!r}')
                unique(options, verb, value)
            elif verb in ('name', 'directory'):
                value, = fields(args, '"')
                unique(options, verb, name(value) if verb == 'name' else local(value, dot=True))
            elif verb == 'target' and not dependency:
                key, value = fields(args, '""')
                unique(targets, name(key), local(value))
            elif verb == 'dependency' and not dependency:
                kind = args[1] if len(args) > 1 else None
                if kind == ('local', False):
                    key, _, path = fields(args, '"w"')
                    unique(deps, name(key, IDENT), ('local', local(path), None))
                elif kind in (('git', False), ('git-remote', False)):
                    key, transport, path, revision = fields(args, '"w""')
                    key = name(key, IDENT)
                    if not re.fullmatch(r'[0-9a-f]{40}|[0-9a-f]{64}', revision):
                        fail('git dependency requires a full lowercase commit object ID')
                    if transport == 'git-remote':
                        remotes[key] = remote_url(path)
                        relative = Path('.on1x-deps') / key / revision
                    else:
                        relative = local(path)
                    unique(deps, key, (transport, relative, revision))
                else:
                    fail('dependency kind must be local, git or git-remote')
            elif verb == 'profile' and not dependency:
                key, path = fields(args, '""')
                if key in ('dev', 'release') or not path.endswith('.lua'):
                    fail('use a custom profile name and a local .lua script')
                unique(profiles, name(key), local(path))
            elif verb == 'command' and not dependency:
                key, action, target, path = fields(args, '"ww"')
                if key in ('build', 'release') or action != 'format' or target not in ('plain', 'latex', 'troff'):
                    fail('expected a custom command of the form: command "name" format plain|latex|troff "source.on1x"')
                source = local(path)
                if source.suffix != '.on1x':
                    fail('formatter commands require an .on1x source path')
                unique(commands, name(key), (target, source))
            elif verb == 'define' and not dependency:
                if len(args) == 3 and args[1] == ('shell', False):
                    key, _, value = fields(args, 'ww"')
                    unique(defines, name(key, IDENT), ('shell', value))
                else:
                    key, value = fields(args, 'w"')
                    unique(defines, name(key, IDENT), ('literal', value))
            elif verb == 'kernel' and not dependency:
                key, = fields(args, '"')
                if not re.fullmatch(r'[a-z0-9][a-z0-9_+-]{0,63}', key):
                    fail('invalid MuLIB kernel name')
                unique(kernels, key, True)
            else:
                fail(f'unsupported {verb!r} in {"dependency" if dependency else "root"} On1xfile')
        except ValueError as error:
            fail(f'{at}: {error}')
    kind = options.get('kind')
    if not options.get('name') or not kind:
        fail(f'{origin}: name and kind are required')
    if kind == 'executable' and len(targets) != 1 or kind == 'complex' and not targets or kind == 'library' and targets:
        fail(f'{origin}: executable needs one target, complex needs at least one, and library allows none')
    if len(targets) > 64 or len(deps) > 32 or len(profiles) > 32 or len(defines) > 64 or len(kernels) > 32:
        fail(f'{origin}: declaration count budget exceeded')
    if commands.keys() & profiles.keys():
        fail(f'{origin}: a command cannot have the same name as a profile')
    if len(commands) > 32:
        fail(f'{origin}: command declaration count budget exceeded')
    options.update(targets=targets, deps=deps, remotes=remotes, defines=defines,
                   profiles=profiles, kernels=list(kernels), commands=commands)
    return options


def program(value):
    path = Path(value)
    if not path.is_absolute() and '/' not in value:
        path = Path(shutil.which(value, path=ENV['PATH']) or value)
    path = path.resolve()
    if not path.is_file() or not os.access(path, os.X_OK):
        fail(f'executable unavailable: {value}')
    return path


def run(command, cwd, *, data=None, env=ENV, timeout=60, bound=MAX_PROCESS, memory=None):
    def limit():
        resource.setrlimit(resource.RLIMIT_FSIZE, (MAX_PROCESS, MAX_PROCESS))
        if memory is not None:
            resource.setrlimit(resource.RLIMIT_AS, (memory, memory))
    with tempfile.TemporaryFile() as output, tempfile.TemporaryFile() as errors:
        with subprocess.Popen(list(map(str, command)), cwd=cwd, env=env,
                              stdin=subprocess.PIPE if data is not None else subprocess.DEVNULL,
                              stdout=output, stderr=errors, start_new_session=True, preexec_fn=limit) as child:
            try:
                child.communicate(data, timeout=timeout)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(child.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                child.wait()
                fail(f'{command[0]} exceeded {timeout}-second subprocess limit')
            if child.returncode:
                errors.seek(0)
                diagnostic = errors.read(8192).decode('utf-8', 'replace').rstrip()
                fail(f'{command[0]} failed ({child.returncode})' + (f':\n{diagnostic}' if diagnostic else ''))
        if output.tell() > bound:
            fail(f'{command[0]}: output byte limit exceeded')
        output.seek(0)
        return output.read()


class Snapshot:
    def __init__(self):
        self.inputs = {}
        self.total = 0

    def record(self, label, data):
        if label in self.inputs:
            fail(f'duplicate input: {label}')
        self.total += len(data)
        if self.total > MAX_INPUT or len(self.inputs) >= MAX_FILES:
            fail('project snapshot exceeds its file/byte budget')
        self.inputs[label] = digest(data)
        return data


class Disk:
    def __init__(self, root, exclude=()):
        self.root = root
        self.exclude = set(exclude)

    def get(self, path, limit):
        return read(contained(self.root, path), limit)

    def manifests(self, directory):
        start = contained(self.root, directory)
        if not start.is_dir():
            fail(f'{start}: declared directory is missing')
        inspected = 0
        for parent, children, files in os.walk(start, followlinks=False, onerror=lambda e: fail(e)):
            parent = Path(parent)
            rel = parent.relative_to(self.root)
            inspected += len(children) + len(files)
            if inspected > 32768 or len(rel.parts) > 32:
                fail(f'{start}: directory discovery budget exceeded')
            children[:] = sorted(c for c in children if not c.startswith('.') and c not in ('target', 'build')
                                 and not (parent / c).is_symlink() and (parent / c) not in self.exclude)
            for filename in ('On1xfile.m', 'On1xfile.i'):
                if filename in files or (parent / filename).is_symlink():
                    yield rel / filename


class Git:
    """Use commit blobs, never checkout, archives, filters, hooks, or remote URLs."""
    @staticmethod
    def command(gitdir):
        return [program('git'), '--no-replace-objects', '--git-dir', gitdir,
                '-c', 'core.hooksPath=/dev/null', '-c', 'pack.threads=1']

    @staticmethod
    def init(revision):
        return ('init', '--bare', '--quiet', '--template=',
                '--object-format=' + ('sha256' if len(revision) == 64 else 'sha1'))

    def __init__(self, repository, revision, cwd, gitdir, *, deadline=None):
        self.cwd = cwd
        self.deadline = deadline
        if not repository.is_dir():
            fail(f'{repository}: offline Git object store is missing')
        dotgit = repository / '.git'
        if dotgit.is_symlink() or dotgit.is_file():
            fail(f'{repository}: external Git worktrees are not supported')
        source = (dotgit if dotgit.is_dir() else repository) / 'objects'
        if source.is_symlink() or not source.is_dir():
            fail(f'{repository}: expected a self-contained Git objects directory')
        self.git = self.command(gitdir)
        self.call(*self.init(revision), bound=4096)
        # Project only standard object filenames into a fresh repository. Its
        # config, alternates, promisor remotes, replacements, and hooks cannot
        # originate in the supplied store. Pack files may be read, never run.
        total = count = 0
        for directory in sorted(source.iterdir()):
            packed = directory.name == 'pack'
            if not packed and not re.fullmatch(r'[a-f0-9]{2}', directory.name):
                continue
            if directory.is_symlink() or not directory.is_dir():
                fail(f'{directory}: expected a real Git object directory')
            for path in sorted(directory.iterdir()):
                match = (rf'pack-[a-f0-9]{{{len(revision)}}}\.(?:idx|pack)' if packed
                         else rf'[a-f0-9]{{{len(revision) - 2}}}')
                if re.fullmatch(match, path.name):
                    count += 1
                    if count > MAX_GIT_OBJECT_FILES:
                        fail('Git object file count limit exceeded')
                    data = read(path, MAX_GIT_OBJECT_BYTES - total)
                    total += len(data)
                    stage_bytes(gitdir / 'objects' / directory.name / path.name, data)
        try:
            object_kind = self.call('cat-file', '-t', revision, bound=64)
        except ValueError:
            fail(f'Git pin {revision} is missing from the offline object store')
        if object_kind != b'commit\n':
            fail('Git pin must name a commit, not a tag, tree or blob')
        self.call('fsck', '--strict', '--no-reflogs', '--no-dangling', revision, bound=4096)
        self.entries = {}
        listing = self.call('ls-tree', '-rz', '--full-tree', revision, bound=4 * 1024 * 1024)
        for record in listing.split(b'\x00')[:-1]:
            header, path = record.split(b'\t', 1)
            if len(self.entries) >= 32768:
                fail('Git tree entry limit exceeded')
            self.entries[os.fsdecode(path)] = header.decode('ascii').split()

    def call(self, *arguments, bound):
        timeout = 20
        if self.deadline is not None:
            timeout = min(timeout, self.deadline - time.monotonic())
            if timeout <= 0:
                fail('Git dependency exceeded its overall validation time limit')
        return run([*self.git, *arguments], self.cwd, env=GIT_ENV,
                   bound=bound, timeout=timeout, memory=MAX_GIT_MEMORY)

    def get(self, path, limit):
        entry = self.entries.get(path.as_posix())
        if not entry or entry[0] not in ('100644', '100755') or entry[1] != 'blob':
            fail(f'Git dependency: {path} is missing or is not a regular blob')
        size = int(self.call('cat-file', '-s', entry[2], bound=64))
        if size > limit:
            fail(f'Git dependency: {path} exceeds {limit} bytes')
        data = self.call('cat-file', 'blob', entry[2], bound=limit)
        if len(data) != size:
            fail(f'Git dependency: {path} object size changed')
        return data

    def manifests(self, directory):
        for key in sorted(self.entries):
            path = Path(key)
            if path.name in ('On1xfile.m', 'On1xfile.i') and path.is_relative_to(directory):
                parents = path.parent.relative_to(directory).parts
                if not any(p.startswith('.') or p in ('target', 'build') for p in parents):
                    yield local(key)


def selected_tool(args, field, filename, compiler, *, optional=False):
    explicit = getattr(args, field)
    if explicit:
        return program(explicit)
    for candidate in (compiler.parent / filename, compiler.parent.parent / 'libexec/on1x' / filename):
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate.resolve()
    if not optional:
        fail(f'{filename} unavailable; configure the build helper or supply --{field.replace("_", "-")}')
    return None


class Execution:
    """Compiler process envelope; host file acquisition itself is intentionally outside it."""
    def __init__(self, args, work, compiler, cc, stdlib, mulib):
        self.work = work
        self.env = ENV
        self.prefix = []
        if args.sandbox:
            if not sys.platform.startswith('linux'):
                fail('--sandbox requires Linux bubblewrap')
            self.prefix = [program('bwrap'), '--unshare-all', '--die-with-parent', '--new-session',
                           '--clearenv', '--ro-bind', '/usr', '/usr', '--proc', '/proc', '--dev', '/dev',
                           '--tmpfs', '/tmp', '--dir', '/etc', '--bind', work, '/work',
                           '--ro-bind', work / 'input', '/work/input', '--chdir', '/work/cwd']
            for alias in ('/bin', '/sbin', '/lib', '/lib64'):
                path = Path(alias)
                if path.is_symlink():
                    self.prefix.extend(('--symlink', os.readlink(path), alias))
                elif path.is_dir():
                    self.prefix.extend(('--ro-bind', alias, alias))
            for key, value in ENV.items():
                self.prefix.extend(('--setenv', key, value))
            self.mounts = set()
            for path in (ROOT / 'tools', ROOT / 'runtime', ROOT / 'stdlib/mulib',
                         mulib / 'include', mulib / 'manual',
                         mulib / 'library', mulib / 'MANIFEST.yaml', compiler, cc, POLICY):
                self.mount(path)
            if stdlib is not None:
                self.mount(stdlib)
                self.mount(stdlib.parent / 'std')
            # Eligibility is checked before any project-owned Lua can be run.
            self.run(['/usr/bin/true'], bound=32)

    def mount(self, path):
        if not self.prefix or not path.exists():
            return
        path = path.resolve()
        if path.is_relative_to('/usr') or path in self.mounts:
            return
        if path == Path('/') or path.is_relative_to(self.work):
            fail(f'invalid sandbox toolchain mount: {path}')
        self.mounts.add(path)
        self.prefix.extend(('--ro-bind', path, path))

    def path(self, path):
        return Path('/work') / path.relative_to(self.work) if self.prefix else path

    def run(self, command, **kwargs):
        return run([*self.prefix, *(['--'] if self.prefix else []), *command],
                   self.work / 'cwd', env=self.env, **kwargs)


def stage_bytes(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('xb') as stream:
        stream.write(data)


# The build helper looks up these C companions via --stdlib-resource. Preserve
# the existing standalone MuLIB discovery manifest as well. Other SDK inputs
# are not part of the project-controlled package.
STDLIB_RESOURCES = frozenset(map(Path, (
    'runtime/basis_native.c', 'runtime/basis_native.h', 'runtime/basis_real.c',
    'prettyprint/runtime.c', 'prettyprint/runtime.h', 'mulib/On1xfile',
)))


def local_roles(data, origin, manifest, kind):
    allowed = ('interface',) if manifest.name == 'On1xfile.i' else ('source', 'library', 'resource')
    declared = {}
    for at, role, args in rows(data, origin):
        try:
            if role not in allowed or role == 'source' and kind == 'library':
                fail(f'role {role!r} is not allowed here')
            filename, = fields(args, '"')
            source = manifest.parent / local(filename, single=True)
            suffixes = {'interface': ('.on1xi',), 'resource': ('.c', '.h')}
            if source.suffix not in suffixes.get(role, ('.on1x',)) and (
                    role != 'resource' or filename != 'On1xfile'):
                fail(f'invalid extension for {role}')
            unique(declared, source, (role, at))
        except ValueError as error:
            fail(f'{at}: {error}')
    return declared


def layouts(provider, config, label, staged, snapshot, preprocess=None, dependency=False):
    directory = config.get('directory', Path('.'))
    roles = {}
    interface_origins = {}
    for path in provider.manifests(directory):
        local(path.as_posix())
        origin = f'{label}/{path.as_posix()}'
        data = snapshot.record(origin, provider.get(path, MAX_MANIFEST))
        if QMR_TAG.search(data):
            if dependency:
                fail(f'{origin}: dependency manifests must be plain declarative text')
            data = snapshot.record(origin + ' (rendered)', preprocess(data, origin))
        for source, (role, at) in local_roles(data, origin, path, config['kind']).items():
            try:
                if role == 'resource' and (
                        dependency or config['kind'] != 'library' or config['name'] != 'std'
                        or source.relative_to(directory) not in STDLIB_RESOURCES):
                    fail('only registered standard-library resources may be packaged as resources')
                relative = source.relative_to(directory) if dependency else source
                unique(roles, relative, role)
                if role == 'interface':
                    interface_origins[relative] = at
                input_label = f'{label}/{source.as_posix()}'
                metadata = role == 'resource' and source.name == 'On1xfile'
                data = snapshot.record(input_label, provider.get(source, MAX_MANIFEST if metadata else MAX_SOURCE))
                if metadata:
                    package = root_manifest(data, input_label, dependency=True)
                    # Discovery uses literal quoted paths/names, unlike the
                    # project's JSON-style string decoder.
                    if b'\\' in data or (package['kind'], package['name'],
                                          package.get('directory', Path('.'))) != ('library', 'mulib', Path('.')):
                        fail('expected a plain MuLIB library discovery manifest at this path')
                stage_bytes(staged / relative, data)
            except (OSError, ValueError) as error:
                fail(f'{at}: {error}')
    if not roles:
        fail(f'{label}: no declared directory-local inputs')
    for interface, at in interface_origins.items():
        implementation = interface.with_suffix('.on1x')
        if roles.get(implementation) not in ('source', 'library'):
            fail(f'{at}: interface {interface.name!r} requires a declared same-directory '
                 f'implementation {implementation.name!r}')
    if config['kind'] == 'library' and 'library' not in roles.values():
        fail(f'{label}: a library requires at least one implementation module')
    for target, source in config['targets'].items():
        if roles.get(source) != 'source':
            fail(f'{label}: target {target!r} must name a locally declared source, not a library or interface')
    if set(config['targets'].values()) != {path for path, role in roles.items() if role == 'source'}:
        fail(f'{label}: every source role must be named by a root target')
    return roles


def dependency_layout(provider, key, stage, snapshot):
    label = f'dependency/{key}'
    manifest = label + '/On1xfile'
    payload = snapshot.record(manifest, provider.get(Path('On1xfile'), MAX_MANIFEST))
    metadata = root_manifest(payload, manifest, dependency=True)
    if metadata['kind'] != 'library' or metadata['name'] != key or key == 'std':
        fail(f'{label}: expected a non-std library certified under the declared dependency name')
    return layouts(provider, metadata, label, stage, snapshot, dependency=True)


def download_git(url, revision, ca_file, work):
    repository = work / 'download.git'
    cwd = work / 'cwd'
    home = work / 'home'
    home.mkdir()
    git = Git.command(repository)
    run([*git, *Git.init(revision)], cwd, env=GIT_ENV, bound=4096, timeout=20, memory=MAX_GIT_MEMORY)
    env = dict(GIT_ENV, HOME=str(home), GIT_ALLOW_PROTOCOL='https', NETRC='/dev/null',
               GIT_ASKPASS='/bin/false', SSH_ASKPASS='/bin/false', GIT_CEILING_DIRECTORIES=str(work))
    options = ['protocol.allow=never', 'protocol.https.allow=always', 'protocol.version=2',
               'http.sslVerify=true', 'http.followRedirects=false', 'http.proxy=',
               'credential.helper=', 'fetch.fsckObjects=true', 'fetch.unpackLimit=1']
    if ca_file is not None:
        trusted_ca = work / 'ca.pem'
        stage_bytes(trusted_ca, read(ca_file.resolve(strict=True), 1024 * 1024))
        options.append('http.sslCAInfo=' + str(trusted_ca))
    try:
        # Git's infinite-depth sentinel requests full history from smart
        # servers while making Git's dumb HTTP fallback refuse the fetch.
        # Keeping the pack applies RLIMIT_FSIZE to the whole pack rather than
        # allowing a stream of loose objects.
        run([*git, *(word for option in options for word in ('-c', option)),
             'fetch', '--quiet', '--no-tags', '--no-recurse-submodules', '--no-auto-maintenance',
             '--no-write-fetch-head', '--no-write-commit-graph', '--no-update-shallow', '--no-filter',
             '--depth=2147483647', '--keep', '--', url, revision], cwd, env=env,
            bound=4096, timeout=GIT_FETCH_TIMEOUT, memory=MAX_GIT_MEMORY)
    except ValueError as error:
        # Escape control characters sent in Git server diagnostics.
        diagnostic = ''.join(c if c.isprintable() or c == '\n' else ascii(c)[1:-1] for c in str(error))
        fail(f'HTTPS Git fetch of {revision} failed: {diagnostic}')
    if (repository / 'shallow').exists():
        fail('remote Git dependency requires complete history, not a shallow object store')
    return repository


def fetch_dependency(args):
    project = args.root.resolve(strict=True)
    if not project.is_dir():
        fail(f'{project}: expected project directory')
    manifest = project / 'On1xfile'
    config = root_manifest(read(manifest, MAX_MANIFEST), manifest)
    key = name(args.name, IDENT)
    if key not in config['remotes']:
        fail(f'{key!r} is not a declared git-remote dependency')
    if key == 'std' or config['kind'] == 'library':
        fail('cannot fetch a dependency named std or a dependency of a standalone library')
    _, relative, revision = config['deps'][key]
    dest = contained(project, relative)
    # Same-filesystem, no-clobber publication; as with other local inputs, no
    # protection is claimed against hostile concurrent writers in the project.
    rename = getattr(ctypes.CDLL(None, use_errno=True), 'renameat2', None)
    if rename is None:
        fail('atomic dependency publication requires Linux renameat2')
    dest.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=dest.parent, prefix='.fetch-') as tmp:
        work = Path(tmp)
        cwd = work / 'cwd'
        cwd.mkdir()
        present = dest.exists()
        repository = dest if present else download_git(config['remotes'][key], revision, args.ca_file, work)
        checked = work / 'checked.git'
        provider = Git(repository, revision, cwd, checked, deadline=time.monotonic() + GIT_VALIDATION_TIMEOUT)
        dependency_layout(provider, key, work / 'declared', Snapshot())
        if not present:
            stage = work / 'store'
            stage.mkdir(mode=0o700)
            (checked / 'objects').rename(stage / 'objects')
            # Publish only the offline validator's object projection: no refs,
            # remotes/config, hooks or checked-out source files.
            if rename(-100, os.fsencode(stage), -100, os.fsencode(dest), 1):  # RENAME_NOREPLACE
                code = ctypes.get_errno() or errno.EIO
                raise OSError(code, os.strerror(code), dest)
    print(dest)


def quoted_import(file, spelling, root, libraries):
    parts = spelling.split('/')
    parents = 0
    while parents < len(parts) and parts[parents] == '..':
        parents += 1
    tail = local('/'.join(parts[parents:]))
    if len(spelling) > 1024 or len(parts) > 32:
        fail(f'{file}: quoted import exceeds the local path budget')
    if tail.suffix != '.on1x':
        fail(f'{file}: quoted imports must name a local .on1x file')
    base = file.parent
    if parents > len(base.relative_to(root).parts):
        fail(f'{file}: relative import escapes the declared package: {spelling!r}')
    for _ in range(parents):
        base = base.parent
    if base / tail not in libraries:
        fail(f'{file}: quoted import must name a declared library in the same package: {spelling!r}')


def source_policy(executor, compiler, file, root, libraries):
    listed = executor.run([compiler, '--tokens', executor.path(file)], bound=16 * MAX_SOURCE)
    # Use the canonical scanner. Only the exact directive token pair matters:
    # comments, string data and hygienic syntax are not regex-scanned as source.
    tokens = []
    for line in listed.split(b'\n'):
        if re.match(rb'^[0-9]+:[0-9]+  ', line):
            tokens.append(line.split(b'  ', 1)[1])
    for i in range(len(tokens) - 1):
        if tokens[i] == b'@' and tokens[i + 1] in (b'ppx', b'enable', b'extension'):
            fail(f'{file}: source PPX and executable extensions are not available in project builds')
        if tokens[i] == b'import' and tokens[i + 1].startswith(b'"'):
            quoted_import(file, tokens[i + 1][1:-1].decode('utf-8'), root, libraries)
        if tokens[i] == b'from' and tokens[i + 1] in (b'.', b'...'):
            # Match the loader's relative `from` depth: one dot stays at
            # this module's directory; each additional dot ascends a level.
            depth, j = 0, i + 1
            while j < len(tokens) and tokens[j] in (b'.', b'...'):
                depth += len(tokens[j])
                j += 1
            if depth > len(file.parent.relative_to(root).parts) + 1:
                fail(f'{file}: relative import escapes the declared package')


def verify_graph(executor, compiler, source, flags, permitted, trusted_roots):
    data = executor.run([compiler, '--deps', source, *flags], bound=8 * MAX_MANIFEST)
    for item in data.decode('utf-8').splitlines():
        path = Path(item)
        if path not in permitted and not any(path.is_relative_to(root) for root in trusted_roots):
            fail(f'{source}: import is outside the declared module graph: {item}')


def verify_interfaces(executor, compiler, staged, roles, flags, permitted, trusted_roots, graph_checked=()):
    for path in sorted(path for path, role in roles.items() if role == 'interface'):
        entry = executor.path(staged / path.with_suffix('.on1x'))
        if entry not in graph_checked:
            verify_graph(executor, compiler, entry, flags, permitted | {entry}, trusted_roots)
        executor.run([compiler, '--check-interfaces', entry, '--interface',
                      executor.path(staged / path), *flags])


def publish(stage, output, report):
    if output.is_symlink() or output.exists() and not output.is_dir():
        fail(f'{output}: expected a project output directory')
    if output.is_dir():
        marker = output / '.on1x-build.json'
        info = json.loads(read(marker, 2 * 1024 * 1024))
        if not isinstance(info, dict) or any(info.get(key) != report[key]
                                             for key in ('schema', 'project', 'kind', 'profile')):
            fail(f'{output}: existing directory is not a build output for this project kind/name/profile')
    libc = ctypes.CDLL(None, use_errno=True)
    rename = getattr(libc, 'renameat2', None)
    if rename is None:
        fail('atomic project output publication requires Linux renameat2')
    # Exchange whole generations; a failed build never leaves a half bundle.
    flags = 2 if output.is_dir() else 1  # RENAME_EXCHANGE / RENAME_NOREPLACE
    if rename(-100, os.fsencode(stage), -100, os.fsencode(output), flags):
        code = ctypes.get_errno()
        raise OSError(code or errno.EIO, os.strerror(code or errno.EIO), output)


def build(args):
    if args.sandbox and args.allow_shell:
        fail('--sandbox cannot be combined with --allow-shell')
    project = args.root.resolve(strict=True)
    if not project.is_dir():
        fail(f'{project}: expected project directory')
    compiler = program(args.on1xc or (str(ROOT / 'build/on1xc') if (ROOT / 'build/on1xc').is_file() else 'on1xc'))
    mulib = (args.mulib_root or ROOT.parent).resolve()
    snapshot = Snapshot()
    disk = Disk(project)
    rootinput = snapshot.record('project/On1xfile', disk.get(Path('On1xfile'), MAX_MANIFEST))
    config = root_manifest(rootinput, project / 'On1xfile')
    if args.project_command:
        if args.profile or args.release:
            fail('a named On1xfile command cannot be combined with --profile or --release')
        selected = config['commands'].get(args.project_command)
        if selected:
            if (args.sandbox or args.allow_shell or args.target or args.output_dir or args.stdlib
                    or args.cc != 'cc' or args.mulib_root or args.libstone_root or args.qmr_compiler
                    or args.mune_runner):
                fail('project formatting commands do not take build options')
            target, source = selected
            if not source.is_relative_to(config.get('directory', Path('.'))):
                fail(f'{source}: formatter input is outside the declared project directory')
            input_path = contained(project, source)
            disk.get(source, MAX_SOURCE)  # Reject non-regular, oversized or linked inputs before invoking the compiler.
            sys.stdout.buffer.write(run([compiler, 'fmt', input_path, '--target', target], project))
            return
        args.profile = args.project_command
    cc = program(args.cc)
    if config['kind'] == 'library' and (config['deps'] or config['kernels']):
        fail('standalone source libraries cannot declare dependencies or kernels')
    directory = config.get('directory', Path('.'))
    if any(directory.is_relative_to(path) for _, path, _ in config['deps'].values()):
        fail('the project source directory cannot be inside a dependency store')
    chosen = args.profile or ('release' if args.release else 'dev')
    name(chosen)
    if args.target and args.target not in config['targets']:
        fail(f'unknown target {args.target!r}')
    targets = {key: source for key, source in config['targets'].items() if not args.target or args.target == key}
    dest = (args.output_dir or contained(project, Path('target') / chosen)).absolute()
    if dest == project or project.is_relative_to(dest) or dest.name in ('', '.', '..'):
        fail(f'invalid output directory: {dest}')
    dest = dest.parent.resolve() / dest.name
    if dest == project or project.is_relative_to(dest):
        fail(f'invalid output directory: {dest}')
    disk.exclude.update([dest, *(project / path for _, path, _ in config['deps'].values())])
    with tempfile.TemporaryDirectory(prefix='on1x-project-') as tmp:
        work = Path(tmp)
        (work / 'input/project').mkdir(parents=True)
        (work / 'input/deps').mkdir()
        (work / 'cwd').mkdir()
        (work / 'artifact').mkdir()
        std_package = config['kind'] == 'library' and config['name'] == 'std'
        stdlib = None
        if not std_package:
            stdlib_args = ['--stdlib', str(args.stdlib.resolve())] if args.stdlib else []
            stdlib = Path(run([compiler, '--print-stdlib', *stdlib_args], work / 'cwd',
                              bound=4096).decode().strip()).resolve(strict=True)
        executor = Execution(args, work, compiler, cc, stdlib, mulib)
        chosen_tools = {'on1xc': compiler, 'cc': cc}
        model = {'profile': chosen, 'project': config['name'], 'kind': config['kind']}
        for key, (kind, value) in sorted(config['defines'].items()):
            if key in model:
                fail(f'define {key!r} is reserved')
            if kind == 'shell':
                if not args.allow_shell:
                    fail(f'define {key!r} requires explicit --allow-shell')
                value = run(['/bin/sh', '-c', value], project, bound=4096, timeout=5).decode('utf-8').rstrip('\n')
                if '\x00' in value:
                    fail(f'define {key!r}: shell result contains NUL')
                snapshot.record(f'shell/{key}', value.encode())
            model[key] = value

        def mune(mode, data, origin):
            runner = selected_tool(args, 'mune_runner', 'on1x-mune-runner', compiler)
            executor.mount(runner)
            chosen_tools['mune'] = runner
            library = 'profiles.lua' if mode == 'profile' else 'render.lua'
            library_data = read(POLICY / library, MAX_MANIFEST)
            if 'stdlib/' + library not in snapshot.inputs:
                snapshot.record('stdlib/' + library, library_data)
            policy_file = work / 'input' / library
            if not policy_file.exists():
                stage_bytes(policy_file, library_data)
            with tempfile.NamedTemporaryFile(dir=work / 'input', suffix='.lua') as stream:
                stream.write(data)
                stream.flush()
                try:
                    return executor.run([runner, mode, executor.path(policy_file), executor.path(Path(stream.name)),
                                         *(word for pair in sorted(model.items()) for word in pair)],
                                        bound=MAX_MANIFEST, timeout=10)
                except ValueError as error:
                    fail(f'{origin}: {error}')

        def preprocess(data, origin):
            helper = selected_tool(args, 'qmr_compiler', 'on1x-qmr-compile', compiler, optional=True)
            if helper is None and args.libstone_root:
                libstone = args.libstone_root.resolve()
                qmr = libstone / 'moonstone/qmr'
                cxx = program('c++')
                if not (qmr / 'src/Compiler.cpp').is_file():
                    fail(f'{qmr}: QMR compiler source unavailable')
                helper = work / 'on1x-qmr-compile'
                if not helper.exists():
                    if args.sandbox:
                        fail('--sandbox requires a prebuilt --qmr-compiler or SDK QMR helper')
                    run([cxx, '-std=c++20', '-I', qmr / 'include', POLICY / 'qmr_compile.cpp',
                         qmr / 'src/Compiler.cpp', '-o', helper], work / 'cwd')
            if helper is None:
                fail(f'{origin}: QMR requires --qmr-compiler, --libstone-root, or an SDK QMR helper')
            if not helper.is_relative_to(work):
                executor.mount(helper)
            chosen_tools['qmr'] = helper
            lua = executor.run([helper, origin], data=data, bound=8 * MAX_MANIFEST, timeout=10)
            return mune('render', lua, origin)

        if chosen in ('dev', 'release'):
            setting = (0, 1) if chosen == 'dev' else (3, 0)
        else:
            path = config['profiles'].get(chosen)
            if path is None:
                fail(f'unknown profile {chosen!r}')
            data = snapshot.record(f'project/{path.as_posix()}', disk.get(path, MAX_MANIFEST))
            result = mune('profile', data, project / path)
            if not re.fullmatch(rb'[0-3] [01]\n', result):
                fail('invalid Mune profile protocol response')
            setting = tuple(map(int, result.split()))
        cflags = [f'-O{setting[0]}', *(['-g'] if setting[1] else [])]
        project_stage = work / 'input/project'
        deps_stage = work / 'input/deps'
        roles = layouts(disk, config, 'project', project_stage, snapshot, preprocess)
        dep_modules = {}
        dep_roles = {}
        dep_report = {}
        for key, (kind, relative, revision) in sorted(config['deps'].items()):
            location = contained(project, relative)
            if kind == 'git-remote' and not location.exists():
                fail(f'{location}: remote Git objects are missing; run on1x-project.py fetch {key} explicitly')
            stage = deps_stage / key
            with tempfile.TemporaryDirectory(dir=work, prefix='git-') as objects:
                provider = (Git(location, revision, work / 'cwd', Path(objects) / 'git')
                            if kind in ('git', 'git-remote') else Disk(location))
                entries = dependency_layout(provider, key, stage, snapshot)
            dep_modules[key] = [stage / path for path, role in entries.items() if role == 'library']
            dep_roles[key] = entries
            dep_report[key] = {'kind': kind, **({'commit': revision} if revision else {})}

        project_modules = {project_stage / path for path, role in roles.items() if role == 'library'}
        project_programs = [project_stage / path for path, role in roles.items() if role in ('source', 'library')]
        for path in sorted(project_programs):
            source_policy(executor, compiler, path, project_stage / directory, project_modules)
        for key, paths in dep_modules.items():
            own_files = set(paths)
            for path in sorted(paths):
                source_policy(executor, compiler, path, deps_stage / key, own_files)
        dep_sources = [path for paths in dep_modules.values() for path in paths]
        # A std package must use its own staged pervasive module and imports,
        # not fill missing declarations from the installed standard library.
        standard = executor.path(project_stage / directory) if std_package else stdlib
        trusted_roots = () if std_package else (stdlib, stdlib.parent / 'std')
        loader = ['--stdlib', str(standard), '-I', str(executor.path(deps_stage)),
                  '-I', str(executor.path(project_stage))]
        dependency_loader = loader[:-2]
        for key, modules in dep_modules.items():
            own_files = {executor.path(path) for path in modules}
            for entry in sorted(own_files):
                verify_graph(executor, compiler, entry, dependency_loader, own_files, trusted_roots)
            verify_interfaces(executor, compiler, deps_stage / key, dep_roles[key],
                              dependency_loader, own_files, trusted_roots, graph_checked=own_files)
        permitted = {executor.path(path) for path in [*project_modules, *dep_sources]}
        verify_interfaces(executor, compiler, project_stage, roles, loader, permitted, trusted_roots)
        outputs = {}
        for key, source in sorted(targets.items()):
            entry = executor.path(project_stage / source)
            verify_graph(executor, compiler, entry, loader, permitted | {entry}, trusted_roots)
            output = executor.path(work / 'artifact' / key)
            command = [sys.executable, ROOT / 'tools/on1x-build.py', entry, '-o', output,
                       '--on1xc', compiler, '--cc', cc, '--mulib-root', mulib, *loader,
                       *(f'--cflag={flag}' for flag in cflags)]
            if setting[1]:
                command.extend(('--debug-build-root', executor.path(work)))
            for kernel in config['kernels']:
                command.extend(('--mulib', kernel))
            if config['kernels']:
                command.extend(('--mulib-plan', executor.path(work / 'artifact' / (key + '.mulib.json'))))
            executor.mount(Path(sys.executable))
            executor.run(command, timeout=180)
            outputs[key] = digest(read(work / 'artifact' / key, MAX_PROCESS))
        if config['kind'] == 'library':
            # The language checker requires a main. Validate each project library
            # through a fresh declaration-only carrier, then publish source only.
            for i, path in enumerate(sorted(project_modules)):
                check = work / 'input' / f'check_{i}.on1x'
                stage_bytes(check, f'import "project/{path.relative_to(project_stage).as_posix()}";\n'
                                   'fn main() -> i64 { return 0; }\n'.encode())
                entry = executor.path(check)
                verify_graph(executor, compiler, entry, loader, permitted | {entry}, trusted_roots)
                executor.run([compiler, '--check', entry, *loader])
            package = work / 'artifact' / 'library'
            shutil.copytree(project_stage, package)
            # Preserve checked interface bytes, but publish only plain,
            # post-preprocessing declarations with the certified role set.
            directory = config.get('directory', Path('.')).as_posix()
            certificate = (f'on1xfile 1\nkind library\nname {json.dumps(config["name"])}\n'
                           f'directory {json.dumps(directory)}\n')
            stage_bytes(package / 'On1xfile', certificate.encode())
            for directory in sorted({path.parent for path in roles}):
                for suffix, accepted in (('m', ('library', 'resource')), ('i', ('interface',))):
                    inputs = sorted((path.name, role) for path, role in roles.items()
                                    if path.parent == directory and role in accepted)
                    if inputs:
                        declarations = ''.join(f'{role} {json.dumps(path)}\n' for path, role in inputs)
                        stage_bytes(package / directory / ('On1xfile.' + suffix),
                                    ('on1xfile 1\n' + declarations).encode())

        report = dict(schema='on1x.project-build/v1', project=config['name'], kind=config['kind'],
                      profile=chosen, cflags=cflags, sandbox=bool(args.sandbox), shell=bool(args.allow_shell),
                      dependencies=dep_report, inputs=snapshot.inputs, outputs=outputs,
                      tools={key: digest(read(path, MAX_PROCESS)) for key, path in chosen_tools.items()})
        stage_bytes(work / 'artifact/.on1x-build.json',
                    (json.dumps(report, indent=2, sort_keys=True) + '\n').encode())
        dest.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=dest.parent, prefix='.on1x-project-') as publish_dir:
            publish_dir = Path(publish_dir) / 'output'
            shutil.copytree(work / 'artifact', publish_dir)
            publish(publish_dir, dest, report)
    print(dest)


def format_manifest(args):
    path = args.input
    data = read(path, MAX_MANIFEST)
    if path.name == 'On1xfile':
        root_manifest(data, path)
    elif path.name in ('On1xfile.m', 'On1xfile.i'):
        if QMR_TAG.search(data):
            fail(f'{path}: formatting QMR templates is unsupported; no template code was executed')
        local_roles(data, path, Path(path.name), None)
    else:
        fail(f'{path}: expected On1xfile, On1xfile.m, or On1xfile.i')
    lines = []
    for line in data.decode('utf-8').split('\n'):
        words, pos = [], 0
        while pos < len(line):
            if line[pos] in ' \t\r':
                pos += 1
            elif line[pos] == '#':
                words.append(line[pos:].rstrip())
                break
            else:
                match = TOKEN.match(line, pos)
                word = match[0]  # Full syntax has been validated above.
                words.append(json.dumps(json.loads(word), ensure_ascii=False) if word.startswith('"') else word)
                pos = match.end()
        lines.append(' '.join(words))
    while lines and not lines[-1]:
        lines.pop()
    formatted = ('\n'.join(lines) + '\n').encode('utf-8')
    if args.check:
        if data != formatted:
            fail(f'{path}: not canonically formatted')
    else:
        sys.stdout.buffer.write(formatted)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    command = commands.add_parser('build')
    command.add_argument('root', type=Path, nargs='?', default=Path('.'))
    command.add_argument('--on1xc')
    command.add_argument('--cc', default='cc')
    command.add_argument('--stdlib', type=Path)
    command.add_argument('--mulib-root', type=Path)
    command.add_argument('--libstone-root', type=Path)
    command.add_argument('--mune-runner')
    command.add_argument('--qmr-compiler')
    mode = command.add_mutually_exclusive_group()
    mode.add_argument('--profile')
    mode.add_argument('--release', action='store_true')
    command.add_argument('--target')
    command.add_argument('--project-command', help=argparse.SUPPRESS)
    command.add_argument('--output-dir', type=Path, help='exact generation output directory')
    command.add_argument('--sandbox', action='store_true', help='require Linux bubblewrap isolation; no fallback')
    command.add_argument('--allow-shell', action='store_true', help='allow explicit root shell defines outside sandbox')
    formatting = commands.add_parser('format', help='validate and format a plain On1xfile without running its policy')
    formatting.add_argument('input', type=Path)
    formatting.add_argument('--check', action='store_true')
    fetch = commands.add_parser('fetch', help='explicitly fetch one pinned git-remote library over HTTPS')
    fetch.add_argument('name', help='dependency name declared in On1xfile')
    fetch.add_argument('root', type=Path, nargs='?', default=Path('.'))
    fetch.add_argument('--ca-file', type=Path, help='explicit trusted PEM CA bundle for this fetch')
    try:
        arguments = parser.parse_args()
        {'build': build, 'format': format_manifest, 'fetch': fetch_dependency}[arguments.command](arguments)
    except (ValueError, OSError, KeyError, TypeError) as error:
        print(f'on1x-project: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
