"""Canonical declaration inventory backed by the on1x-mulib planner."""
from dataclasses import dataclass
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import threading
from types import MappingProxyType


_ON1X = Path(__file__).resolve().parents[3]
_TOOL_ROOT = _ON1X.parent
_PLANNER = _ON1X / 'stdlib/mulib/housekeeping.py'
_spec = importlib.util.spec_from_file_location('on1x_ffi_housekeeping', _PLANNER)
_planner = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_planner)
_contracts = _planner.contracts

MAX_MODULES = 4096
MAX_INCLUDE_NODES = 16384
MAX_INCLUDE_BYTES = 128 * 1024 * 1024


@dataclass(frozen=True)
class Entry:
    key: str
    module: str
    filename: str
    header: str
    header_path: str
    implementation: str
    reserved_prefix: str


@dataclass(frozen=True)
class Plan:
    source: bytes
    include_dirs: tuple[Path, ...]
    defines: tuple[str, ...]
    metadata: dict


def _digest(data):
    return hashlib.sha256(data).hexdigest()


def _include_snapshot(root):
    """Hash the complete canonical root, including unregistered detail includes."""
    if root.is_symlink() or not root.is_dir():
        raise ValueError('expected a real canonical MuLIB include directory')
    files = {}
    nodes = total = 0
    pending = [root]
    while pending:
        directory = pending.pop()
        with os.scandir(directory) as children:
            for child in children:
                nodes += 1
                if nodes > MAX_INCLUDE_NODES:
                    raise ValueError('MuLIB include tree node budget exceeded')
                path = Path(child.path)
                if child.is_symlink():
                    raise ValueError('symlink in canonical MuLIB include tree: '
                                     + path.relative_to(root).as_posix())
                if child.is_dir(follow_symlinks=False):
                    pending.append(path)
                    continue
                if not child.is_file(follow_symlinks=False):
                    raise ValueError('not a regular canonical MuLIB include input: '
                                     + path.relative_to(root).as_posix())
                data = _contracts.read(path, MAX_INCLUDE_BYTES - total)
                total += len(data)
                files[path.relative_to(root).as_posix()] = _digest(data)
    return dict(sorted(files.items()))


def _tree_metadata(path, files):
    return {
        'path': path,
        'method': 'complete_regular_file_inventory',
        'sha256': _digest(json.dumps(files, separators=(',', ':'),
                                     ensure_ascii=True).encode('ascii')),
        'files': dict(files),
    }


class Inventory:
    def __init__(self, root):
        self.root = Path(root).resolve(strict=True)
        self._include = self.root / 'include'
        self._inputs = {}
        self._input_paths = {}
        self._fixture_files = {}
        self._lock = threading.Lock()
        manifest = _contracts.read(_contracts.local(self.root, 'MANIFEST.yaml'), 8 * 1024 * 1024)
        self._remember(self.root / 'MANIFEST.yaml', _digest(manifest))
        for path in (_PLANNER, _planner.CATALOG, Path(_contracts.__file__),
                     _contracts.EXOTRACT_CATALOG, Path(__file__).resolve()):
            self._remember(path, _contracts.digest(path))
        self._headers = _include_snapshot(self._include)
        # Registered OCI compatibility headers traverse to ../.deprecated.
        # Hash the archived tree without adding it to Cephyr's include search.
        self._archived = self._archive_snapshot()
        reserved = set()
        self._modules = _planner.inventory(self.root, raw=manifest, registration=True,
                                           reservations=reserved)
        if not 0 < len(self._modules) <= MAX_MODULES:
            raise ValueError('MuLIB inventory module budget exceeded')
        aliases = _contracts.manifest_aliases(self._modules)
        for alias, keys in sorted(aliases.items()):
            if len(keys) != 1:
                raise ValueError('canonical MuLIB alias collision: ' + alias + ': '
                                 + ', '.join(sorted(keys)))
            if not re.fullmatch(r'[a-z_][a-z0-9_]{0,95}', alias):
                raise ValueError('canonical MuLIB alias exceeds identifier bounds: ' + alias)
        self.entries = tuple(self._entry(key) for key in sorted(self._modules))
        self.by_key = MappingProxyType({entry.key: entry for entry in self.entries})
        self._profiles = _contracts.exotract_catalog()
        self._fixtures = _planner.catalog(self.root, self._modules)['fixtures']
        # When the real planner supplies a prepared context, let it avoid
        # re-reading the entire manifest and profile catalogs for every key.
        prepare = getattr(_planner, 'prepare_plan', None)
        self._prepared = None if prepare is None else prepare(self.root)
        self.index_metadata = {
            'manifest': {'path': 'MANIFEST.yaml', 'sha256': _digest(manifest)},
            'registered_modules': len(self.entries),
            'reserved_headers': sorted(reserved),
            'mulib_include_tree': _tree_metadata('include', self._headers),
            'relative_include_roots': [] if self._archived is None else [
                _tree_metadata('.deprecated', self._archived)],
            'planner_inputs': self._stable_inputs(self._inputs),
        }

    def _archive_snapshot(self):
        directory = self.root / '.deprecated'
        return _include_snapshot(directory) if directory.exists() or directory.is_symlink() else None

    def _entry(self, key):
        item = self._modules[key]
        header = Path(item['header']).name
        alias = _contracts.manifest_alias(key)
        if header not in self._headers:
            raise ValueError('canonical MuLIB header is not in the include snapshot: ' + key)
        return Entry(key, 'mulib.' + alias, key + '.ffi', header, item['header'],
                     item.get('implementation', 'switch'), 'mulib_' + alias + '_')

    def _remember(self, path, digest):
        resolved = path.resolve(strict=True)
        with self._lock:
            if (self._inputs.setdefault(path, digest) != digest
                    or self._input_paths.setdefault(path, resolved) != resolved):
                raise ValueError('MuLIB planner input drift: ' + self._relative(path))

    def _relative(self, path):
        if path.is_relative_to(_ON1X):
            return 'on1x-toolchain/' + path.relative_to(_TOOL_ROOT).as_posix()
        if path.is_relative_to(self.root):
            return 'mulib/' + path.relative_to(self.root).as_posix()
        raise ValueError('MuLIB planner selected an input outside declared roots')

    def _stable_inputs(self, inputs):
        return dict(sorted((self._relative(path), digest) for path, digest in inputs.items()))

    def plan(self, key):
        entry = self.by_key[key]
        declaration = _planner.declaration_plan(self.root, key, self._modules, ())
        kwargs = {} if self._prepared is None else {'prepared': self._prepared}
        native = _planner.plan(self.root, [key], discovery=True, **kwargs)
        dirs = tuple(declaration['include_directories'])
        if (dirs != (self._include,) or tuple(map(Path, native['include_directories'])) != dirs
                or declaration['implementation_macros']):
            raise ValueError('noncanonical MuLIB declaration environment: ' + key)
        defines = tuple(native['compile_definitions'])
        if any(not re.fullmatch(r'(?!MULIB_IMPL_)[A-Za-z_][A-Za-z0-9_]*(?:=[A-Za-z0-9_]+)?', d)
               for d in defines):
            raise ValueError('unsupported MuLIB declaration feature definition: ' + key)

        closure = {}
        for item in declaration['modules']:
            path = item['header']
            digest = self._headers[Path(path).relative_to('include').as_posix()]
            if digest != item['header_sha256']:
                raise ValueError('MuLIB header drift: ' + path)
            closure[path] = digest

        native_metadata = self._native_metadata(key, native)
        default_fixtures = {group['name'] for group in native['fixtures']}
        fixtures = []
        for name, group in sorted(self._fixtures.items()):
            if key not in group['kernels']:
                continue
            record = {
                'name': name,
                'kind': group['kind'],
                'selected_by_default': name in default_fixtures,
            }
            # Explicit opt-in providers are alternate native recipes, not
            # headers to include in the declaration-only TU.
            if group['kind'] == 'provider':
                alternate = _planner.plan(self.root, [key], [name], discovery=True, **kwargs)
                record['native_plan'] = self._native_metadata(key, alternate)
            fixtures.append(record)
        metadata = {
            'header_sha256': closure[entry.header_path],
            'owned_headers': [entry.header_path],
            'declared_dependency_headers': closure,
            'include_dirs': ['include'],
            'defines': list(defines),
            'implementation': entry.implementation,
            'fixture_associations': fixtures,
            'native_plan': native_metadata,
        }
        return Plan(f'#include <{entry.header}>\n'.encode('ascii'), dirs, defines, metadata)

    def _native_metadata(self, key, native):
        inputs = {Path(path): digest for path, digest in native['inputs'].items()}
        inputs.update((self.root / record['path'], record['sha256']) for record in native['files'])
        native_headers = {}
        for item in native['modules']:
            if Path(item['header_path']) != self.root / item['header']:
                raise ValueError('noncanonical native MuLIB header: ' + key)
            digest = self._headers[Path(item['header']).relative_to('include').as_posix()]
            if digest != item['header_sha256']:
                raise ValueError('MuLIB native header drift: ' + item['header'])
            native_headers[item['header']] = digest
        for path, digest in inputs.items():
            self._remember(path, digest)
        with self._lock:
            for group in native['fixtures']:
                name, files = group['name'], tuple(group['files'])
                if self._fixture_files.setdefault(name, files) != files:
                    raise ValueError('MuLIB fixture inventory drift: ' + name)
        profile = self._profiles['profiles'].get(key, {})
        recipe = profile.get('recipe', self._profiles['default_recipe'])
        if recipe == 'exoml-core':
            for item in native['modules']:
                self._remember(self.root / item['manual'], item['manual_sha256'])
        return {
            'recipe': recipe,
            'headers': native_headers,
            'implementation_macros': native['implementation_macros'],
            'implementation_translation_units': native['implementation_translation_units'],
            'required_symbols': native['required_symbols'],
            'dependency_cycles': native['dependency_cycles'],
            'fixtures': native['fixtures'],
            'native_sources': native['native_sources'],
            'include_headers': native['include_headers'],
            'generator_checks': native['generator_checks'],
            'link_libraries': native['link_libraries'],
            'archive_discovery_only': native.get('discovery_only', {}),
            # Header digests are already recorded above; include external
            # recipe/config inputs without computing an absolute-path hash.
            'other_inputs': self._stable_inputs(
                {path: digest for path, digest in inputs.items()
                 if not path.is_relative_to(self._include)}),
        }

    def verify_unchanged(self):
        with self._lock:
            inputs = [(path, self._input_paths[path], digest) for path, digest in self._inputs.items()]
            fixtures = dict(self._fixture_files)
        for path, resolved, digest in inputs:
            if not path.is_file() or path.resolve() != resolved or _contracts.digest(path) != digest:
                raise ValueError('MuLIB planner input drift: ' + self._relative(path))
        for name, files in fixtures.items():
            if tuple(_planner.fixture_files(self.root, self._fixtures[name])) != files:
                raise ValueError('MuLIB fixture inventory drift: ' + name)
        if _include_snapshot(self._include) != self._headers or self._archive_snapshot() != self._archived:
            raise ValueError('MuLIB include tree changed during generation')


def load(mulib_root):
    return Inventory(mulib_root)
