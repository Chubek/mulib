#!/usr/bin/env python3
"""Build a declared On1x project without running dependency build programs."""
import argparse
from contextlib import contextmanager
import ctypes
from dataclasses import dataclass
import errno
import hashlib
import importlib.util
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

# The SDK also loads this engine by filename under isolated Python (-I).
# Always use our shipped sibling, independent of sys.path or the project cwd.
_opt_spec = importlib.util.spec_from_file_location(
    '_on1x_optimization', Path(__file__).resolve().with_name('on1x_optimization.py'))
optimization = importlib.util.module_from_spec(_opt_spec)
_opt_spec.loader.exec_module(optimization)
_tools_spec = importlib.util.spec_from_file_location(
    '_on1x_toolchain', Path(__file__).resolve().with_name('on1x_toolchain.py'))
toolchain = importlib.util.module_from_spec(_tools_spec)
_tools_spec.loader.exec_module(toolchain)
_dotenv_spec = importlib.util.spec_from_file_location(
    '_on1x_dotenv', Path(__file__).resolve().with_name('on1x_dotenv.py'))
dotenv = importlib.util.module_from_spec(_dotenv_spec)
_dotenv_spec.loader.exec_module(dotenv)
_proxy_spec = importlib.util.spec_from_file_location(
    '_on1x_proxy', Path(__file__).resolve().with_name('on1x_proxy.py'))
proxy = importlib.util.module_from_spec(_proxy_spec)
_proxy_spec.loader.exec_module(proxy)

try:
    import fcntl
except ImportError:  # Non-POSIX hosts can still validate/format manifests.
    fcntl = None

ROOT = Path(__file__).resolve().parents[1]
POLICY = ROOT / 'stdlib/on1xfile'
FFI_HANDLER = ROOT / 'hir/FFIHandler'
FFI_CEPHYR = ROOT / 'third_party/libstone/cornerstone/Cephyr'
MAX_MANIFEST = 64 * 1024
BACKENDS = ('c11', 'mir-c11', 'mir-vm', 'mir-archon')
VM_POLICIES = ('opt-in', 'always')
VM_DIRECTIVE = re.compile(rb'^[ \t\r]*vm(?=[ \t\r#]|\\[ \t\r]*$|$)')
VM_COMMANDS = ('run', 'vm-rerun', 'vm-cache-clear')
FFI_GENERATE = 'ffi-generate'
KERNEL_PATH_DIRECTIVE = re.compile(rb'^[ \t\r]*kernel-path(?=[ \t\r#]|\\[ \t\r]*$|$)')
KERNEL_LINK_DIRECTIVE = re.compile(rb'^[ \t\r]*kernel-link(?=[ \t\r#]|\\[ \t\r]*$|$)')
MAX_KERNEL_PATHS = 32
MAX_LOCKFILE = 256 * 1024
MAX_SOURCE = 2 * 1024 * 1024
MAX_INPUT = 64 * 1024 * 1024
MAX_FILES = 4096
MAX_MODULES = 1024
MAX_MODULE_EDGES = 16384
# A source package can record thousands of paths in both inputs and outputs.
MAX_BUILD_REPORT = 16 * 1024 * 1024
MAX_PROCESS = 64 * 1024 * 1024
MAX_GIT_MEMORY = 512 * 1024 * 1024
MAX_GIT_OBJECT_FILES = 32768
MAX_GIT_OBJECT_BYTES = 128 * 1024 * 1024
MAX_GIT_REFS = 4 * 1024 * 1024
GIT_FETCH_TIMEOUT = 30
GIT_VALIDATION_TIMEOUT = 30
NAME = re.compile(r'[A-Za-z_][A-Za-z0-9_-]{0,63}\Z')
IDENT = re.compile(r'[A-Za-z_][A-Za-z0-9_]{0,63}\Z')
MODULE_NAME = re.compile(r'[A-Za-z_][A-Za-z0-9_]{0,63}(?:\.[A-Za-z_][A-Za-z0-9_]{0,63}){0,31}\Z')
MODULE_DIRECTIVE = re.compile(rb'^[ \t\r]*(?:program|module(?:-interface(?:-policy)?)?)(?=[ \t\r#]|\\[ \t\r]*$|$)')
PART = re.compile(r'[A-Za-z_0-9][A-Za-z_0-9. -]{0,127}\Z')
TOKEN = re.compile(r'"(?:[^"\\\r\n]|\\.)*"|[A-Za-z0-9_-]+')
# Reserve heads even where the directive is forbidden; validation decides
# authority, not the lexer. Keep compiler package discovery in sync.
HEADS = frozenset((
    'on1xfile', 'kind', 'name', 'directory', 'vm', 'scrutiny', 'scrutiny-work',
    'optimization-profile', 'optimization', 'backend', 'target', 'dependency', 'profile',
    'profile-arg', 'command', 'define', 'dotenv', 'kernel', 'kernel-path', 'kernel-link',
    'source', 'library', 'resource', 'interface', 'module', 'module-interface', 'module-interface-policy', 'program',
)) | toolchain.DIRECTIVES | proxy.DIRECTIVES
ROW_HINT = ('start each intended directive with a known unquoted head on its own line; '
            'quote string fields, or move a directive-looking bare field to the preceding line '
            'or use a trailing backslash on that line')
QMR_TAG = re.compile(rb'\{[{%#]')
ENV = {'PATH': '/usr/bin:/bin', 'LC_ALL': 'C', 'TZ': 'UTC',
       'HOME': '/nonexistent', 'SOURCE_DATE_EPOCH': '0', 'PYTHONDONTWRITEBYTECODE': '1'}
GIT_ENV = dict(ENV, GIT_CONFIG_NOSYSTEM='1', GIT_CONFIG_GLOBAL='/dev/null',
               GIT_ALLOW_PROTOCOL='', GIT_NO_LAZY_FETCH='1', GIT_TERMINAL_PROMPT='0')


@dataclass(frozen=True)
class Dependency:
    kind: str
    source: object
    selector_kind: object = None
    selector: object = None


@dataclass(frozen=True)
class ModuleMembership:
    sources: tuple
    interfaces: tuple
    interface_policy: str


def fail(message):
    raise ValueError(str(message))


def scrutiny_work(value):
    # Match the compiler's accepted range without parsing arbitrary-size integers.
    digits = value.lstrip('0')
    if 1 <= len(digits) <= 8 and re.fullmatch(r'[0-9]+', value):
        budget = int(digits)
        if budget <= 10_000_000:
            return budget
    fail('scrutiny-work requires a decimal integer from 1 to 10000000')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def tool_digest(path):
    # Debug compiler/runners can exceed the source/process-output budget.
    with path.open('rb') as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_size > 512 * 1024 * 1024:
            fail(f'{path}: invalid or oversized tool')
        result = hashlib.sha256()
        remaining = info.st_size
        while remaining:
            chunk = stream.read(min(1024 * 1024, remaining))
            if not chunk:
                fail(f'{path}: tool changed while fingerprinting')
            result.update(chunk)
            remaining -= len(chunk)
        if stream.read(1):
            fail(f'{path}: tool changed while fingerprinting')
        return result.hexdigest()


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


def read(path, limit, *, dir_fd=None):
    # No following the final component, and no blocking on pipes. Parent paths
    # are checked separately; this is not a concurrent-worktree security lock.
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=dir_fd)
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


def paths_overlap(a, b):
    return a.is_relative_to(b) or b.is_relative_to(a)


def manifest_span(origin, start, end):
    return f'{origin}:{start}' if start == end else f'{origin}:{start}-{end}'


def manifest_line(line):
    words, pos = [], 0
    while pos < len(line):
        if line[pos] in ' \t\r':
            pos += 1
            continue
        if line[pos] == '#':
            return words, line[pos:].rstrip(), False
        if line[pos] == '\\':
            if line[pos + 1:].strip(' \t\r'):
                fail('a continuation backslash must be followed only by whitespace')
            if not words:
                fail('a continuation backslash must follow a field on the same line')
            return words, '', True
        match = TOKEN.match(line, pos)
        if not match:
            if line[pos] == '"':
                fail('unterminated quoted string; a quoted field must stay on one physical line')
            fail('expected a word or quoted string')
        word = match[0]
        quoted = word.startswith('"')
        try:
            value = json.loads(word) if quoted else word
            value.encode('utf-8')
        except (UnicodeError, ValueError) as error:
            fail(f'invalid string: {error}')
        if '\x00' in value or len(value) > 4096:
            fail('invalid or overlong field')
        words.append((value, quoted))
        pos = match.end()
        if pos != len(line) and line[pos] not in ' \t\r#\\':
            fail('fields must be separated')
    return words, '', False


def manifest_lines(payload, origin):
    if len(payload) > MAX_MANIFEST or b'\x00' in payload:
        fail(f'{origin}: manifest byte budget exceeded or contains NUL')
    try:
        text = payload.decode('utf-8')
    except UnicodeDecodeError as error:
        number = 1 + payload.count(b'\n', 0, error.start)
        fail(f'{origin}:{number}: invalid manifest UTF-8')
    start = end = 0
    forced = False
    for number, line in enumerate(text.split('\n'), 1):
        line = line.lstrip(' \t\r')
        leading = TOKEN.match(line)
        # Decide the boundary before lexing the rest of the line so even a
        # malformed field can be attributed to its statement.
        head = not forced and leading is not None and leading[0] in HEADS
        try:
            words, comment, continued = manifest_line(line)
        except ValueError as error:
            at = manifest_span(origin, number if head else start or number, number)
            fail(f'{at}: {error}')
        if words:
            if head or not start:
                start = number
            end = number
            forced = continued
        yield number, words, head, comment, continued
    if forced:
        fail(f'{manifest_span(origin, start, end)}: dangling continuation backslash; '
             'add a field on the next nonblank, noncomment line or remove the backslash')


def manifest_statements(payload, origin):
    words = []
    for number, line_words, head, _, _ in manifest_lines(payload, origin):
        if not line_words:
            continue
        if head and words:
            yield start, end, words
            words = []
        if not words:
            start = number
        words.extend(line_words)
        end = number
    if words:
        yield start, end, words


def rows(payload, origin):
    statements = manifest_statements(payload, origin)
    header = next(statements, None)
    if header is None:
        fail(f'{origin}: missing on1xfile 1')
    start, end, words = header
    if words != [('on1xfile', False), ('1', False)]:
        fail(f'{manifest_span(origin, start, end)}: expected on1xfile 1; {ROW_HINT}')
    for start, end, words in statements:
        if words[0][0] == 'on1xfile':
            fail(f'{manifest_span(origin, start, end)}: duplicate on1xfile header')
        yield start, end, words[0][0], words[1:]


class ManifestFieldsError(ValueError):
    pass


def row_error(at, start, end, error):
    hint = f'; {ROW_HINT}' if start != end or isinstance(error, ManifestFieldsError) else ''
    fail(f'{at}: {error}{hint}')


def fields(args, signature):
    if len(args) != len(signature) or any(q != (kind == '"') for (_, q), kind in zip(args, signature)):
        raise ManifestFieldsError('missing, extra or incorrectly quoted fields')
    return [value for value, _ in args]


def unique(items, key, value):
    if key in items:
        fail(f'duplicate declaration {key!s}')
    items[key] = value


def root_only_vm(origin=None):
    at = f'{origin}: ' if origin is not None else ''
    fail(f'{at}vm policy is only allowed in the root On1xfile of an executable or complex project')


def root_only_kernel_path(origin=None):
    at = f'{origin}: ' if origin is not None else ''
    fail(f'{at}kernel-path is only allowed in the root project On1xfile')


def root_only_kernel_link(origin=None):
    at = f'{origin}: ' if origin is not None else ''
    fail(f'{at}kernel-link is only allowed in the root On1xfile of an executable or complex project')


def check_nested_authority(payload, origin):
    # Nested manifests are inert and may be templates. Look only for literal
    # root-only heads, honoring explicit continuations we can lex as plain text.
    forced = False
    for number, line in enumerate(payload.split(b'\n'), 1):
        code = line.strip(b' \t\r')
        if not code or code.startswith(b'#'):
            continue
        if not forced:
            if VM_DIRECTIVE.match(line):
                root_only_vm(f'{origin}:{number}')
            if KERNEL_PATH_DIRECTIVE.match(line):
                root_only_kernel_path(f'{origin}:{number}')
            if KERNEL_LINK_DIRECTIVE.match(line):
                root_only_kernel_link(f'{origin}:{number}')
            if MODULE_DIRECTIVE.match(line):
                fail(f'{origin}:{number}: module membership is only allowed in the root On1xfile')
            if toolchain.ROOT_ONLY.match(line):
                fail(f'{origin}:{number}: native toolchain directives are only allowed in the root project On1xfile')
        forced = False
        if code.endswith(b'\\'):
            try:
                _, _, forced = manifest_line(code.decode('utf-8'))
            except ValueError:
                pass  # Do not suppress a potential head after malformed text.


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


def git_object_id(value, message='git dependency requires a full lowercase commit object ID'):
    if not isinstance(value, str) or not re.fullmatch(r'[0-9a-f]{40}|[0-9a-f]{64}', value):
        fail(message)
    return value


def git_ref(kind, value):
    if kind not in ('branch', 'tag'):
        fail('git selector must be commit, branch or tag')
    # Use an intentionally conservative portable subset of Git ref names. The
    # fully qualified ref is still passed after `--`; this validation also
    # keeps lockfiles and diagnostics unambiguous.
    if (not isinstance(value, str) or len(value) > 240
            or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._/+-]*', value)
            or value == '@' or value.endswith(('/', '.')) or '//' in value
            or '..' in value or '@{' in value
            or any(part.startswith('.') or part.endswith('.lock') for part in value.split('/'))):
        fail(f'invalid Git {kind} name {value!r}')
    return f'refs/{"heads" if kind == "branch" else "tags"}/{value}'


def git_selector(kind, value):
    if kind == 'commit':
        return git_object_id(value)
    git_ref(kind, value)
    return value


def profile_argument(args):
    if len(args) != 4:
        raise ManifestFieldsError('expected profile-arg "profile" key string|integer|boolean value')
    kind = args[2][0]
    scope, key, kind, value = fields(args, '"ww"' if kind == 'string' else '"www')
    if kind == 'integer':
        if (not re.fullmatch(r'0|-?[1-9][0-9]{0,18}', value)
                or not -(1 << 63) <= int(value) < (1 << 63)):
            fail('profile-arg integer must be a signed 64-bit decimal integer without leading zeros or negative zero')
    elif kind == 'boolean':
        if value not in ('true', 'false'):
            fail('profile-arg boolean must be true or false')
    elif kind != 'string':
        fail('profile-arg type must be string, integer or boolean')
    # The bounded Mune adapter consumes the validated typed literal directly.
    return name(scope), name(key, IDENT), (kind, value)


def root_manifest(payload, origin, dependency=False):
    options, targets, deps, defines, profiles, kernels, commands = {}, {}, {}, {}, {}, {}, {}
    selections, scoped, backend_scopes, profile_args = {}, {}, {}, {}
    kernel_paths = {}
    modules, module_paths, programs = {}, {}, {}
    for start, end, verb, args in rows(payload, origin):
        at = manifest_span(origin, start, end)
        try:
            if verb == 'kind':
                value, = fields(args, 'w')
                if value not in ('executable', 'complex', 'library'):
                    fail(f'unsupported project kind {value!r}')
                unique(options, verb, value)
            elif verb in ('name', 'directory'):
                value, = fields(args, '"')
                unique(options, verb, name(value) if verb == 'name' else local(value, dot=True))
            elif verb == 'dotenv' and not dependency:
                if len(args) == 1:
                    path, = fields(args, '"')
                    mode = 'override'
                else:
                    path, mode = fields(args, '"w')
                    if mode not in ('override', 'preserve'):
                        fail('dotenv mode must be override or preserve')
                unique(options, verb, (dotenv.local_path(path), mode))
            elif verb in proxy.DIRECTIVES and not dependency:
                if verb == 'proxy-none':
                    fields(args, '')
                    unique(options, verb, True)
                else:
                    value, = fields(args, '"')
                    unique(options, verb, proxy.manifest_url(verb, value))
                proxy.validate_manifest(options)
            elif verb in toolchain.DIRECTIVES:
                if dependency:
                    fail('native toolchain directives are only allowed in the root project On1xfile')
                if verb in toolchain.CHOICES:
                    value, = fields(args, 'w')
                    if value not in toolchain.CHOICES[verb]:
                        fail(f'invalid {verb}: {value!r}; choose from {", ".join(toolchain.CHOICES[verb])}')
                    unique(options, verb, value)
                else:
                    value, = fields(args, '"')
                    argv = options.setdefault(verb, [])
                    if len(argv) >= 64:
                        fail(f'too many {verb} directives (maximum 64)')
                    argv.append(toolchain.arg_token(value))
            elif verb == 'vm':
                if dependency:
                    root_only_vm()
                value, = fields(args, 'w')
                if value not in VM_POLICIES:
                    fail('vm policy must be opt-in or always')
                unique(options, verb, value)
            elif verb == 'scrutiny' and not dependency:
                value, = fields(args, 'w')
                if not re.fullmatch(r'[a-z][a-z0-9-]{0,63}', value):
                    fail('invalid scrutiny level')
                unique(options, verb, value)
            elif verb == 'scrutiny-work' and not dependency:
                value, = fields(args, 'w')
                unique(options, verb, scrutiny_work(value))
            elif verb == 'optimization-profile' and not dependency:
                value, = fields(args, 'w')
                if value not in optimization.PROFILES:
                    fail(f'unknown optimization profile: {value}')
                unique(options, verb, value)
            elif verb == 'optimization' and not dependency:
                if len(args) == 3:
                    scope, key, value = fields(args, '""w')
                    choices = scoped.setdefault(name(scope), {})
                else:
                    key, value = fields(args, '"w')
                    choices = selections
                if value not in ('on', 'off'):
                    fail('optimization selection must be on or off')
                unique(choices, optimization.selector(key), value == 'on')
                if len(selections) + sum(map(len, scoped.values())) > 512:
                    fail('optimization selector count budget exceeded')
            elif verb == 'backend' and not dependency:
                if len(args) == 2:
                    scope, value = fields(args, '"w')
                    unique(backend_scopes, name(scope), value)
                else:
                    value, = fields(args, 'w')
                    unique(options, verb, value)
                if value not in BACKENDS:
                    fail(f'unknown project backend {value!r}; choose from {", ".join(BACKENDS)}')
            elif verb == 'target' and not dependency:
                key, value = fields(args, '""')
                unique(targets, name(key), local(value))
            elif verb == 'program' and not dependency:
                key, value = fields(args, '""')
                path = local(value)
                if path.suffix != '.on1x':
                    fail('program requires an .on1x path')
                unique(programs, name(key, MODULE_NAME), path)
                if len(programs) > MAX_MODULES:
                    fail('program count budget exceeded')
            elif verb in ('module', 'module-interface', 'module-interface-policy'):
                if verb == 'module-interface-policy':
                    key, policy = fields(args, '"w')
                    if policy not in ('optional', 'required', 'forbidden'):
                        fail('module interface policy must be optional, required or forbidden')
                    value = policy
                else:
                    key, path = fields(args, '""')
                    value = local(path)
                    extension = '.on1xm' if verb == 'module' else '.on1xi'
                    if value.suffix != extension:
                        fail(f'{verb} requires a {extension} path')
                    if value in module_paths:
                        fail(f'{value}: duplicate module membership (first declared at {module_paths[value]})')
                    module_paths[value] = at
                    if len(module_paths) > MAX_FILES:
                        fail('module fragment count budget exceeded')
                key = name(key, MODULE_NAME)
                record = modules.setdefault(key, {})
                if len(modules) > MAX_MODULES:
                    fail('module count budget exceeded')
                if verb == 'module-interface-policy':
                    unique(record, verb, value)
                else:
                    record.setdefault(verb, []).append(value)
            elif verb == 'dependency' and not dependency:
                kind = args[1] if len(args) > 1 else None
                if kind == ('local', False):
                    key, _, path = fields(args, '"w"')
                    unique(deps, name(key, IDENT), Dependency('local', local(path)))
                elif kind in (('git', False), ('git-remote', False)):
                    if len(args) == 4:
                        key, transport, path, revision = fields(args, '"w""')
                        selector_kind = 'commit'
                    elif len(args) == 5:
                        key, transport, path, selector_kind, revision = fields(args, '"w"w"')
                    else:
                        raise ManifestFieldsError('expected a Git source and commit, branch or tag selector')
                    key = name(key, IDENT)
                    revision = git_selector(selector_kind, revision)
                    if transport == 'git-remote':
                        source = remote_url(path)
                    else:
                        source = local(path)
                    unique(deps, key, Dependency(transport, source, selector_kind, revision))
                else:
                    raise ManifestFieldsError('dependency kind must be local, git or git-remote')
            elif verb == 'profile' and not dependency:
                key, path = fields(args, '""')
                if key in ('dev', 'release', *VM_COMMANDS, FFI_GENERATE) or not path.endswith('.lua'):
                    fail('use a custom profile name and a local .lua script')
                unique(profiles, name(key), local(path))
            elif verb == 'profile-arg' and not dependency:
                scope, key, value = profile_argument(args)
                unique(profile_args.setdefault(scope, {}), key, value)
                if sum(map(len, profile_args.values())) > 64:
                    fail('profile-arg declaration count budget exceeded')
            elif verb == 'command' and not dependency:
                key, action, target, path = fields(args, '"ww"')
                if (key in ('build', 'release', *VM_COMMANDS, FFI_GENERATE)
                        or action != 'format' or target not in ('plain', 'latex', 'troff')):
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
            elif verb == 'kernel-path':
                if dependency:
                    root_only_kernel_path()
                path, = fields(args, '"')
                unique(kernel_paths, local(path), True)
                if len(kernel_paths) > MAX_KERNEL_PATHS:
                    fail(f'too many kernel-path directives (maximum {MAX_KERNEL_PATHS})')
            elif verb == 'kernel-link':
                if dependency:
                    root_only_kernel_link()
                mode, = fields(args, 'w')
                if mode != 'load-time':
                    fail('kernel-link expects load-time; omit it for normal linkage')
                unique(options, verb, mode)
            else:
                fail(f'unsupported {verb!r} in {"dependency" if dependency else "root"} On1xfile')
        except ValueError as error:
            row_error(at, start, end, error)
    kind = options.get('kind')
    if not options.get('name') or not kind:
        fail(f'{origin}: name and kind are required')
    if kind == 'library' and programs:
        fail(f'{origin}: only executable and complex projects can admit program imports')
    if 'kernel-link' in options:
        if kind == 'library':
            root_only_kernel_link(origin)
        if 'vm' in options or kernels:
            fail(f'{origin}: kernel-link load-time cannot be combined with vm or static kernel declarations')
    if toolchain.DIRECTIVES & options.keys() and (kind == 'library' or 'vm' in options):
        fail(f'{origin}: native toolchain directives require a hosted executable project')
    try:
        toolchain.validate_manifest(options)
    except ValueError as error:
        fail(f'{origin}: {error}')
    if 'vm' in options:
        if kind == 'library':
            root_only_vm(origin)
        if 'backend' in options or backend_scopes or kernels:
            fail(f'{origin}: vm policy cannot be combined with hosted backends or kernel declarations')
    if kind == 'executable' and len(targets) != 1 or kind == 'complex' and not targets or kind == 'library' and targets:
        fail(f'{origin}: executable needs one target, complex needs at least one, and library allows none')
    if len(targets) > 64 or len(deps) > 32 or len(profiles) > 32 or len(defines) > 64 or len(kernels) > 32:
        fail(f'{origin}: declaration count budget exceeded')
    if commands.keys() & profiles.keys():
        fail(f'{origin}: a command cannot have the same name as a profile')
    unknown = profile_args.keys() - profiles.keys()
    if unknown:
        fail(f'{origin}: profile-arg requires a declared Lua profile, got {min(unknown)!r}')
    unknown = scoped.keys() - (profiles.keys() | {'dev', *optimization.PROFILES})
    if unknown:
        fail(f'{origin}: unknown optimization scope {min(unknown)!r}')
    unknown = backend_scopes.keys() - (profiles.keys() | {'dev', *optimization.PROFILES})
    if unknown:
        fail(f'{origin}: unknown backend scope {min(unknown)!r}')
    if len(commands) > 32:
        fail(f'{origin}: command declaration count budget exceeded')
    # Do not let a parent root pull checkout data into the independent FFI
    # generation process, or let a path point inside a dependency store.
    if any(paths_overlap(root, dependency.source) for root in kernel_paths
           for dependency in deps.values() if dependency.kind != 'git-remote'):
        fail(f'{origin}: kernel-path cannot overlap a declared dependency store')
    directory = options.get('directory', Path('.'))
    for path, at in module_paths.items():
        if not path.is_relative_to(directory):
            fail(f'{at}: module fragment {path} is outside the declared directory {directory}')
        if any(part.startswith('.') or part in ('build', 'target')
               for part in path.parent.relative_to(directory).parts):
            fail(f'{at}: module fragment {path} is inside a pruned directory')
        if any(path.is_relative_to(dep.source) for dep in deps.values() if dep.kind != 'git-remote'):
            fail(f'{at}: module fragment {path} is inside a declared dependency store')
    for key, record in modules.items():
        if key.split('.', 1)[0] == 'std' and (kind != 'library' or options['name'] != 'std' or dependency):
            fail(f'{origin}: module {key!r} cannot claim the standard-library namespace')
        sources = tuple(record.get('module', ()))
        interfaces = tuple(record.get('module-interface', ()))
        policy = record.get('module-interface-policy', 'optional')
        if not sources:
            fail(f'{origin}: module {key!r} requires at least one implementation fragment')
        if policy == 'required' and not interfaces:
            fail(f'{origin}: module {key!r} requires at least one interface fragment')
        if policy == 'forbidden' and interfaces:
            fail(f'{origin}: module {key!r} forbids interface fragments')
        modules[key] = ModuleMembership(sources, interfaces, policy)
    options.update(targets=targets, deps=deps, defines=defines,
                   profiles=profiles, profile_args=profile_args, kernels=list(kernels),
                   kernel_paths=list(kernel_paths), commands=commands,
                   selections=selections, scoped=scoped, backend_scopes=backend_scopes,
                   modules=modules, programs=programs)
    return options


def dependency_source(dependency):
    return (dependency.source.as_posix()
            if isinstance(dependency.source, Path) else dependency.source)


def lock_entry(dependency, commit):
    commit = git_object_id(commit, 'lockfile commit must be a full lowercase Git object ID')
    if dependency.selector_kind == 'commit' and dependency.selector != commit:
        fail('resolved Git commit does not match the pinned On1xfile commit')
    return {
        'kind': dependency.kind,
        'source': dependency_source(dependency),
        'selector': {'kind': dependency.selector_kind, 'value': dependency.selector},
        'commit': commit,
    }


def declared_policy(config):
    policy = {k: config[k] for k in ('optimization-profile', 'backend') if k in config}
    if config['selections']:
        policy['selections'] = config['selections']
    if config['scoped']:
        policy['scoped'] = config['scoped']
    if config['backend_scopes']:
        policy['backend_scopes'] = config['backend_scopes']
    return policy


def lock_bytes(entries, policy, vm_policy, kernel_inputs):
    document = {'schema': 'on1x.lock/v1',
                'dependencies': {key: entries[key] for key in sorted(entries)}}
    if policy:
        document.update(schema='on1x.lock/v2', optimization=policy)
    if vm_policy is not None:
        document.update(schema='on1x.lock/v3', vm=vm_policy)
    if kernel_inputs:
        document.update(schema='on1x.lock/v4', kernel_paths=kernel_inputs)
    return (json.dumps(document, indent=2, sort_keys=True, ensure_ascii=True) + '\n').encode()


def unique_json_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            fail(f'duplicate lockfile key {key!r}')
        result[key] = value
    return result


def parse_lock(payload, origin):
    try:
        document = json.loads(payload.decode('utf-8'), object_pairs_hook=unique_json_object)
    except (UnicodeError, ValueError, RecursionError) as error:
        fail(f'{origin}: invalid lockfile JSON: {error}')
    if not isinstance(document, dict) or not isinstance(document.get('dependencies'), dict):
        fail(f'{origin}: expected an On1x lock object')
    schema = document.get('schema')
    keys = set(document) - {'schema', 'dependencies'}
    valid = (schema == 'on1x.lock/v1' and not keys
             or schema == 'on1x.lock/v2' and keys == {'optimization'}
             or schema == 'on1x.lock/v3' and 'vm' in keys and keys <= {'optimization', 'vm'}
             or schema == 'on1x.lock/v4' and 'kernel_paths' in keys
             and keys <= {'optimization', 'vm', 'kernel_paths'})
    if not valid:
        fail(f'{origin}: expected an on1x.lock/v1, on1x.lock/v2, on1x.lock/v3 or on1x.lock/v4 object')
    vm_policy = document.get('vm')
    if 'vm' in keys and vm_policy not in VM_POLICIES:
        fail(f'{origin}: invalid locked vm policy')
    kernel_inputs = document.get('kernel_paths', [])
    if 'kernel_paths' in keys:
        if not isinstance(kernel_inputs, list) or not 1 <= len(kernel_inputs) <= MAX_KERNEL_PATHS:
            fail(f'{origin}: invalid locked kernel paths')
        seen = set()
        for entry in kernel_inputs:
            if (not isinstance(entry, dict) or set(entry) != {'path', 'sha256'}
                    or not isinstance(entry['path'], str) or not isinstance(entry['sha256'], str)
                    or not re.fullmatch(r'[0-9a-f]{64}', entry['sha256'])):
                fail(f'{origin}: invalid locked kernel-path fingerprint')
            path = local(entry['path'])
            if path in seen:
                fail(f'{origin}: duplicate locked kernel-path {path}')
            seen.add(path)
    policy = {}
    if 'optimization' in document:
        policy = document['optimization']
        if (not isinstance(policy, dict) or not policy
                or set(policy) - {'optimization-profile', 'backend', 'backend_scopes', 'selections', 'scoped'}
                or policy.get('optimization-profile', 'default') not in optimization.PROFILES
                or policy.get('backend', 'c11') not in BACKENDS):
            fail(f'{origin}: invalid locked optimization policy')
        if 'backend_scopes' in policy:
            mapping = policy['backend_scopes']
            if not isinstance(mapping, dict) or not 1 <= len(mapping) <= 36:
                fail(f'{origin}: invalid locked backend scopes')
            for scope, backend in mapping.items():
                name(scope)
                if backend not in BACKENDS:
                    fail(f'{origin}: invalid locked backend {backend!r}')
        scopes = policy.get('scoped', {})
        if not isinstance(scopes, dict) or len(scopes) > 36 or 'scoped' in policy and not scopes:
            fail(f'{origin}: invalid locked optimization scopes')
        for scope in scopes:
            name(scope)
        total = 0
        for choices in ([policy['selections']] if 'selections' in policy else []) + list(scopes.values()):
            if not isinstance(choices, dict) or not 1 <= len(choices) <= 512:
                fail(f'{origin}: invalid locked optimization selections')
            total += len(choices)
            for key, value in choices.items():
                optimization.selector(key)
                if not isinstance(value, bool):
                    fail(f'{origin}: invalid locked optimization selection for {key!r}')
        if total > 512:
            fail(f'{origin}: locked selector count budget exceeded')
    dependencies = document['dependencies']
    if len(dependencies) > 32:
        fail(f'{origin}: dependency count budget exceeded')
    result = {}
    for key, entry in dependencies.items():
        try:
            name(key, IDENT)
            if not isinstance(entry, dict) or set(entry) != {'kind', 'source', 'selector', 'commit'}:
                fail('expected exactly kind, source, selector and commit')
            if entry['kind'] not in ('git', 'git-remote') or not isinstance(entry['source'], str):
                fail('invalid Git kind or source')
            selector = entry['selector']
            if (not isinstance(selector, dict) or set(selector) != {'kind', 'value'}
                    or not isinstance(selector['kind'], str) or not isinstance(selector['value'], str)):
                fail('invalid selector')
            git_selector(selector['kind'], selector['value'])
            git_object_id(entry['commit'], 'lockfile commit must be a full lowercase Git object ID')
            if len(entry['source']) > 4096 or '\x00' in entry['source']:
                fail('invalid or overlong source')
            result[key] = entry
        except ValueError as error:
            fail(f'{origin}: dependency {key!r}: {error}')
    return result, policy, vm_policy, kernel_inputs


def read_lock(project, config, *, required=False, strict=True, snapshot=None, kernel_inputs=None):
    path = project / 'On1xfile.lock'
    try:
        mode = path.lstat().st_mode
    except FileNotFoundError:
        if required:
            fail(f'{path}: lockfile is required; run on1x-project.py lock')
        return {}, None
    if not stat.S_ISREG(mode):
        fail(f'{path}: expected a regular lockfile')
    payload = read(path, MAX_LOCKFILE)
    entries, policy, vm_policy, locked_kernels = parse_lock(payload, path)
    if strict and vm_policy != config.get('vm'):
        fail(f'{path}: stale vm policy does not match On1xfile; run on1x-project.py lock')
    if strict and policy != declared_policy(config):
        fail(f'{path}: stale optimization policy does not match On1xfile')
    if strict and ([item['path'] for item in locked_kernels] != [p.as_posix() for p in config['kernel_paths']]
                   or kernel_inputs is not None and locked_kernels != kernel_inputs):
        fail(f'{path}: stale kernel-path declarations or contents; run on1x-project.py lock')
    if snapshot is not None:
        snapshot.record('project/On1xfile.lock', payload)
    git_dependencies = {key: dependency for key, dependency in config['deps'].items()
                        if dependency.kind in ('git', 'git-remote')}
    matching = {}
    for key, entry in entries.items():
        dependency = git_dependencies.get(key)
        expected = None if dependency is None else {
            'kind': dependency.kind,
            'source': dependency_source(dependency),
            'selector': {'kind': dependency.selector_kind, 'value': dependency.selector},
        }
        if dependency is None or any(entry[field] != value for field, value in expected.items()):
            if strict:
                fail(f'{path}: stale entry for {key!r} does not match On1xfile')
            continue
        if dependency.selector_kind == 'commit' and entry['commit'] != dependency.selector:
            if strict:
                fail(f'{path}: pinned commit for {key!r} does not match On1xfile')
            continue
        matching[key] = entry
    return matching, payload


def require_locked(entries, config, keys=None):
    selected = (set(keys) if keys is not None else
                {key for key, dependency in config['deps'].items()
                 if dependency.kind in ('git', 'git-remote')})
    missing = sorted(selected - entries.keys())
    if missing:
        fail('lockfile is missing Git dependencies: ' + ', '.join(missing))


def write_lock(project, entries, config, kernel_inputs):
    path = project / 'On1xfile.lock'
    try:
        mode = path.lstat().st_mode
        if not stat.S_ISREG(mode):
            fail(f'{path}: refusing to replace a non-regular lockfile')
    except FileNotFoundError:
        pass
    payload = lock_bytes(entries, declared_policy(config), config.get('vm'), kernel_inputs)
    descriptor, temporary = tempfile.mkstemp(dir=project, prefix='.On1xfile.lock-')
    temporary = Path(temporary)
    try:
        os.fchmod(descriptor, 0o644)
        with os.fdopen(descriptor, 'wb') as stream:
            descriptor = -1
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        directory = os.open(project, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
    finally:
        if descriptor >= 0:
            os.close(descriptor)
        temporary.unlink(missing_ok=True)
    return path


def cache_relative(key, commit):
    return Path('.on1x-deps') / name(key, IDENT) / git_object_id(commit)


class DependencyCacheLock:
    """Serialize cache publication and lockfile replacement for one project."""
    def __init__(self, project):
        self.root = project / '.on1x-deps'
        self.descriptor = None

    def __enter__(self):
        if fcntl is None:
            fail('dependency cache locking requires POSIX flock')
        if self.root.is_symlink():
            fail(f'{self.root}: dependency cache must not be a symlink')
        self.root.mkdir(mode=0o700, exist_ok=True)
        if not self.root.is_dir() or self.root.is_symlink():
            fail(f'{self.root}: expected a real dependency cache directory')
        path = self.root / '.lock'
        self.descriptor = os.open(path, os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
        try:
            if not stat.S_ISREG(os.fstat(self.descriptor).st_mode):
                fail(f'{path}: expected a regular cache lock')
            fcntl.flock(self.descriptor, fcntl.LOCK_EX)
        except Exception:
            os.close(self.descriptor)
            self.descriptor = None
            raise
        return self

    def __exit__(self, *_):
        if self.descriptor is not None:
            fcntl.flock(self.descriptor, fcntl.LOCK_UN)
            os.close(self.descriptor)
            self.descriptor = None


def dependency_modes(args):
    frozen = bool(getattr(args, 'frozen', False))
    return bool(getattr(args, 'offline', False) or frozen), bool(
        getattr(args, 'locked', False) or frozen)


def program(value):
    path = Path(value)
    if not path.is_absolute() and '/' not in os.fspath(value):
        selected = shutil.which(value, path=ENV['PATH'])
        if selected is None:
            fail(f'executable unavailable on the fixed PATH: {value}')
        path = Path(selected)
    path = path.resolve()
    if not path.is_file() or not os.access(path, os.X_OK):
        fail(f'executable unavailable: {value}')
    return path


def compiler_path(explicit):
    selected = explicit or os.environ.get('ON1X_PROJECT_COMPILER')
    return program(selected or toolchain.default_compiler(ROOT))


def validate_locked_names(args, config, project):
    names = declared_names(config)
    if names:
        flags = optimization.flags(None, dict.fromkeys(names, True))
        run([compiler_path(args.on1xc), '--list-optimizations', *flags], project, bound=MAX_MANIFEST)


def declared_names(config):
    return set(config['selections']).union(*config['scoped'].values()) - {'all'}


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


@contextmanager
def open_directory(root, relative):
    # Anchor descendant opens to directory descriptors so a concurrent link
    # swap cannot redirect a kernel input outside the project's directory.
    flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
    fd = os.open(root, flags)
    try:
        for part in relative.parts:
            child = os.open(part, flags, dir_fd=fd)
            os.close(fd)
            fd = child
        yield fd
    finally:
        os.close(fd)


def regular_tree(directory, origin, *, limit):
    """Yield sorted relative directory/file records without following links."""
    entries = 0

    def walk(fd, relative):
        nonlocal entries
        if len(relative.parts) > 32:
            fail(f'{origin}: tree discovery depth exceeded')
        children = []
        with os.scandir(fd) as stream:
            for entry in stream:
                entries += 1
                if entries > 32768:
                    fail(f'{origin}: tree discovery budget exceeded')
                children.append(entry.name)
        for child in sorted(children):
            path = relative / child
            # Names may contain + (as MuLIB headers do), etc. Reject control
            # characters and paths not representable in a UTF-8 fingerprint.
            child.encode('utf-8')
            if any(ord(char) < 32 or ord(char) == 127 for char in child):
                fail(f'{origin}: invalid tree entry name: {child!r}')
            flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
            mode = os.stat(child, dir_fd=fd, follow_symlinks=False).st_mode
            if stat.S_ISDIR(mode):
                subdir = os.open(child, flags, dir_fd=fd)
                try:
                    yield path, None
                    yield from walk(subdir, path)
                finally:
                    os.close(subdir)
            elif stat.S_ISREG(mode):
                yield path, read(child, limit, dir_fd=fd)
            else:
                fail(f'{origin / path}: expected a regular file or directory; symlinks are not allowed')

    yield from walk(directory, Path('.'))


def kernel_path_inputs(project, config, snapshot=None, *, stage=None):
    if snapshot is None:
        snapshot = Snapshot()
    roots = []
    for number, root in enumerate(config['kernel_paths']):
        origin = project / root
        contents = {}
        try:
            with open_directory(project, root) as directory:
                dest = stage / str(number) if stage is not None else None
                if dest is not None:
                    dest.mkdir(parents=True)
                # Stage the whole declared tree (not just .h): Cephyr can
                # read transitive includes with other extensions. Discovery
                # and any duplicate-kernel policy are the generator's job.
                for path, data in regular_tree(directory, origin, limit=MAX_SOURCE):
                    key = path.as_posix()
                    if data is None:
                        contents[key + '/'] = None
                        if dest is not None:
                            (dest / path).mkdir()
                    else:
                        label = f'kernel-path/{number}/{key}'
                        snapshot.record(label, data)
                        contents[key] = snapshot.inputs[label]
                        if dest is not None:
                            stage_bytes(dest / path, data)
        except (OSError, UnicodeError) as error:
            fail(f'{origin}: kernel-path requires a real directory tree without symlinks: {error}')
        roots.append({'path': root.as_posix(),
                      'sha256': digest(json.dumps(contents, sort_keys=True).encode('ascii'))})
    return roots


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
            for filename in ('On1xfile', 'On1xfile.m', 'On1xfile.i'):
                if (filename != 'On1xfile' or rel != Path('.')) and (
                        filename in files or (parent / filename).is_symlink()):
                    yield rel / filename


def git_directory(repository):
    if repository.is_symlink() or not repository.is_dir():
        fail(f'{repository}: offline Git object store is missing')
    dotgit = repository / '.git'
    if dotgit.is_symlink() or dotgit.is_file():
        fail(f'{repository}: external Git worktrees are not supported')
    directory = dotgit if dotgit.is_dir() else repository
    objects = directory / 'objects'
    if objects.is_symlink() or not objects.is_dir():
        fail(f'{repository}: expected a self-contained Git objects directory')
    return directory, objects


def local_ref_object(repository, kind, value):
    directory, _ = git_directory(repository)
    reference = git_ref(kind, value)
    loose = contained(directory, Path(reference))
    try:
        payload = read(loose, 256)
    except FileNotFoundError:
        payload = None
    if payload is not None:
        try:
            object_id = payload.decode('ascii').strip()
        except UnicodeError:
            fail(f'{repository}: malformed Git {kind} {value!r}')
        return git_object_id(object_id, f'{repository}: malformed Git {kind} {value!r}')
    packed = directory / 'packed-refs'
    try:
        payload = read(packed, MAX_GIT_REFS)
    except FileNotFoundError:
        fail(f'{repository}: Git {kind} {value!r} was not found')
    found = None
    # Git ref names are bytes; only the On1x selector must be ASCII.
    wanted = reference.encode('ascii')
    lines = payload.splitlines()
    if len(lines) > 65536:
        fail(f'{packed}: ref count budget exceeded')
    for line in lines:
        if not line or line.startswith((b'#', b'^')):
            continue
        fields = line.split(b' ')
        if len(fields) != 2:
            fail(f'{packed}: malformed packed ref')
        encoded_id, candidate = fields
        try:
            object_id = encoded_id.decode('ascii')
        except UnicodeError:
            fail(f'{packed}: malformed packed ref object ID')
        git_object_id(object_id, f'{packed}: malformed packed ref object ID')
        if candidate == wanted:
            if found is not None:
                fail(f'{packed}: duplicate packed ref {reference}')
            found = object_id
    if found is None:
        fail(f'{repository}: Git {kind} {value!r} was not found')
    return found


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

    def __init__(self, repository, revision, cwd, gitdir, *, deadline=None, peel=False):
        self.cwd = cwd
        self.deadline = deadline
        git_object_id(revision)
        _, source = git_directory(repository)
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
        if object_kind == b'commit\n':
            commit = revision
        elif peel and object_kind == b'tag\n':
            try:
                commit = self.call('rev-parse', '--verify',
                                   revision + '^{commit}', bound=128).decode().strip()
            except ValueError:
                fail('Git tag must resolve to a commit')
            git_object_id(commit, 'Git tag did not resolve to a full commit object ID')
        else:
            fail('Git pin must name a commit, not a tag, tree or blob')
        self.call('fsck', '--strict', '--no-reflogs', '--no-dangling', revision, bound=4096)
        self.revision = commit
        self.entries = {}
        listing = self.call('ls-tree', '-rz', '--full-tree', commit, bound=4 * 1024 * 1024)
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
            if path != Path('On1xfile') and path.name in ('On1xfile', 'On1xfile.m', 'On1xfile.i') and path.is_relative_to(directory):
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


def dotenv_environment(project, config, args):
    if 'dotenv' in config and (args.sandbox or dependency_modes(args)[1]):
        fail('dotenv is unavailable with --sandbox, --locked or --frozen')
    if args.sandbox:
        return ENV
    # Opt in only names declared by the file; do not import ambient compiler,
    # loader or search-path policy into the clean project environment.
    values = {}
    if 'dotenv' in config:
        path, mode = config['dotenv']
        values = dotenv.load(project, path, {})
        if mode == 'preserve':
            values.update((key, os.environ[key]) for key in values if key in os.environ)
    for key in values:
        if (key in ENV or key in {'BASH_ENV', 'ENV', 'SHELLOPTS', 'CDPATH', 'IFS', 'TMPDIR'}
                or key.startswith(('LD_', 'DYLD_', 'GIT_', 'PYTHON', 'ON1X_', 'MULIB_'))):
            fail(f'dotenv cannot override reserved environment key {key}')
    proxy_keys = {'http_proxy', 'https_proxy', 'any_proxy', 'all_proxy', 'no_proxy'}
    ambient = {key: value for key, value in os.environ.items() if key.lower() in proxy_keys}
    for key in tuple(values):
        if key.lower() in proxy_keys:
            ambient = {name: value for name, value in ambient.items() if name.lower() != key.lower()}
            ambient[key] = values.pop(key)
    return {**ENV, **values, **proxy.compiler_environment(config, ambient)}


class Execution:
    """Compiler process envelope; host file acquisition itself is intentionally outside it."""
    def __init__(self, args, work, compiler, cc, mulib, *, env=ENV, load_time=False):
        self.work = work
        self.env = env
        self.python = Path(sys.executable)
        self.bwrap = None
        self.prefix = []
        if args.sandbox:
            if not sys.platform.startswith('linux'):
                fail('--sandbox requires Linux bubblewrap')
            self.bwrap = program('bwrap')
            self.prefix = [self.bwrap, '--unshare-all', '--die-with-parent', '--new-session',
                           '--clearenv', '--ro-bind', '/usr', '/usr', '--proc', '/proc', '--dev', '/dev',
                           '--tmpfs', '/tmp', '--dir', '/etc',
                           '--ro-bind', work / 'input', '/work/input',
                           '--bind', work / 'cwd', '/work/cwd',
                           '--bind', work / 'artifact', '/work/artifact', '--chdir', '/work/cwd']
            for alias in ('/bin', '/sbin', '/lib', '/lib64'):
                path = Path(alias)
                if path.is_symlink():
                    self.prefix.extend(('--symlink', os.readlink(path), alias))
                elif path.is_dir():
                    self.prefix.extend(('--ro-bind', alias, alias))
            for key, value in ENV.items():
                self.prefix.extend(('--setenv', key, value))
            self.mounts = set()
            if compiler is not None:
                for path in (ROOT / 'tools', ROOT / 'runtime', ROOT / 'stdlib/mulib', compiler, cc):
                    self.mount(path)
                if not load_time:
                    for path in (mulib / 'kernels', mulib / 'manual',
                                 mulib / 'library', mulib / 'MANIFEST.yaml'):
                        self.mount(path)
                    # Mount executable recipes only: the build's sibling
                    # catalogue is metadata and can be much larger.
                    # No ambient MULIB_FOREIGNLIB_PATH in isolated builds.
                    for recipes in (compiler.parent / 'stdffi/mulib/recipes',
                                    compiler.parent.parent / 'share/on1x/stdffi/mulib'):
                        self.mount(recipes, exact=True)
            # A failed setup must not even start a compiler discovery process.
            self.run(['/usr/bin/true'], bound=32)
            # Use the same name for the mount and argv, including when the
            # driver itself was started via a Python symlink or venv.
            self.python = program(sys.executable)

    def standard_library(self, compiler, explicit):
        def query(override):
            options = ['--stdlib', override] if override is not None else []
            result = Path(self.run([compiler, '--print-stdlib', *options], bound=4096).decode().strip())
            if not result.is_absolute() or '..' in result.parts or not result.is_dir():
                fail(f'compiler did not report a normalized absolute standard library directory: {result}')
            return result

        override = explicit.resolve(strict=True) if explicit is not None else None
        if self.prefix:
            # Make the installed layout visible for on1xc's relative-to-binary
            # lookup. Its compiled-in fallback can be reported before it is
            # mounted, even when using a compiler from a different checkout.
            candidate = override if override is not None else compiler.parent.parent / 'share/on1x/stdlib'
            previous = len(self.prefix)
            mounts = self.mounts.copy()
            self.mount(candidate, exact=True)
        standard = query(override)
        if self.prefix:
            if override is None and not candidate.is_dir():
                # Resolve a manifest-relative root now that the compiler can
                # actually inspect its reported fallback directory.
                self.mount(standard, exact=True)
                standard = query(None)
            del self.prefix[previous:]
            self.mounts = mounts
        standard = standard.resolve(strict=True)
        self.mount(standard)
        self.mount(standard.parent / 'std', exact=True)
        return standard

    def mount(self, path, *, exact=False):
        if not self.prefix or path is None or not path.exists():
            return
        source = path.resolve()
        # The ArchVirt toolchain records the paths it actually opens. For
        # example, the ELF loader may request a shared library's SONAME link,
        # not the canonical filename.
        target = path if exact else source
        if ((source.is_relative_to('/usr') and target.is_relative_to('/usr'))
                 or target in self.mounts):
            return
        private = ('/work/input', '/work/cwd', '/work/artifact', '/dev', '/proc', '/etc')
        if (source == Path('/') or source.is_relative_to(self.work)
                or target.is_relative_to(self.work) or target in (Path('/work'), Path('/tmp'))
                or any(target.is_relative_to(path) for path in private)):
            fail(f'invalid sandbox toolchain mount: {path}')
        self.mounts.add(target)
        self.prefix.extend(('--ro-bind', source, target))

    def path(self, path):
        return Path('/work') / path.relative_to(self.work) if self.prefix else path

    def run(self, command, *, extra_env=None, **kwargs):
        # Only the explicit FFI generator may receive its staged kernel roots.
        # Neither bwrap nor ordinary compiler/build subprocesses inherit them.
        prefix = self.prefix
        if prefix and extra_env:
            prefix = [*prefix, *(arg for key, value in extra_env.items() for arg in ('--setenv', key, value))]
        env = dict(self.env, **extra_env) if extra_env and not prefix else self.env
        return run([*prefix, *(['--'] if prefix else []), *command], self.work / 'cwd', env=env, **kwargs)


def stage_bytes(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('xb') as stream:
        stream.write(data)


# Registered native companions may be found via --stdlib-resource. Packaging
# explicit-link companions does not enable automatic linkage or authorize I/O.
# Preserve the existing standalone MuLIB discovery manifest as well. Other SDK
# inputs are not part of the project-controlled package.
STDLIB_RESOURCES = frozenset(map(Path, (
    'runtime/basis_native.c', 'runtime/basis_native.h', 'runtime/basis_real.c',
    'environ/runtime.c',
    'cephyr/runtime.c',
    'daemon/supervisor.c', 'daemon/supervisor.h', 'daemon/lifecycle_kernels.c',
    'elvi/runtime.c', 'elvi/runtime.h', 'elvi/bridge.c', 'elvi/bridge.h',
    'elvi/capture.c', 'elvi/capture.h', 'elvi/kernels.c',
    'elvi/script.c', 'elvi/script.h', 'elvi/live.c', 'elvi/live.h',
    'html/runtime.c', 'html/runtime.h', 'html/kernels.c',
    'http/runtime.c', 'http/runtime.h', 'http/kernels.c',
    'http/probe/context.c', 'http/probe/context.h',
    'http/probe/supervised.c', 'http/probe/supervised.h',
    'http/probe/artifact.c', 'http/probe/artifact.h',
    'kron/runtime.c', 'kron/runtime.h', 'kron/kernels.c',
    'prettyprint/runtime.c', 'prettyprint/runtime.h', 'tty/runtime.c', 'tty/runtime.h',
    'container/runtime.c', 'container/runtime.h',
    'container/shared.c', 'container/on1x_container_shared.h',
    'container/pod_kernel.h', 'container/pod_runtime.c', 'container/pod_runtime.h',
    'container/pod_worker.c', 'mulib/On1xfile',
)))


def local_roles(data, origin, manifest, kind):
    allowed = ('interface',) if manifest.name == 'On1xfile.i' else ('source', 'library', 'resource')
    declared = {}
    for start, end, role, args in rows(data, origin):
        at = manifest_span(origin, start, end)
        try:
            if role == 'vm':
                root_only_vm()
            if role == 'kernel-path':
                root_only_kernel_path()
            if role == 'kernel-link':
                root_only_kernel_link()
            if role in toolchain.DIRECTIVES:
                fail('native toolchain directives are only allowed in the root project On1xfile')
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
            row_error(at, start, end, error)
    return declared


def layouts(provider, config, label, staged, snapshot, preprocess=None, dependency=False, membership=None):
    directory = config.get('directory', Path('.'))
    roles = {}
    interface_origins = {}
    for path in provider.manifests(directory):
        local(path.as_posix())
        origin = f'{label}/{path.as_posix()}'
        if path.name == 'On1xfile':
            # Nested package manifests are inert here, including templates:
            # the driver never evaluates one as a second project root.
            check_nested_authority(provider.get(path, MAX_MANIFEST), origin)
            continue
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
    for key, module in sorted(config['modules'].items()):
        def stage_fragments(paths, role):
            fragments = []
            for source in paths:
                relative = source.relative_to(directory) if dependency else source
                input_label = f'{label}/{source.as_posix()}'
                if relative in roles:
                    fail(f'{input_label}: module {key!r} fragment also has directory-local role {roles[relative]!r}')
                try:
                    data = snapshot.record(input_label, provider.get(source, MAX_SOURCE))
                    stage_bytes(staged / relative, data)
                except (OSError, ValueError) as error:
                    fail(f'{input_label}: {error}')
                roles[relative] = role
                fragments.append(relative)
            return tuple(fragments)

        sources = stage_fragments(module.sources, 'module')
        interfaces = stage_fragments(module.interfaces, 'module-interface')
        if membership is not None:
            membership[key] = ModuleMembership(sources, interfaces, module.interface_policy)
    if not roles:
        fail(f'{label}: no declared directory-local inputs')
    for interface, at in interface_origins.items():
        implementation = interface.with_suffix('.on1x')
        if roles.get(implementation) not in ('source', 'library'):
            fail(f'{at}: interface {interface.name!r} requires a declared same-directory '
                 f'implementation {implementation.name!r}')
    if config['kind'] == 'library' and not {'library', 'module'}.intersection(roles.values()):
        fail(f'{label}: a library requires at least one implementation module')
    for target, source in config['targets'].items():
        if roles.get(source) != 'source':
            fail(f'{label}: target {target!r} must name a locally declared source, not a library or interface')
    for program, source in config['programs'].items():
        if roles.get(source) != 'library':
            fail(f'{label}: program {program!r} must name a locally declared .on1x library input')
    if set(config['targets'].values()) != {path for path, role in roles.items() if role == 'source'}:
        fail(f'{label}: every source role must be named by a root target')
    return roles


def dependency_layout(provider, key, stage, snapshot, membership=None):
    label = f'dependency/{key}'
    manifest = label + '/On1xfile'
    payload = snapshot.record(manifest, provider.get(Path('On1xfile'), MAX_MANIFEST))
    metadata = root_manifest(payload, manifest, dependency=True)
    if metadata['kind'] != 'library' or metadata['name'] != key or key == 'std':
        fail(f'{label}: expected a non-std library certified under the declared dependency name')
    return layouts(provider, metadata, label, stage, snapshot, dependency=True, membership=membership)


def git_diagnostic(error):
    # Escape controls and non-ASCII terminal formatting sent in server
    # diagnostics. URLs admitted by remote_url cannot contain credentials.
    return ''.join(c if c == '\n' or 0x20 <= ord(c) <= 0x7e else ascii(c)[1:-1]
                   for c in str(error))


def download_git(url, revision, ca_file, work, *, reference=None, selector=None, proxy_env=None):
    repository = work / 'download.git'
    cwd = work / 'cwd'
    home = work / 'home'
    home.mkdir()
    env = dict(GIT_ENV, HOME=str(home), GIT_ALLOW_PROTOCOL='https', NETRC='/dev/null',
               GIT_ASKPASS='/bin/false', SSH_ASKPASS='/bin/false', GIT_CEILING_DIRECTORIES=str(work))
    env.update(proxy_env or {})
    options = ['protocol.allow=never', 'protocol.https.allow=always', 'protocol.version=2',
               'http.sslVerify=true', 'http.followRedirects=false',
               'credential.helper=', 'fetch.fsckObjects=true', 'fetch.unpackLimit=1']
    if not proxy_env:
        options.append('http.proxy=')
    if ca_file is not None:
        trusted_ca = work / 'ca.pem'
        stage_bytes(trusted_ca, read(ca_file.resolve(strict=True), 1024 * 1024))
        options.append('http.sslCAInfo=' + str(trusted_ca))
    configured = [word for option in options for word in ('-c', option)]
    if reference is not None:
        remote = [program('git'), '--no-replace-objects', '-c', 'core.hooksPath=/dev/null',
                  '-c', 'pack.threads=1']
        try:
            advertised = run([*remote, *configured, 'ls-remote', '--exit-code', '--refs',
                              '--', url, reference], cwd, env=env, bound=4096,
                             timeout=GIT_FETCH_TIMEOUT, memory=MAX_GIT_MEMORY)
        except ValueError as error:
            if 'failed (2)' in str(error):
                fail(f'remote Git {selector} was not found')
            if proxy_env:
                fail('HTTPS Git ref discovery failed through the configured proxy')
            fail(f'HTTPS Git ref discovery failed: '
                 f'{git_diagnostic(error).replace(url, "<git-remote>")}')
        records = advertised.rstrip(b'\n').split(b'\n')
        if len(records) != 1:
            fail(f'remote Git {selector} did not resolve to exactly one object')
        try:
            object_id, found = records[0].decode('ascii').split('\t')
        except (UnicodeError, ValueError):
            fail(f'remote Git {selector} returned a malformed advertisement')
        if found != reference:
            fail(f'remote Git {selector} returned an unexpected ref')
        revision = git_object_id(object_id, f'remote Git {selector} returned an invalid object ID')
    else:
        revision = git_object_id(revision)
    git = Git.command(repository)
    run([*git, *Git.init(revision)], cwd, env=GIT_ENV, bound=4096, timeout=20, memory=MAX_GIT_MEMORY)
    request = revision if reference is None else f'{reference}:refs/on1x/resolved'
    try:
        # Git's infinite-depth sentinel requests full history from smart
        # servers while making Git's dumb HTTP fallback refuse the fetch.
        # Keeping the pack applies RLIMIT_FSIZE to the whole pack rather than
        # allowing a stream of loose objects.
        run([*git, *configured,
             'fetch', '--quiet', '--no-tags', '--no-recurse-submodules', '--no-auto-maintenance',
              '--no-write-fetch-head', '--no-write-commit-graph', '--no-update-shallow', '--no-filter',
              '--depth=2147483647', '--keep', '--', url, request], cwd, env=env,
             bound=4096, timeout=GIT_FETCH_TIMEOUT, memory=MAX_GIT_MEMORY)
    except ValueError as error:
        if proxy_env:
            fail('HTTPS Git fetch failed through the configured proxy')
        fail(f'HTTPS Git fetch of {selector or revision} failed: '
             f'{git_diagnostic(error).replace(url, "<git-remote>")}')
    if (repository / 'shallow').exists():
        fail('remote Git dependency requires complete history, not a shallow object store')
    if reference is not None:
        fetched = run([*git, 'rev-parse', '--verify', 'refs/on1x/resolved'], cwd, env=GIT_ENV,
                      bound=128, timeout=20, memory=MAX_GIT_MEMORY).decode().strip()
        if fetched != revision:
            fail(f'remote Git {selector} changed while it was being resolved; retry')
    return repository, revision


def inspect_git_dependency(repository, requested, key, work, *, peel=False):
    work.mkdir()
    cwd = work / 'cwd'
    cwd.mkdir()
    checked = work / 'checked.git'
    provider = Git(repository, requested, cwd, checked,
                   deadline=time.monotonic() + GIT_VALIDATION_TIMEOUT, peel=peel)
    dependency_layout(provider, key, work / 'declared', Snapshot())
    return provider.revision, checked


def atomic_publish_store(checked, destination):
    rename = getattr(ctypes.CDLL(None, use_errno=True), 'renameat2', None)
    if rename is None:
        fail('atomic dependency publication requires Linux renameat2')
    stage = checked.parent / 'store'
    stage.mkdir(mode=0o700)
    (checked / 'objects').rename(stage / 'objects')
    # Publish only the offline validator's object projection: no refs,
    # remotes/config, hooks or checked-out source files.
    if rename(-100, os.fsencode(stage), -100, os.fsencode(destination), 1):  # RENAME_NOREPLACE
        code = ctypes.get_errno() or errno.EIO
        if code == errno.EEXIST:
            return False
        raise OSError(code, os.strerror(code), destination)
    return True


def materialize_remote(project, key, dependency, commit, *, refresh, offline, ca_file, config):
    if dependency.kind != 'git-remote':
        fail('internal error: expected a remote Git dependency')
    if not refresh:
        commit = git_object_id(commit)
        destination = contained(project, cache_relative(key, commit))
        if destination.is_symlink():
            fail(f'{destination}: dependency cache entry must not be a symlink')
        if destination.exists():
            with tempfile.TemporaryDirectory(prefix='on1x-git-check-') as temporary:
                resolved, _ = inspect_git_dependency(destination, commit, key,
                                                     Path(temporary) / 'existing')
            return resolved, destination
        if offline:
            fail(f'{destination}: remote Git objects are missing and network access is disabled')
    elif offline:
        fail(f'{key!r}: cannot refresh a Git selector while offline')

    cache_parent = contained(project, Path('.on1x-deps') / key)
    cache_parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=cache_parent, prefix='.fetch-') as temporary:
        work = Path(temporary)
        (work / 'cwd').mkdir()
        proxy_env = proxy.git_environment(config)
        if refresh:
            reference = git_ref(dependency.selector_kind, dependency.selector)
            repository, requested = download_git(
                dependency.source, None, ca_file, work, reference=reference,
                selector=f'{dependency.selector_kind} {dependency.selector!r}', proxy_env=proxy_env)
            peel = dependency.selector_kind == 'tag'
        else:
            repository, requested = download_git(dependency.source, commit, ca_file, work, proxy_env=proxy_env)
            peel = False
        resolved, checked = inspect_git_dependency(repository, requested, key,
                                                   work / 'candidate', peel=peel)
        destination = contained(project, cache_relative(key, resolved))
        if destination.is_symlink():
            fail(f'{destination}: dependency cache entry must not be a symlink')
        if destination.exists():
            winner, _ = inspect_git_dependency(destination, resolved, key, work / 'winner')
            return winner, destination
        if not atomic_publish_store(checked, destination):
            winner, _ = inspect_git_dependency(destination, resolved, key, work / 'winner')
            return winner, destination
        return resolved, destination


def resolve_local_git(project, key, dependency, commit=None):
    repository = contained(project, dependency.source)
    if commit is not None:
        requested, peel = git_object_id(commit), False
    elif dependency.selector_kind == 'commit':
        requested, peel = dependency.selector, False
    else:
        requested = local_ref_object(repository, dependency.selector_kind, dependency.selector)
        peel = dependency.selector_kind == 'tag'
    with tempfile.TemporaryDirectory(prefix='on1x-git-local-') as temporary:
        resolved, _ = inspect_git_dependency(repository, requested, key,
                                             Path(temporary) / 'candidate', peel=peel)
    return resolved, repository


def fetch_dependency(args):
    project = args.root.resolve(strict=True)
    if not project.is_dir():
        fail(f'{project}: expected project directory')
    manifest = project / 'On1xfile'
    manifest_payload = read(manifest, MAX_MANIFEST)
    config = root_manifest(manifest_payload, manifest)
    validate_locked_names(args, config, project)
    key = name(args.name, IDENT)
    dependency = config['deps'].get(key)
    if dependency is None or dependency.kind != 'git-remote':
        fail(f'{key!r} is not a declared git-remote dependency')
    if key == 'std' or config['kind'] == 'library':
        fail('cannot fetch a dependency named std or a dependency of a standalone library')
    offline, locked = dependency_modes(args)
    with DependencyCacheLock(project):
        if read(manifest, MAX_MANIFEST) != manifest_payload:
            fail(f'{manifest}: changed while waiting for the dependency cache lock')
        kernel_inputs = kernel_path_inputs(project, config)
        entries, _ = read_lock(project, config, required=locked, strict=locked, kernel_inputs=kernel_inputs)
        if locked:
            require_locked(entries, config, (key,))
        existing = entries.get(key)
        if dependency.selector_kind == 'commit':
            commit, refresh = dependency.selector, False
        elif offline or locked:
            if existing is None:
                fail(f'{key!r}: selector requires a matching lockfile entry')
            commit, refresh = existing['commit'], False
        else:
            commit, refresh = None, True
        resolved, destination = materialize_remote(
            project, key, dependency, commit, refresh=refresh, offline=offline,
            ca_file=args.ca_file, config=config)
        updated = dict(entries)
        updated[key] = lock_entry(dependency, resolved)
        if not locked:
            write_lock(project, updated, config, kernel_inputs)
    print(destination)


def lock_dependencies(args):
    project = args.root.resolve(strict=True)
    if not project.is_dir():
        fail(f'{project}: expected project directory')
    manifest = project / 'On1xfile'
    manifest_payload = read(manifest, MAX_MANIFEST)
    config = root_manifest(manifest_payload, manifest)
    validate_locked_names(args, config, project)
    if config['kind'] == 'library' and config['deps']:
        fail('standalone source libraries cannot declare dependencies')
    offline, locked = dependency_modes(args)
    with DependencyCacheLock(project):
        if read(manifest, MAX_MANIFEST) != manifest_payload:
            fail(f'{manifest}: changed while waiting for the dependency cache lock')
        kernel_inputs = kernel_path_inputs(project, config)
        entries, _ = read_lock(project, config, required=locked, strict=locked, kernel_inputs=kernel_inputs)
        if locked:
            require_locked(entries, config)
        resolved_entries = {}
        for key, dependency in sorted(config['deps'].items()):
            if dependency.kind == 'local':
                continue
            existing = entries.get(key)
            if dependency.kind == 'git':
                commit = (existing['commit'] if locked and existing is not None else
                          dependency.selector if dependency.selector_kind == 'commit' else None)
                resolved, _ = resolve_local_git(project, key, dependency, commit)
            else:
                if dependency.selector_kind == 'commit':
                    commit, refresh = dependency.selector, False
                elif offline or locked:
                    if existing is None:
                        fail(f'{key!r}: selector requires a matching lockfile entry')
                    commit, refresh = existing['commit'], False
                else:
                    commit, refresh = None, True
                resolved, _ = materialize_remote(
                    project, key, dependency, commit, refresh=refresh, offline=offline,
                    ca_file=args.ca_file, config=config)
            resolved_entries[key] = lock_entry(dependency, resolved)
        if not locked:
            path = write_lock(project, resolved_entries, config, kernel_inputs)
        else:
            path = project / 'On1xfile.lock'
    print(path)


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


def source_policy(executor, compiler, file, root, libraries, *, declared_module=None, programs=()):
    listed = executor.run([compiler, '--tokens', executor.path(file)], bound=16 * MAX_SOURCE)
    # Use the canonical scanner. Only the exact directive token pair matters:
    # comments, string data and hygienic syntax are not regex-scanned as source.
    tokens = []
    for line in listed.split(b'\n'):
        if re.match(rb'^[0-9]+:[0-9]+  ', line):
            tokens.append(line.split(b'  ', 1)[1])
    if declared_module is not None:
        header = [b'module']
        for component in declared_module.split('.'):
            if len(header) > 1:
                header.append(b'.')
            header.append(component.encode('ascii'))
        header.append(b';')
        if tokens[:len(header)] != header:
            fail(f'{file}: source module header does not match declared membership {declared_module!r}')
    for i in range(len(tokens) - 1):
        if tokens[i] == b'@' and tokens[i + 1] in (b'ppx', b'enable', b'extension'):
            fail(f'{file}: source PPX and executable extensions are not available in project builds')
        if tokens[i] == b'import':
            target = tokens[i + 1]
            following = tokens[i + 2] if i + 2 < len(tokens) else b''
            if target == b'program' and following != b'as' and (
                    following.startswith(b'"') or re.fullmatch(rb'[A-Za-z_][A-Za-z0-9_]*', following)):
                if following.startswith(b'"'):
                    fail(f'{file}: project program imports require an admitted program name')
                parts, j = [following], i + 3
                while j + 1 < len(tokens) and tokens[j] == b'.':
                    parts.append(tokens[j + 1])
                    j += 2
                program = b'.'.join(parts).decode('utf-8')
                if program not in programs:
                    fail(f'{file}: program is not in the project index: {program}')
            elif target.startswith(b'"'):
                quoted_import(file, target[1:-1].decode('utf-8'), root, libraries)
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


def module_graph(members, source_edges):
    """Collapse physical fragment edges to package-local logical members.

    `members` maps canonical source paths to (package, name). Edges to
    legacy sources are checked by verify_graph, not promoted to modules.
    """
    dependencies = {member: set() for member in members.values()}
    for count, (importer, imported) in enumerate(source_edges, 1):
        if count > MAX_MODULE_EDGES:
            fail('project module graph edge budget exceeded')
        parent, child = members.get(importer), members.get(imported)
        if parent is not None and child is not None:
            dependencies[parent].add(child)

    # Iterative DFS keeps the accepted depth independent of Python's stack
    # limit. Sorting roots and edges also fixes the first reported cycle.
    completed, order = set(), []
    for root in sorted(dependencies):
        if root in completed:
            continue
        active = {root: 0}
        stack = [(root, iter(sorted(dependencies[root])))]
        while stack:
            node, edges = stack[-1]
            child = next(edges, None)
            if child is None:
                stack.pop()
                del active[node]
                completed.add(node)
                order.append(node)
            elif child in active:
                cycle = [key for key, _ in stack[active[child]:]] + [child]
                fail('project module import cycle: ' + ' -> '.join(f'{pkg}:{key}' for pkg, key in cycle))
            elif child not in completed:
                active[child] = len(stack)
                stack.append((child, iter(sorted(dependencies[child]))))
    return [{'package': pkg, 'name': key,
             'dependencies': [{'package': dep_pkg, 'name': dep_key}
                              for dep_pkg, dep_key in sorted(dependencies[pkg, key])]}
            for pkg, key in order]


def module_index(executor, destination, project_root, packages, programs=None):
    """Create one compiler view of the staged files, not of their checkouts.

    Each value is (stage root, modules, visible dotted prefix). An isolated
    dependency gets its own view with the prefix empty.
    """
    records, owners = {}, {}
    for package, (stage, members, prefix) in sorted(packages.items()):
        for key, member in sorted(members.items()):
            visible = prefix + key
            if visible in records:
                fail(f'ambiguous project module name {visible!r}')
            sources = [executor.path(stage / path) for path in member.sources]
            interfaces = [executor.path(stage / path) for path in member.interfaces]
            for path in sources:
                unique(owners, path, (package, key))
            records[visible] = dict(package=str(executor.path(stage)), declared_name=key,
                                    fragments=list(map(str, sources)), interfaces=list(map(str, interfaces)))
    if not records and not programs:
        return [], owners
    if len(records) > MAX_MODULES:
        fail('project module index count budget exceeded')
    program_paths = {key: str(executor.path(project_root / path)) for key, path in (programs or {}).items()}
    payload = json.dumps({'schema': 'on1x.project-modules/v1', 'project': str(executor.path(project_root)),
                          'members': records, 'programs': program_paths}, sort_keys=True).encode()
    if len(payload) > 8 * 1024 * 1024:
        fail('project module index exceeds its byte budget')
    stage_bytes(destination, payload)
    return ['--project-module-index', str(executor.path(destination))], owners


def module_certificate(members):
    lines = []
    for key, member in sorted(members.items()):
        quoted = json.dumps(key)
        lines.extend(f'module {quoted} {json.dumps(path.as_posix())}\n' for path in member.sources)
        lines.extend(f'module-interface {quoted} {json.dumps(path.as_posix())}\n' for path in member.interfaces)
        if member.interface_policy != 'optional':
            lines.append(f'module-interface-policy {quoted} {member.interface_policy}\n')
    return ''.join(lines)


def inspect_module_graph(executor, compiler, entry, flags, permitted, trusted_roots, edges, expected=()):
    data = executor.run([compiler, '--project-module-graph', entry, *flags], bound=MAX_BUILD_REPORT)
    graph = json.loads(data, object_pairs_hook=unique_json_object)
    if (not isinstance(graph, dict) or set(graph) != {'schema', 'sources', 'edges'}
            or graph['schema'] != 'on1x.project-module-graph/v1'
            or not isinstance(graph['sources'], list) or not isinstance(graph['edges'], list)):
        fail('compiler returned invalid project module graph')

    def path_field(value):
        if not isinstance(value, str) or not value or '\0' in value:
            fail('compiler returned an invalid project module graph path')
        path = Path(value)
        if not path.is_absolute() or '..' in path.parts or str(path) != value:
            fail('compiler returned a noncanonical project module graph path')
        return path

    inputs = {path_field(value) for value in graph['sources']}
    if entry not in inputs or len(inputs) != len(graph['sources']) or len(inputs) > MAX_FILES:
        fail('compiler returned invalid project module graph sources')
    missing = set(expected) - inputs
    if missing:
        fail(f'{entry}: module graph did not load the declared fragment {min(missing)}')
    for path in sorted(inputs):
        if path not in permitted and not any(path.is_relative_to(root) for root in trusted_roots):
            fail(f'{entry}: import is outside the declared module graph: {path}')
    for edge in graph['edges']:
        if not isinstance(edge, list) or len(edge) != 2:
            fail('compiler returned an invalid project module graph edge')
        importer, imported = map(path_field, edge)
        if importer not in inputs or imported not in inputs:
            fail('compiler returned a project module graph edge outside its sources')
        edges.add((importer, imported))
        if len(edges) > MAX_MODULE_EDGES:
            fail('project module graph edge budget exceeded')


def check_manifest_members(executor, compiler, stage, members, loader, compiler_flags,
                           permitted, trusted_roots, edges):
    for i, (member, declaration) in enumerate(sorted(members.items())):
        # The loader uses the carrier's canonical staged package to select
        # its scope. A dot-prefixed name cannot be a manifest-declared path.
        carrier = stage / f'.on1x-check-module-{i}.on1x'
        stage_bytes(carrier, f'import {member};\nfn main() -> i64 {{ return 0; }}\n'.encode())
        try:
            entry = executor.path(carrier)
            inspect_module_graph(executor, compiler, entry, loader, permitted | {entry},
                                 trusted_roots, edges,
                                 (executor.path(stage / path) for path in declaration.sources))
            if declaration.interfaces:
                executor.run([compiler, '--check-interfaces', entry, *compiler_flags])
            executor.run([compiler, '--check', entry, *compiler_flags])
        finally:
            carrier.unlink()


def verify_interfaces(executor, compiler, staged, roles, loader, flags, permitted, trusted_roots, graph_checked=()):
    for path in sorted(path for path, role in roles.items() if role == 'interface'):
        entry = executor.path(staged / path.with_suffix('.on1x'))
        if entry not in graph_checked:
            verify_graph(executor, compiler, entry, loader, permitted | {entry}, trusted_roots)
        executor.run([compiler, '--check-interfaces', entry, '--interface',
                      executor.path(staged / path), *flags])


def publish(stage, output, report):
    if output.is_symlink() or output.exists() and not output.is_dir():
        fail(f'{output}: expected a project output directory')
    if output.is_dir():
        marker = output / '.on1x-build.json'
        info = json.loads(read(marker, MAX_BUILD_REPORT))
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


def native_tools(compiler, cc, executor, chosen_tools):
    """Resolve the exact artifacts used by the native helper; mount files, not a host build tree."""
    helper = ROOT / 'tools/on1x-native-build.py'
    spec = importlib.util.spec_from_file_location('_on1x_native_build', helper)
    native = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(native)
    build = compiler.parent
    archon, scalar, runtime = native.layout(build)
    try:
        manifest = native.manifest_path(build, None)
        for path in (archon, scalar, runtime, manifest):
            if not path.is_file():
                fail(f'missing {path.name}')
            executor.mount(path)
        executor.mount(executor.python)
        command = [executor.python, helper, '--on1xc', compiler, '--target', native.TARGET,
                   '--cc', cc, '--archon-mir', archon, '--scalar-abi', scalar,
                   '--runtime', runtime, '--archon-manifest', manifest]
        probe = json.loads(executor.run([*command, '--probe'], bound=4096))
    except (ValueError, OSError, native.BuildError) as error:
        fail(f'mir-archon unavailable: {error}')
    assembler, linker = Path(probe.pop('assembler')), Path(probe.pop('linker'))
    chosen_tools.update(native_helper=helper, archon=archon, scalar_abi=scalar, runtime=runtime,
                        archon_manifest=manifest, assembler=assembler, linker=linker)
    return [*command, '--assembler', assembler, '--linker', linker], probe


def archvirt_mounts(compiler, executor, chosen_tools):
    # The explicitly selected compiler is a trusted toolchain input. Probe
    # its installation before project policy executes, without passing source
    # paths. The regular-file list never grants a directory or device mount.
    payload = run([compiler, '--print-archvirt-build-inputs'], executor.work / 'cwd',
                  bound=MAX_MANIFEST)
    lines = payload.decode('utf-8').splitlines()
    if not 1 <= len(lines) <= 128 or len(lines) != len(set(lines)):
        fail('invalid ArchVirt build toolchain file list')
    for i, line in enumerate(lines):
        path = Path(line)
        if (not path.is_absolute() or str(path) != line or '..' in path.parts
                or not path.is_file()):
            fail(f'invalid regular file in ArchVirt build toolchain: {line!r}')
        executor.mount(path, exact=True)
        chosen_tools[f'archvirt_build_input/{i}'] = path


def require_vm_policy(config, action):
    if 'vm' not in config:
        fail(f'{action} requires vm opt-in or vm always in the root On1xfile; no host execution fallback')


def archvirt_exec(compiler, command, *arguments, cwd):
    # Cache location is a user setting, not project policy. Pass only absolute
    # ambient cache/home paths and keep the rest of the build environment clean.
    env = dict(ENV)
    for key in ('XDG_CACHE_HOME', 'HOME'):
        value = os.environ.get(key)
        if value and Path(value).is_absolute():
            env[key] = value
    try:
        completed = subprocess.run([compiler, command, *arguments], cwd=cwd, env=env,
                                   stdin=subprocess.DEVNULL, timeout=60)
    except subprocess.TimeoutExpired:
        fail(f'ArchVirt {command} exceeded the 60-second project command limit')
    return 128 - completed.returncode if completed.returncode < 0 else completed.returncode


def archvirt_cache_command(args):
    project = args.root.resolve(strict=True)
    if not project.is_dir():
        fail(f'{project}: expected project directory')
    manifest = project / 'On1xfile'
    config = root_manifest(read(manifest, MAX_MANIFEST), manifest)
    require_vm_policy(config, args.command)
    _, locked = dependency_modes(args)
    entries, _ = read_lock(project, config, required=locked, strict=True)
    if locked:
        require_locked(entries, config)
    arguments = []
    if args.command == 'vm-rerun':
        if not re.fullmatch(r'[0-9a-f]{64}', args.key):
            fail('vm-rerun requires a lowercase 64-hex cache key')
        arguments.append(args.key)
    # Do not run dependency discovery here. The compiler revalidates the
    # exact cached image; neither changed sources nor local policies affect it.
    compiler = compiler_path(args.on1xc)
    with tempfile.TemporaryDirectory(prefix='on1x-project-vm-cache-') as work:
        return archvirt_exec(compiler, '--' + args.command, *arguments, cwd=work)


def vm_launcher(key):
    # `name()` restricts the artifact stem to a portable ASCII identifier.
    return ('#!/bin/sh\n'
            'case "$0" in */*) dir=${0%/*};; *) dir=.;; esac\n'
            f'exec "$dir/.on1x-vm-runner" "$dir/{key}.o1vm" "$@"\n').encode()


def backend_inventory(args):
    """Probe complete tool installations without executing project policy or the produced programs."""
    compiler = compiler_path(args.on1xc)
    helper = ROOT / 'tools/on1x-build.py'
    rows = []
    with tempfile.TemporaryDirectory(prefix='on1x-backend-probe-') as directory:
        work = Path(directory)
        source = work / 'probe.on1x'
        stage_bytes(source, b'fn main()->i64{return 0;}\n')
        for backend in BACKENDS:
            row = {'name': backend}
            try:
                run([sys.executable, helper, source, '--backend', backend, '--no-prelude',
                     '--on1xc', compiler, '--cc', args.cc, '-o', work / backend], work)
                row['available'] = True
            except (OSError, ValueError) as error:
                row.update(available=False, reason=str(error).replace(directory, '<probe>'))
            rows.append(row)
        row = {'name': 'archvirt', 'root_policy': ['always', 'opt-in']}
        try:
            run([compiler, '--vm-launcher=always', '--vm-uncached', '--no-prelude', source,
                 '-o', work / 'archvirt'], work)
            row['available'] = True
        except (OSError, ValueError) as error:
            row.update(available=False, reason=str(error).replace(directory, '<probe>'))
        rows.append(row)
    print(json.dumps({'schema': 'on1x.project-backends/v1', 'compiler_sha256': tool_digest(compiler),
                      'backends': rows}, indent=2, sort_keys=True))


def ffi_generator(args):
    """Feed the canonical build-host generator an ordered, verified snapshot."""
    project = args.root.resolve(strict=True)
    if not project.is_dir():
        fail(f'{project}: expected project directory')
    manifest = project / 'On1xfile'
    snapshot = Snapshot()
    config = root_manifest(snapshot.record('project/On1xfile', read(manifest, MAX_MANIFEST)), manifest)
    if not config['kernel_paths']:
        fail(f'{manifest}: {FFI_GENERATE} requires at least one kernel-path declaration')
    dest = args.output_dir.parent.resolve() / args.output_dir.name
    if any(paths_overlap(dest, project / root) for root in config['kernel_paths']):
        fail(f'{dest}: FFI output cannot overlap a declared kernel-path')
    if dest.exists() or dest.is_symlink():
        fail(f'{dest}: FFI output must not already exist')

    script = FFI_HANDLER / 'ffispec-gen.py'
    if not script.is_file():
        fail(f'{FFI_GENERATE} requires an On1x source checkout with {script.name}')
    config_path = (args.config or FFI_CEPHYR / 'Cephyr.toml').resolve(strict=True)
    library = args.cephyr_library.resolve(strict=True)
    if not config_path.is_file() or not library.is_file():
        fail('--config and --cephyr-library must name regular toolchain files')
    mounts = (FFI_HANDLER, FFI_CEPHYR / 'python', FFI_CEPHYR / 'triples', FFI_CEPHYR / 'stdlib/include',
              config_path, library)
    tools = {'project': Path(__file__).resolve(), 'cephyr_config': config_path, 'cephyr_library': library}
    # Fingerprint the checked-out generator and its canonical helper, policy
    # and Cephyr data inputs. Ignore incidental files such as __pycache__.
    for subtree, patterns in (
            (FFI_HANDLER, ('*.py',)),
            (FFI_CEPHYR, ('python/cephyr/*.py', 'triples/*.lua', 'stdlib/include/**/*.h'))):
        for pattern in patterns:
            for path in sorted(subtree.glob(pattern)):
                tools[path.relative_to(ROOT).as_posix()] = path

    _, locked = dependency_modes(args)
    with tempfile.TemporaryDirectory(prefix='on1x-project-ffi-') as tmp:
        work = Path(tmp).resolve()
        staged, artifact = work / 'input/kernel-inputs', work / 'artifact'
        (work / 'cwd').mkdir()
        artifact.mkdir()
        kernel_inputs = kernel_path_inputs(project, config, snapshot, stage=staged)
        entries, lock_payload = read_lock(project, config, required=locked, snapshot=snapshot,
                                          kernel_inputs=kernel_inputs)
        if locked:
            require_locked(entries, config)
        executor = Execution(args, work, None, None, None, env=dotenv_environment(project, config, args))
        for item in (*mounts, executor.python):
            executor.mount(item)
        tools['python'] = executor.python
        if executor.bwrap is not None:
            tools['bubblewrap'] = executor.bwrap
        tool_hashes = {key: tool_digest(path) for key, path in tools.items()}
        root_names = [f'kernel-inputs/{i}' for i in range(len(kernel_inputs))]
        # A relative path anchored at the private cwd avoids separators in
        # the operator's temp-directory path; sandbox paths are fixed.
        roots = [str(executor.path(work / 'input' / item)) if args.sandbox else f'../input/{item}'
                 for item in root_names]
        executor.run([executor.python, '-sB', script, '--output-dir', executor.path(artifact),
                      '--cephyr-library', library, '--config', config_path],
                     extra_env={'MULIB_KERNELS_PATH': os.pathsep.join(roots), 'PYTHONHASHSEED': '0'},
                     timeout=180)
        index = json.loads(read(artifact / 'index.json', MAX_BUILD_REPORT))
        if not isinstance(index, dict) or not isinstance(index.get('recipes'), (list, dict)):
            fail(f'{script.name}: expected index.json with the canonical recipes manifest')
        outputs = Snapshot()
        with open_directory(artifact, Path('.')) as directory:
            for path, data in regular_tree(directory, artifact, limit=MAX_PROCESS):
                if data is not None:
                    outputs.record(path.as_posix(), data)
        report = {'schema': 'on1x.project-ffi/v1', 'project': config['name'], 'sandbox': bool(args.sandbox),
                  'kernel_paths': kernel_inputs, 'generation_roots': root_names,
                  'inputs': snapshot.inputs, 'tools': tool_hashes, 'outputs': outputs.inputs}
        if lock_payload is not None:
            report['lockfile'] = digest(lock_payload)
        report_bytes = (json.dumps(report, indent=2, sort_keys=True) + '\n').encode()
        if len(report_bytes) > MAX_BUILD_REPORT:
            fail(f'project FFI report exceeds {MAX_BUILD_REPORT} bytes')
        shutil.copytree(staged, artifact / 'kernel-inputs')
        stage_bytes(artifact / '.on1x-ffi.json', report_bytes)
        dest.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=dest.parent, prefix='.on1x-project-ffi-') as publish_dir:
            bundle = Path(publish_dir) / 'output'
            shutil.copytree(artifact, bundle)
            rename = getattr(ctypes.CDLL(None, use_errno=True), 'renameat2', None)
            if rename is None:
                fail('atomic FFI output publication requires Linux renameat2')
            if rename(-100, os.fsencode(bundle), -100, os.fsencode(dest), 1):  # RENAME_NOREPLACE
                code = ctypes.get_errno() or errno.EIO
                raise OSError(code, os.strerror(code), dest)
    print(dest)


def build(args):
    if args.sandbox and args.allow_shell:
        fail('--sandbox cannot be combined with --allow-shell')
    project = args.root.resolve(strict=True)
    if not project.is_dir():
        fail(f'{project}: expected project directory')
    mulib = (args.mulib_root or ROOT.parent).resolve()
    snapshot = Snapshot()
    disk = Disk(project)
    rootinput = snapshot.record('project/On1xfile', disk.get(Path('On1xfile'), MAX_MANIFEST))
    config = root_manifest(rootinput, project / 'On1xfile')
    child_env = dotenv_environment(project, config, args)
    vm_policy = config.get('vm')
    load_time = config.get('kernel-link') == 'load-time'
    launching = args.command == 'run'
    if launching:
        require_vm_policy(config, args.command)
        if args.output_dir or args.project_command:
            fail('run uses a private build and does not take --output-dir or a project command')
    if vm_policy is not None and (args.backend or args.optimize_ir or args.cc is not None or toolchain.requested(args)):
        fail('ArchVirt projects do not accept --backend, --via-mir, --optimize-ir, --cc or native tool overrides')
    compiler = compiler_path(args.on1xc)
    if args.project_command:
        if args.profile or args.release:
            fail('a named On1xfile command cannot be combined with --profile or --release')
        selected = config['commands'].get(args.project_command)
        if selected:
            if (args.sandbox or args.allow_shell or args.target or args.output_dir or args.stdlib
                    or args.cc is not None or toolchain.requested(args) or args.mulib_root or args.libstone_root or args.qmr_compiler
                    or args.mune_runner or args.offline or args.locked or args.frozen
                     or args.scrutiny or args.scrutiny_work is not None
                      or args.opt_select or args.opt_profile or args.backend or args.optimize_ir):
                fail('project formatting commands do not take build options')
            target, source = selected
            if not source.is_relative_to(config.get('directory', Path('.'))):
                fail(f'{source}: formatter input is outside the declared project directory')
            input_path = contained(project, source)
            disk.get(source, MAX_SOURCE)  # Reject non-regular, oversized or linked inputs before invoking the compiler.
            sys.stdout.buffer.write(run([compiler, 'fmt', input_path, '--target', target], project, env=child_env))
            return
        args.profile = args.project_command
    analysis_work = (scrutiny_work(args.scrutiny_work) if args.scrutiny_work is not None
                     else config.get('scrutiny-work'))
    chosen = args.profile or ('release' if args.release else 'dev')
    name(chosen)
    backend = ('archvirt' if vm_policy else
               args.backend or config['backend_scopes'].get(chosen) or config.get('backend', 'c11'))
    if load_time and (backend != 'c11' or args.mulib_root):
        fail('kernel-link load-time requires direct C11 without --mulib-root')
    tool_options = toolchain.requested(args) or bool(config.keys() & toolchain.DIRECTIVES)
    uses_c11 = backend in ('c11', 'mir-c11') and config['kind'] != 'library'
    if tool_options and not uses_c11:
        fail('native toolchain directives and options require a hosted C11 build')
    selected, tool_files, missing = None, {}, []
    if uses_c11:
        selected = toolchain.resolve(args, {**toolchain.HOST, **config} if load_time else config)
        if load_time and not toolchain.is_host(selected):
            fail('kernel-link load-time requires the host compiler, assembler and linker')
        tool_files, missing = toolchain.resolve_files(selected, args.cephyr_prefix)
        if args.sandbox and not toolchain.is_host(selected):
            fail(f'LibStone project tool mounts are not qualified under --sandbox; {toolchain.HOST_HELP}')
    cc = (program(args.cc or 'cc') if backend == 'mir-archon' or
          selected and any(selected[key] == 'host' for key in toolchain.CHOICES) else None)
    if config['kernels'] and backend not in ('c11', 'mir-c11'):
        fail(f'{backend} cannot link project kernel declarations')
    if config['kind'] == 'library' and backend != 'c11':
        fail('source libraries do not have a MIR code generation stage')
    if config['kind'] == 'library' and (config['deps'] or config['kernels']):
        fail('standalone source libraries cannot declare dependencies or kernels')
    _, locked = dependency_modes(args)
    git_keys = {key for key, dependency in config['deps'].items()
                if dependency.kind in ('git', 'git-remote')}
    kernel_inputs = kernel_path_inputs(project, config, snapshot)
    lock_entries, lock_payload = read_lock(
        project, config, required=locked and bool(git_keys or declared_policy(config) or vm_policy or kernel_inputs),
        strict=True, snapshot=snapshot, kernel_inputs=kernel_inputs)
    if locked:
        require_locked(lock_entries, config)
    resolved_dependencies = {}
    for key, dependency in sorted(config['deps'].items()):
        if dependency.kind == 'local':
            location, commit = dependency.source, None
        else:
            entry = lock_entries.get(key)
            if dependency.selector_kind == 'commit':
                commit = dependency.selector
            elif entry is not None:
                commit = entry['commit']
            else:
                fail(f'{key!r}: Git {dependency.selector_kind} selector is not locked; '
                     'run on1x-project.py lock')
            location = (dependency.source if dependency.kind == 'git'
                        else cache_relative(key, commit))
        resolved_dependencies[key] = (dependency, location, commit)
    directory = config.get('directory', Path('.'))
    if any(directory.is_relative_to(location)
           for _, location, _ in resolved_dependencies.values()):
        fail('the project source directory cannot be inside a dependency store')
    if args.target and args.target not in config['targets']:
        fail(f'unknown target {args.target!r}')
    targets = {key: source for key, source in config['targets'].items() if not args.target or args.target == key}
    if launching and len(targets) != 1:
        fail('run requires exactly one target; select it with --target')
    dest = None
    if not launching:
        dest = (args.output_dir or contained(project, Path('target') / chosen)).absolute()
        if dest == project or project.is_relative_to(dest) or dest.name in ('', '.', '..'):
            fail(f'invalid output directory: {dest}')
        dest = dest.parent.resolve() / dest.name
        if dest == project or project.is_relative_to(dest):
            fail(f'invalid output directory: {dest}')
        if any(paths_overlap(dest, project / root) for root in config['kernel_paths']):
            fail(f'{dest}: project output cannot overlap a declared kernel-path')
        disk.exclude.add(dest)
    disk.exclude.update(project / location for _, location, _ in resolved_dependencies.values())
    with tempfile.TemporaryDirectory(prefix='on1x-project-') as tmp:
        work = Path(tmp).resolve()
        (work / 'input/project').mkdir(parents=True)
        (work / 'input/deps').mkdir()
        (work / 'cwd').mkdir()
        (work / 'artifact').mkdir()
        executor = Execution(args, work, compiler, cc, mulib, env=child_env, load_time=load_time)
        chosen_tools = {'on1xc': compiler, 'project': Path(__file__).resolve(),
                         'optimization_helper': Path(optimization.__file__)}
        if executor.bwrap is not None:
            chosen_tools['bubblewrap'] = executor.bwrap
        # A selected ArchVirt compiler wrapper can declare its backend binary
        # here; make those files available before sandboxed compiler queries.
        if vm_policy and args.sandbox:
            archvirt_mounts(compiler, executor, chosen_tools)
        std_package = config['kind'] == 'library' and config['name'] == 'std'
        stdlib = None if std_package else executor.standard_library(compiler, args.stdlib)
        if vm_policy and not args.sandbox and args.command != 'plan':
            archvirt_mounts(compiler, executor, chosen_tools)
        if cc is not None:
            chosen_tools['cc'] = cc
        if uses_c11:
            chosen_tools['build_helper'] = ROOT / 'tools/on1x-build.py'
            chosen_tools['native_toolchain_helper'] = Path(toolchain.__file__)
            chosen_tools.update(tool_files)
        if load_time:
            runtime_archive, link_policy, _ = toolchain.kernel_import_link(compiler)
            chosen_tools.update(kernel_import_runtime=runtime_archive, kernel_import_link=link_policy)
            for path in (runtime_archive, link_policy):
                executor.mount(path)
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
                if mode == 'profile':
                    mode = 'profile-context'
                    records = [('data', key, value) for key, value in sorted(model.items())]
                    records.extend((('context', 'backend', backend),
                                    ('context', 'directory', directory.as_posix())))
                    if selected is not None:
                        records.extend(('context', stage, selected[stage]) for stage in toolchain.CHOICES)
                    if args.target:
                        records.append(('context', 'target', args.target))
                    records.extend(('target', key, path.as_posix()) for key, path in sorted(targets.items()))
                    records.extend((kind, key, value) for key, (kind, value) in
                                   sorted(config['profile_args'].get(chosen, {}).items()))
                else:
                    records = sorted(model.items())
                try:
                    return executor.run([runner, mode, executor.path(policy_file), executor.path(Path(stream.name)),
                                         *(word for record in records for word in record)],
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

        if chosen in ('dev', 'release', 'debug', 'default') and chosen not in config['profiles']:
            setting = (3, 0) if chosen == 'release' else (0, 1)
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
        if selected and selected['compiler'] == 'cephyr':
            if chosen in config['profiles'] and setting != (0, 0):
                fail('project Lua C optimization/debug flags are not supported by Cephyr C11')
            if chosen == 'debug':
                fail(f'Cephyr C11 has no DWARF debug mode; {toolchain.HOST_HELP}')
            cflags = ['-O0']
        # Keep ordinary dev and existing custom C profiles backwards compatible.
        builtin = chosen if chosen in optimization.PROFILES and chosen not in config['profiles'] else None
        opt_profile = args.opt_profile or config.get('optimization-profile') or builtin or 'default'
        cli_select = args.opt_select or {}
        selections = dict(config['selections'])
        for layer in (config['scoped'].get(chosen, {}), cli_select):
            if 'all' in layer:
                selections.clear()
            selections.update(layer)
        if args.optimize_ir and backend == 'c11':
            fail('--optimize-ir requires a MIR backend')
        native_command = None
        backend_info = {'name': backend}
        if backend == 'archvirt':
            backend_info.update(artifact='archvirt-launcher', vm=vm_policy)
            cflags = []
        elif backend == 'mir-archon':
            native_command, probe = native_tools(compiler, cc, executor, chosen_tools)
            backend_info.update(artifact='elf-executable', **probe)
        elif backend == 'mir-vm':
            runner = compiler.parent / 'on1x-vm'
            if not runner.is_file() or not os.access(runner, os.X_OK):
                fail('mir-vm unavailable: missing on1x-vm beside the selected compiler')
            chosen_tools['vm_runner'] = runner
            executor.mount(runner)
            cflags = []
            backend_info.update(artifact='bytecode', runner='.on1x-vm-runner')
        else:
            backend_info['artifact'] = 'source-library' if config['kind'] == 'library' else 'host-executable'
            if selected is not None:
                backend_info['toolchain'] = {**selected, 'missing': missing,
                                             'runtime': ('host' if toolchain.is_host(selected) else 'cephyr-static-amd64')}
        if config['kind'] != 'library':
            backend_info['artifacts'] = sorted([*targets] + (
                [key + '.o1vm' for key in targets] + ['.on1x-vm-runner'] if backend == 'mir-vm' else []))
        opt_flags = optimization.flags(opt_profile, selections)
        if args.optimize_ir:
            opt_flags.append('--optimize-ir')
        inventory = json.loads(executor.run([compiler, '--list-optimizations', *opt_flags], bound=MAX_MANIFEST))
        stages = {entry['name']: entry['stage'] for entry in inventory['optimizations']}
        active_stages = ['processor']
        if backend == 'c11' and 'c11' in stages.values():  # Older on1xc inventories.
            active_stages.append('c11')
        if backend in ('c11', 'mir-vm') and 'hir' in stages.values():
            active_stages.append('hir')
        if backend not in ('c11', 'archvirt'):
            active_stages.extend(['lowering', 'mir'])
        for key in sorted(declared_names(config)):
            if key not in stages:
                fail(f'unknown optimization in On1xfile: {key}')
        for key in selections:
            if key != 'all' and backend == 'archvirt' and stages[key] not in active_stages:
                fail(f'optimization {key} is not available for the project ArchVirt backend')
            if key != 'all' and stages[key] == 'hir' and 'hir' not in active_stages:
                fail(f'optimization {key} requires the C11 or mir-vm backend')
            if key != 'all' and stages[key] not in active_stages and backend == 'c11':
                fail(f'optimization {key} requires a MIR backend')
            if key != 'all' and stages[key] == 'c11' and backend != 'c11':
                fail(f'optimization {key} requires the C11 backend')
        check_flags = optimization.flags(opt_profile, {key: on for key, on in selections.items()
                                                       if key == 'all' or stages[key] == 'processor'})
        policy = dict(backend=backend, args=opt_flags, declared=declared_policy(config),
                      active_stages=active_stages, **inventory)
        project_stage = work / 'input/project'
        deps_stage = work / 'input/deps'
        root_members = {}
        roles = layouts(disk, config, 'project', project_stage, snapshot, preprocess, membership=root_members)
        dep_modules = {}
        dep_roles = {}
        dep_members = {}
        dep_report = {}
        for key, (dependency, relative, revision) in resolved_dependencies.items():
            location = contained(project, relative)
            if dependency.kind == 'git-remote' and not location.exists():
                fail(f'{location}: remote Git objects are missing; run on1x-project.py fetch {key} explicitly')
            stage = deps_stage / key
            with tempfile.TemporaryDirectory(dir=work, prefix='git-') as objects:
                provider = (Git(location, revision, work / 'cwd', Path(objects) / 'git')
                            if dependency.kind in ('git', 'git-remote') else Disk(location))
                entries = dependency_layout(provider, key, stage, snapshot, membership=dep_members.setdefault(key, {}))
            dep_modules[key] = [stage / path for path, role in entries.items() if role == 'library']
            dep_roles[key] = entries
            record = {'kind': dependency.kind, **({'commit': revision} if revision else {})}
            if dependency.selector_kind in ('branch', 'tag'):
                record['selector'] = {'kind': dependency.selector_kind,
                                      'value': dependency.selector}
            dep_report[key] = record

        project_modules = {project_stage / path for path, role in roles.items() if role == 'library'}
        project_programs = [project_stage / path for path, role in roles.items() if role in ('source', 'library')]
        for path in sorted(project_programs):
            source_policy(executor, compiler, path, project_stage / directory,
                          project_modules, programs=config['programs'])
        for name, module in sorted(root_members.items()):
            for path in module.sources:
                source_policy(executor, compiler, project_stage / path, project_stage / directory,
                              project_modules, declared_module=name, programs=config['programs'])
        for key, paths in dep_modules.items():
            own_files = set(paths)
            for path in sorted(paths):
                source_policy(executor, compiler, path, deps_stage / key, own_files)
            for name, module in sorted(dep_members[key].items()):
                for path in module.sources:
                    source_policy(executor, compiler, deps_stage / key / path, deps_stage / key,
                                  own_files, declared_module=name)
        dep_sources = [path for paths in dep_modules.values() for path in paths]
        # A std package must use its own staged pervasive module and imports,
        # not fill missing declarations from the installed standard library.
        standard = executor.path(project_stage / directory) if std_package else stdlib
        trusted_roots = () if std_package else (stdlib, stdlib.parent / 'std')
        loader = ['--stdlib', str(standard), '-I', str(executor.path(deps_stage)),
                  '-I', str(executor.path(project_stage))]
        dependency_base = loader[:-2]
        if root_members and config['name'] in dep_members:
            fail(f'project module package shares declared dependency name {config["name"]!r}')
        packages = {key: (deps_stage / key, members, key + '.') for key, members in dep_members.items()}
        if root_members:
            packages[config['name']] = (project_stage, root_members, '')
        index_flags, module_owners = module_index(executor, work / 'input/module-index.json', project_stage,
                                                  packages, config['programs'])
        loader += index_flags
        module_edges = set()
        kernel_flags = ['--kernel-load-time'] if load_time else []
        graph_loader = [*loader, *kernel_flags]
        analysis_flags = []
        if config.get('scrutiny'):
            analysis_flags.extend(('--project-scrutiny', config['scrutiny']))
        if args.scrutiny:
            analysis_flags.extend(('--scrutiny', args.scrutiny))
        if analysis_work is not None:
            analysis_flags.extend(('--scrutiny-work', str(analysis_work)))
        compiler_flags = [*graph_loader, *analysis_flags, *check_flags]
        for key, modules in dep_modules.items():
            isolated_flags, isolated_owners = module_index(
                executor, work / 'input' / f'dep-module-index-{key}.json', deps_stage / key,
                {key: (deps_stage / key, dep_members[key], '')})
            dependency_loader = [*dependency_base, *isolated_flags, *kernel_flags]
            dependency_compiler_flags = [*dependency_loader, *analysis_flags, *check_flags]
            legacy_files = {executor.path(path) for path in modules}
            own_files = legacy_files | isolated_owners.keys()
            for entry in sorted(legacy_files):
                verify_graph(executor, compiler, entry, dependency_loader, own_files, trusted_roots)
            verify_interfaces(executor, compiler, deps_stage / key, dep_roles[key],
                               dependency_loader, dependency_compiler_flags,
                               own_files, trusted_roots, graph_checked=legacy_files)
            check_manifest_members(executor, compiler, deps_stage / key, dep_members[key], dependency_loader,
                                   dependency_compiler_flags, own_files, trusted_roots, module_edges)
        permitted = {executor.path(path) for path in [*project_modules, *dep_sources]} | module_owners.keys()
        verify_interfaces(executor, compiler, project_stage, roles, graph_loader,
                          compiler_flags, permitted, trusted_roots)
        check_manifest_members(executor, compiler, project_stage, root_members, graph_loader,
                               compiler_flags, permitted, trusted_roots, module_edges)
        outputs = {}
        guest = work / 'artifact' / (next(iter(targets)) + '.elf') if launching else None
        if backend == 'mir-vm' and args.command != 'plan':
            bundled = work / 'artifact/.on1x-vm-runner'
            shutil.copyfile(runner, bundled)
            bundled.chmod(0o755)
            outputs[bundled.name] = tool_digest(bundled)
        for key, source in sorted(targets.items()):
            entry = executor.path(project_stage / source)
            if index_flags:
                inspect_module_graph(executor, compiler, entry, graph_loader, permitted | {entry},
                                     trusted_roots, module_edges)
            else:
                verify_graph(executor, compiler, entry, graph_loader, permitted | {entry}, trusted_roots)
            if args.command == 'plan':
                continue
            if backend in ('archvirt', 'mir-vm'):
                # Source locations in VM artifacts must not pin temporary
                # build directories or disclose their names at runtime.
                source_roots = [(executor.path(project_stage), '/__on1x__/project'),
                                (executor.path(deps_stage), '/__on1x__/deps'),
                                (standard, '/__on1x__/stdlib'), (stdlib.parent / 'std', '/__on1x__/std')]
                map_flags = [v for src, dest_root in source_roots
                             for v in ('--vm-source-prefix', src, dest_root)]
            if backend == 'archvirt':
                artifact = guest if launching else work / 'artifact' / key
                mode = '--vm-build' if launching else '--vm-launcher=' + vm_policy
                executor.run([compiler, mode, entry, '-o', executor.path(artifact),
                              '--vm-uncached', *loader, *analysis_flags, *opt_flags, *map_flags], timeout=180)
                outputs[key] = digest(read(artifact, MAX_PROCESS))
                continue
            output = executor.path(work / 'artifact' / key)
            if backend == 'mir-vm':
                tape = work / 'artifact' / (key + '.o1vm')
                executor.run([compiler, '--emit-vm', entry, '-o', executor.path(tape),
                              *loader, *analysis_flags, *opt_flags, *map_flags])
                executor.run([executor.path(bundled), '--verify', executor.path(tape)])
                launcher = work / 'artifact' / key
                stage_bytes(launcher, vm_launcher(key))
                launcher.chmod(0o755)
                outputs[tape.name] = digest(read(tape, MAX_PROCESS))
                outputs[key] = digest(read(launcher, 4096))
                continue
            if backend == 'mir-archon':
                executor.run([*native_command, entry, '-o', output, *opt_flags,
                              *(f'--on1xc-arg={flag}' for flag in [*loader, *analysis_flags]),
                              *(f'--cflag={flag}' for flag in cflags)], timeout=180)
                outputs[key] = digest(read(work / 'artifact' / key, MAX_PROCESS))
                continue
            toolchain.assert_available(missing)
            native_args = [f'--{stage}={selected[stage]}' for stage in toolchain.CHOICES]
            native_args.extend(f'--{key}={value}' for key in toolchain.ARGUMENTS for value in selected[key])
            if cc is not None:
                native_args.extend(('--cc', cc))
            if args.cephyr_prefix:
                native_args.extend(('--cephyr-prefix', args.cephyr_prefix.resolve()))
            elif 'cephyr' in tool_files:
                # The project runner does not inherit the operator's PATH.
                native_args.extend(('--_cephyr-executable', tool_files['cephyr']))
            command = [executor.python, ROOT / 'tools/on1x-build.py', entry, '-o', output,
                        '--on1xc', compiler,
                        *kernel_flags, *([] if load_time else ['--mulib-root', mulib]),
                        *loader, *analysis_flags, *native_args,
                        *(['--via-mir'] if backend == 'mir-c11' else []), *opt_flags,
                        *(f'--cflag={flag}' for flag in cflags)]
            if '-g' in cflags:
                command.extend(('--debug-build-root', executor.path(work)))
            for kernel in config['kernels']:
                command.extend(('--mulib', kernel))
            if config['kernels']:
                command.extend(('--mulib-plan', executor.path(work / 'artifact' / (key + '.mulib.json'))))
            executor.mount(executor.python)
            executor.run(command, timeout=180)
            outputs[key] = digest(read(work / 'artifact' / key, MAX_PROCESS))
        if guest is not None:
            # --sandbox controls the build only. The runtime validates and
            # launches the staged guest, never the raw ELF as a host program.
            command = '--vm-register-elf' if args.prepare else '--vm-run-elf'
            return archvirt_exec(compiler, command, guest, cwd=work / 'cwd')
        if config['kind'] == 'library':
            # The language checker requires a main. Validate each project library
            # through a fresh declaration-only carrier, then publish source only.
            for i, path in enumerate(sorted(project_modules)):
                check = work / 'input' / f'check_{i}.on1x'
                stage_bytes(check, f'import "project/{path.relative_to(project_stage).as_posix()}";\n'
                                   'fn main() -> i64 { return 0; }\n'.encode())
                entry = executor.path(check)
                verify_graph(executor, compiler, entry, loader, permitted | {entry}, trusted_roots)
                executor.run([compiler, '--check', entry, *compiler_flags])
        if config['kind'] == 'library' and args.command != 'plan':
            package = work / 'artifact' / 'library'
            shutil.copytree(project_stage, package)
            # Preserve checked interface bytes, but publish only plain,
            # post-preprocessing declarations with the certified role set.
            directory = config.get('directory', Path('.')).as_posix()
            certificate = (f'on1xfile 1\nkind library\nname {json.dumps(config["name"])}\n'
                           f'directory {json.dumps(directory)}\n' + module_certificate(root_members))
            if len(certificate.encode()) > MAX_MANIFEST:
                fail('published On1xfile exceeds its byte budget')
            package_files = {*roles, Path('On1xfile')}
            stage_bytes(package / 'On1xfile', certificate.encode())
            for directory in sorted({path.parent for path in roles}):
                for suffix, accepted in (('m', ('library', 'resource')), ('i', ('interface',))):
                    inputs = sorted((path.name, role) for path, role in roles.items()
                                    if path.parent == directory and role in accepted)
                    if inputs:
                        declarations = ''.join(f'{role} {json.dumps(path)}\n' for path, role in inputs)
                        manifest = directory / ('On1xfile.' + suffix)
                        stage_bytes(package / manifest, ('on1xfile 1\n' + declarations).encode())
                        package_files.add(manifest)
            for path in sorted(package_files):
                outputs['library/' + path.as_posix()] = digest(read(package / path, MAX_SOURCE))

        report = dict(schema='on1x.project-build/v1', project=config['name'], kind=config['kind'],
                      profile=chosen, cflags=cflags, sandbox=bool(args.sandbox), shell=bool(args.allow_shell),
                      optimization=policy, backend=backend_info,
                      scrutiny=args.scrutiny or config.get('scrutiny'),
                      dependencies=dep_report, inputs=snapshot.inputs, outputs=outputs,
                      tools={key: tool_digest(path) for key, path in chosen_tools.items()})
        if module_owners:
            report['module_graph'] = module_graph(module_owners, sorted(module_edges))
        if analysis_work is not None:
            report['scrutiny_work'] = analysis_work
        if lock_payload is not None:
            report['lockfile'] = digest(lock_payload)
        if kernel_inputs:
            report['kernel_paths'] = kernel_inputs
        if load_time:
            report['kernel_link'] = 'load-time'
        if backend == 'mir-vm' and args.command != 'plan' and report['tools']['vm_runner'] != outputs[bundled.name]:
            fail('mir-vm runner changed during project build')
        if args.command == 'plan':
            report.update(schema='on1x.project-plan/v1', targets={k: p.as_posix() for k, p in targets.items()})
            del report['outputs']
            print(json.dumps(report, indent=2, sort_keys=True))
            return
        report_bytes = (json.dumps(report, indent=2, sort_keys=True) + '\n').encode()
        if len(report_bytes) > MAX_BUILD_REPORT:
            fail(f'project build report exceeds {MAX_BUILD_REPORT} bytes')
        stage_bytes(work / 'artifact/.on1x-build.json', report_bytes)
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
        config = root_manifest(data, path)
        if args.on1xc:
            validate_locked_names(args, config, path.resolve().parent)
    elif path.name in ('On1xfile.m', 'On1xfile.i'):
        if QMR_TAG.search(data):
            fail(f'{path}: formatting QMR templates is unsupported; no template code was executed')
        local_roles(data, path, Path(path.name), None)
    else:
        fail(f'{path}: expected On1xfile, On1xfile.m, or On1xfile.i')
    lines = []
    for _, fields_on_line, head, comment, continued in manifest_lines(data, path):
        words = [json.dumps(value, ensure_ascii=False) if quoted else value
                 for value, quoted in fields_on_line]
        if continued:
            words.append('\\')
        if comment:
            words.append(comment)
        indent = '  ' if fields_on_line and not head else ''
        lines.append(indent + ' '.join(words))
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
    init_spec = importlib.util.spec_from_file_location(
        '_on1x_project_init', Path(__file__).resolve().with_name('on1x_project_init.py'))
    project_init = importlib.util.module_from_spec(init_spec)
    init_spec.loader.exec_module(project_init)
    project_init.add_command(commands, root_manifest=root_manifest, local_roles=local_roles,
                             local=local, max_manifest=MAX_MANIFEST, qmr_tag=QMR_TAG)
    command = argparse.ArgumentParser(add_help=False)
    command.add_argument('root', type=Path, nargs='?', default=Path('.'))
    command.add_argument('--on1xc')
    command.add_argument('--cc', help='explicit host C driver; defaults unset native stages to host')
    toolchain.arguments(command)
    command.add_argument('--stdlib', type=Path)
    command.add_argument('--mulib-root', type=Path)
    command.add_argument('--libstone-root', type=Path)
    command.add_argument('--mune-runner')
    command.add_argument('--qmr-compiler')
    mode = command.add_mutually_exclusive_group()
    mode.add_argument('--profile')
    mode.add_argument('--release', action='store_true')
    optimization.arguments(command, alias=False)
    backends = command.add_mutually_exclusive_group()
    backends.add_argument('--backend', choices=BACKENDS,
                          help='override the selected build profile and root backend')
    backends.add_argument('--via-mir', dest='backend', action='store_const', const='mir-c11',
                          help='shorthand for --backend mir-c11')
    command.add_argument('--optimize-ir', action='store_true')
    command.add_argument('--target')
    command.add_argument('--project-command', help=argparse.SUPPRESS)
    command.add_argument('--output-dir', type=Path, help='exact generation output directory')
    command.add_argument('--sandbox', action='store_true', help='require Linux bubblewrap isolation; no fallback')
    command.add_argument('--allow-shell', action='store_true', help='allow explicit root shell defines outside sandbox')
    command.add_argument('--scrutiny', help='override source and On1xfile static-analysis scrutiny')
    command.add_argument('--scrutiny-work', metavar='UNITS',
                         help='override the On1xfile static-analysis work budget (1..10000000)')
    command.add_argument('--offline', action='store_true',
                         help='explicitly require the normal network-free build behavior')
    command.add_argument('--locked', action='store_true',
                         help='require a complete, current On1xfile.lock')
    command.add_argument('--frozen', action='store_true',
                          help='equivalent to --offline --locked')
    commands.add_parser('build', parents=[command])
    commands.add_parser('plan', parents=[command], help='print the resolved build policy and inputs without publishing')
    ffi = commands.add_parser(FFI_GENERATE, help='generate build-host FFI material using declared kernel paths')
    ffi.add_argument('root', type=Path, nargs='?', default=Path('.'))
    ffi.add_argument('--output-dir', type=Path, required=True, help='new directory for the generated FFI bundle')
    ffi.add_argument('--cephyr-library', type=Path, required=True, help='trusted build-host Cephyr shared library')
    ffi.add_argument('--config', type=Path, help='trusted Cephyr configuration (default: checkout Cephyr.toml)')
    ffi.add_argument('--sandbox', action='store_true', help='require Linux bubblewrap isolation; no fallback')
    ffi.add_argument('--offline', action='store_true', help='do not fetch dependencies (the default)')
    ffi.add_argument('--locked', action='store_true', help='require a complete, current On1xfile.lock')
    ffi.add_argument('--frozen', action='store_true', help='equivalent to --offline --locked')
    launch = commands.add_parser('run', parents=[command], help='build, cache and run one declared ArchVirt target')
    launch.add_argument('--prepare', action='store_true', help='cache the VM and print its ID without booting it')
    for action, description in (
            ('vm-rerun', 'replay an exact cached ArchVirt guest without rebuilding the project'),
            ('vm-cache-clear', "clear the current user's ArchVirt cache")):
        cache = commands.add_parser(action, help=description)
        cache.add_argument('root', type=Path, nargs='?', default=Path('.'))
        if action == 'vm-rerun':
            cache.add_argument('key', help='cache key printed by an earlier VM run')
        cache.add_argument('--on1xc')
        cache.add_argument('--offline', action='store_true')
        cache.add_argument('--locked', action='store_true')
        cache.add_argument('--frozen', action='store_true', help='equivalent to --offline --locked')
    inventory = commands.add_parser('backends', help='probe the actual installed backend tools using fixed input')
    inventory.add_argument('--on1xc')
    inventory.add_argument('--cc', default='cc')
    formatting = commands.add_parser('format', help='validate and format a plain On1xfile without running its policy')
    formatting.add_argument('input', type=Path)
    formatting.add_argument('--check', action='store_true')
    formatting.add_argument('--on1xc', help='also validate optimizer names against this compiler inventory')
    fetch = commands.add_parser('fetch', help='explicitly resolve and fetch one git-remote library over HTTPS')
    fetch.add_argument('name', help='dependency name declared in On1xfile')
    fetch.add_argument('root', type=Path, nargs='?', default=Path('.'))
    fetch.add_argument('--on1xc', help='compiler used to validate named optimization directives')
    fetch.add_argument('--ca-file', type=Path, help='explicit trusted PEM CA bundle for this fetch')
    fetch.add_argument('--offline', action='store_true', help='validate only an existing locked cache entry')
    fetch.add_argument('--locked', action='store_true', help='do not change On1xfile.lock')
    fetch.add_argument('--frozen', action='store_true', help='equivalent to --offline --locked')
    locking = commands.add_parser('lock', help='resolve Git selectors and atomically write On1xfile.lock')
    locking.add_argument('root', type=Path, nargs='?', default=Path('.'))
    locking.add_argument('--on1xc', help='compiler used to validate named optimization directives')
    locking.add_argument('--ca-file', type=Path, help='explicit trusted PEM CA bundle for HTTPS Git')
    locking.add_argument('--offline', action='store_true', help='use local stores and existing remote cache entries only')
    locking.add_argument('--locked', action='store_true', help='validate without changing On1xfile.lock')
    locking.add_argument('--frozen', action='store_true', help='equivalent to --offline --locked')
    try:
        argv = sys.argv[1:]
        if argv[:1] == ['run'] and '--' in argv:
            separator = argv.index('--')
            if argv[separator + 1:]:
                fail('ArchVirt project run does not support guest arguments')
            argv = argv[:separator]
        arguments = parser.parse_args(argv)
        if arguments.command == 'init':
            arguments.init_project(arguments)
            return 0
        status = {'build': build, 'plan': build, 'run': build, 'backends': backend_inventory,
                  'format': format_manifest, 'fetch': fetch_dependency,
                  FFI_GENERATE: ffi_generator,
                  'vm-rerun': archvirt_cache_command, 'vm-cache-clear': archvirt_cache_command,
                  'lock': lock_dependencies}[arguments.command](arguments)
    except (ValueError, OSError, KeyError, TypeError) as error:
        print(f'on1x-project: {error}', file=sys.stderr)
        return 1
    return status or 0


if __name__ == '__main__':
    sys.exit(main())
