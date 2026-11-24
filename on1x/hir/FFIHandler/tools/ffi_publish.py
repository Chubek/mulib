"""Bounded validation, freshness and atomic publication of generated stdffi."""
from concurrent.futures import FIRST_COMPLETED, ThreadPoolExecutor, wait
from contextlib import contextmanager
import ctypes
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import tempfile


SCHEMA = 'on1x.stdffi.mulib/v1'
DEFAULT_TARGET = 'x86_64-linux-gnu'
TARGETS = {DEFAULT_TARGET, 'aarch64-linux-gnu', 'riscv64-linux-gnu'}
CATALOGUE_ROOT = Path('on1x/stdffi/mulib')
MAX_JOBS = 16
MAX_MODULES = 4096
MAX_FILE_BYTES = 64 * 1024
MAX_MODULE_BYTES = 128 * 1024 * 1024
MAX_INDEX_BYTES = 64 * 1024 * 1024
_KEY = re.compile(r'[a-z0-9][a-z0-9_+-]{0,95}')
_DIGEST = re.compile(r'[0-9a-f]{64}')


def _package_path(path, target, canonical_root=None):
    if target not in TARGETS:
        raise ValueError(f'unsupported stdffi target: {target}')
    original = Path(path).absolute()
    if original.is_symlink():
        raise ValueError('stdffi package cannot be a symlink')
    try:
        output = original.resolve()
        real_root = canonical_root.resolve() if canonical_root is not None else None
    except RuntimeError as error:
        raise ValueError(f'cannot resolve stdffi catalogue path: {original}') from error
    for candidate in (original, output):
        roots = [p for p in (candidate, *candidate.parents)
                 if p.parts[-len(CATALOGUE_ROOT.parts):] == CATALOGUE_ROOT.parts]
        if canonical_root is not None:
            if any(root != candidate and root.is_relative_to(candidate)
                   for root in (canonical_root, real_root)):
                raise ValueError(f'output overlaps the stdffi catalogue root: {canonical_root}')
            roots += [canonical_root, real_root]
        for root in roots:
            if candidate.is_relative_to(root) and (
                    candidate != root / target or original != output):
                raise ValueError(f'stdffi catalogue path must be the real {root / target} directory')
    return output


def catalogue_path(root, target, output=None):
    """Select an exact target slot, or a standalone explicit package directory."""
    canonical_root = Path(root).resolve() / CATALOGUE_ROOT
    return _package_path(canonical_root / target if output is None else output,
                         target, canonical_root)


def bounded_map(generate, entries, jobs):
    """Run bounded independent work; return results in manifest/input order."""
    if type(jobs) is not int or not 1 <= jobs <= MAX_JOBS:
        raise ValueError(f'generation jobs must be between 1 and {MAX_JOBS}')
    pending = {}
    results = []
    source = iter(entries)
    with ThreadPoolExecutor(max_workers=jobs) as workers:
        try:
            def submit():
                try:
                    entry = next(source)
                except StopIteration:
                    return
                if len(results) >= MAX_MODULES:
                    raise ValueError('generation module count limit exceeded')
                pending[workers.submit(generate, entry)] = len(results)
                results.append(None)

            for _ in range(jobs):
                submit()
            while pending:
                complete, _ = wait(pending, return_when=FIRST_COMPLETED)
                for work in sorted(complete, key=pending.get):
                    results[pending.pop(work)] = work.result()
                    submit()
        except BaseException:
            for work in pending:
                work.cancel()
            raise
    return results


def _read(path, limit):
    if path.is_symlink() or not path.is_file():
        raise ValueError('not a regular stdffi package file: ' + path.name)
    with path.open('rb') as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise ValueError('stdffi package byte limit: ' + path.name)
    return data


def _snapshot(directory):
    if directory.is_symlink():
        raise ValueError('stdffi package cannot be a symlink')
    if not directory.exists():
        return None
    if not directory.is_dir():
        raise ValueError('stdffi package must be a directory')
    files = {}
    total = modules = 0
    for path in directory.iterdir():
        if path.name == 'index.json':
            data = _read(path, MAX_INDEX_BYTES)
        else:
            if path.suffix != '.ffi' or not _KEY.fullmatch(path.stem):
                raise ValueError('unexpected stdffi package entry: ' + path.name)
            modules += 1
            if modules > MAX_MODULES:
                raise ValueError('stdffi package module count limit exceeded')
            data = _read(path, min(MAX_FILE_BYTES, MAX_MODULE_BYTES - total))
            total += len(data)
        files[path.name] = hashlib.sha256(data).hexdigest()
    return files


def _unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate stdffi index field: ' + key)
        result[key] = value
    return result


def _invalid_constant(value):
    raise ValueError('invalid stdffi index scalar: ' + value)


def _check_index(index, target):
    if not isinstance(index, dict) or index.get('schema') != SCHEMA:
        raise ValueError('unsupported stdffi index schema')
    if index.get('target') != target:
        raise ValueError('stdffi index target mismatch; select a separate output directory')
    if not isinstance(index.get('modules'), list) or len(index['modules']) > MAX_MODULES:
        raise ValueError('invalid stdffi index module list')
    pending = [(index, 0)]
    nodes = 0
    while pending:
        node, depth = pending.pop()
        nodes += 1
        if depth > 64 or nodes > 2_000_000:
            raise ValueError('stdffi index depth/node limit exceeded')
        if isinstance(node, dict):
            pending.extend((value, depth + 1) for value in node.values())
        elif isinstance(node, list):
            pending.extend((value, depth + 1) for value in node)
    files = {}
    for record in index['modules']:
        if not isinstance(record, dict):
            raise ValueError('invalid stdffi module record')
        key, name, digest = (record.get(field) for field in ('key', 'file', 'ffi_sha256'))
        if (not isinstance(key, str) or not _KEY.fullmatch(key)
                or name != key + '.ffi' or name in files
                or not isinstance(digest, str) or not _DIGEST.fullmatch(digest)):
            raise ValueError('invalid or duplicate stdffi module identity')
        files[name] = digest
    return files


def _load_index(directory, target):
    try:
        index = json.loads(_read(directory / 'index.json', MAX_INDEX_BYTES),
                           object_pairs_hook=_unique_pairs, parse_constant=_invalid_constant)
    except (UnicodeError, RecursionError) as error:
        raise ValueError('invalid stdffi package index') from error
    return index, _check_index(index, target)


def _difference(expected, actual):
    unexpected = sorted(actual.keys() - expected.keys())
    missing = sorted(expected.keys() - actual.keys())
    changed = sorted(k for k in expected.keys() & actual.keys() if expected[k] != actual[k])
    parts = []
    for label, names in [('missing', missing), ('unexpected', unexpected), ('changed', changed)]:
        if names:
            parts.append(f'{label} ({len(names)}): ' + ', '.join(names[:8]))
    return '; '.join(parts)


def check_package(output, target, expected_keys):
    """Check a published complete package against the actual manifest keys."""
    output = _package_path(output, target)
    before = _snapshot(output)
    if not before or 'index.json' not in before:
        raise ValueError(f'missing stdffi index for {target}: {output / "index.json"}')
    index, expected = _load_index(output, target)
    found = {name: digest for name, digest in before.items() if name != 'index.json'}
    if found != expected:
        raise ValueError('stdffi package drift: ' + _difference(expected, found))
    if {record['key'] for record in index['modules']} != set(expected_keys):
        raise ValueError('stdffi package does not account for exactly the registered manifest keys')
    if _snapshot(output) != before:
        raise ValueError('published stdffi package changed during check')
    return index


def _sync(directory):
    for path in directory.iterdir():
        with path.open('rb') as stream:
            os.fsync(stream.fileno())
    fd = os.open(directory, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def _rename(stage, output, *, exchange):
    # Never fall back to sequential renames: readers could see missing/mixed generations.
    renameat2 = getattr(ctypes.CDLL(None, use_errno=True), 'renameat2', None)
    if renameat2 is None:
        raise ValueError('atomic stdffi publication requires Linux renameat2')
    renameat2.argtypes = (ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p,
                          ctypes.c_uint)
    renameat2.restype = ctypes.c_int
    if renameat2(-100, os.fsencode(stage), -100, os.fsencode(output), 2 if exchange else 1):
        error = ctypes.get_errno()
        raise OSError(error, 'atomic stdffi publication: ' + os.strerror(error))


class _Package:
    def __init__(self, stage, output, target, check, full, previous_files):
        self.stage = stage
        self.previous = None
        self._output = output
        self._target = target
        self._check = check
        self._before = previous_files
        self._finished = False
        if previous_files is None or not previous_files:
            return
        if 'index.json' not in previous_files:
            raise ValueError('existing nonempty stdffi directory has no target index')
        self.previous, indexed = _load_index(output, target)
        if full:
            return
        found = {name: digest for name, digest in previous_files.items() if name != 'index.json'}
        if found != indexed:
            raise ValueError('existing stdffi package drift: ' + _difference(indexed, found))
        for name in indexed:
            (stage / name).write_bytes(_read(output / name, MAX_FILE_BYTES))

    def finish(self, index, *, validate, verify, expected_keys=None):
        if self._finished:
            raise ValueError('stdffi transaction already finished')
        found = _snapshot(self.stage) or {}
        expected = _check_index(index, self._target)
        if expected_keys is not None and set(expected_keys) != {r['key'] for r in index['modules']}:
            raise ValueError('stdffi package does not account for exactly the requested manifest keys')
        if found != expected:
            raise ValueError('staged stdffi index mismatch: ' + _difference(expected, found))
        canonical = {**index, 'modules': sorted(index['modules'], key=lambda row: row['key'])}
        index_path = self.stage / 'index.json'
        with index_path.open('xb') as stream:
            size = 1
            for chunk in json.JSONEncoder(ensure_ascii=True, sort_keys=True, allow_nan=False,
                                          separators=(',', ':')).iterencode(canonical):
                size += len(chunk)
                if size > MAX_INDEX_BYTES:
                    raise ValueError('stdffi index byte limit exceeded')
                stream.write(chunk.encode('ascii'))
            stream.write(b'\n')
        staged = _snapshot(self.stage)
        validate(self.stage)
        if _snapshot(self.stage) != staged:
            raise ValueError('stdffi validation modified the staged package')
        if not self._check:
            _sync(self.stage)
        verify()
        current = _snapshot(self._output)
        if current != self._before:
            raise ValueError('published stdffi package changed during generation')
        if self._check:
            if current != staged:
                raise ValueError('stale stdffi package: ' + _difference(staged, current or {}))
        elif current != staged:
            _rename(self.stage, self._output, exchange=current is not None)
        self._finished = True


@contextmanager
def transaction(output, target, *, check=False, full=False):
    """Keep the old catalogue visible until a hash-checked stage is committed."""
    output = _package_path(output, target)
    if not check:
        output.parent.mkdir(parents=True, exist_ok=True)
    elif not output.parent.is_dir():
        raise ValueError(f'missing stdffi package for {target}: {output}')
    parent = os.open(output.parent, os.O_RDONLY | os.O_DIRECTORY)
    try:
        try:
            fcntl.flock(parent, (fcntl.LOCK_SH if check else fcntl.LOCK_EX) | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise ValueError('another stdffi transaction holds the output parent') from error
        before = _snapshot(output)
        with tempfile.TemporaryDirectory(prefix='.stdffi-stage-',
                                         dir=None if check else output.parent) as temp:
            stage = Path(temp) / 'package'
            stage.mkdir()
            package = _Package(stage, output, target, check, full, before)
            yield package
            if not package._finished:
                raise ValueError('stdffi transaction exited without validation and publication')
    finally:
        os.close(parent)
