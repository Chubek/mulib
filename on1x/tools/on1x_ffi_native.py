"""Descriptor-driven MuLIB native linkage. No declaration extraction at build time."""

import hashlib
import importlib.util
import os
from pathlib import Path
import re
import resource
import shutil
import signal
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('mulib_housekeeping', ROOT / 'stdlib/mulib/housekeeping.py')
house = importlib.util.module_from_spec(spec)
spec.loader.exec_module(house)
c = house.contracts

TARGET = ['x86_64-linux-gnu', 'LP64', 'SysVAMD64', 'sysv']
STRICT = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror']
CFLAGS = frozenset(('-O0', '-O1', '-O2', '-O3', '-g', '-fno-omit-frame-pointer',
                   '-fsanitize=address,undefined', '-fno-sanitize-recover=all'))
DSO = 'libon1x_kernels.so'
HEX = re.compile(r'[0-9a-f]{64}')
IDENTIFIER = re.compile(r'[A-Za-z_][A-Za-z_0-9]{0,95}')
SURFACE = dict(i64='int64_t', u8='uint8_t', u16='uint16_t', u32='uint32_t', u64='uint64_t',
               f32='float', f64='double', bool='_Bool', void='void')
NATIVE = {
    'bool': '_Bool', 'char': 'char', 'signed-char': 'signed char', 'unsigned-char': 'unsigned char',
    'short': 'short', 'unsigned-short': 'unsigned short', 'int': 'int', 'unsigned-int': 'unsigned int',
    'long': 'long', 'unsigned-long': 'unsigned long', 'long-long': 'long long',
    'unsigned-long-long': 'unsigned long long', 'float': 'float', 'double': 'double',
}
ENV = {'PATH': '/usr/bin:/bin', 'LC_ALL': 'C', 'TZ': 'UTC', 'PYTHONDONTWRITEBYTECODE': '1'}


def executable(value):
    path = shutil.which(str(value))
    if not path:
        raise ValueError('FFI executable unavailable: ' + str(value))
    return Path(path).resolve(strict=True)


def native_run(argv, cwd, stdin, output, errors, timeout, env):
    def limits():
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        resource.setrlimit(resource.RLIMIT_FSIZE, (c.MAX_BYTES, c.MAX_BYTES))

    with subprocess.Popen(argv, cwd=cwd, env=env, stdin=stdin, stdout=output, stderr=errors,
                          start_new_session=True, preexec_fn=limits) as proc:
        try:
            return proc.wait(timeout)
        except BaseException:
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            proc.wait()
            raise


def capture(argv, stage, *, timeout=60, bound=1024 * 1024, env=ENV):
    with tempfile.TemporaryFile() as output, tempfile.TemporaryFile() as errors:
        result = native_run(argv, stage, subprocess.DEVNULL, output, errors, timeout, env)
        output.seek(0)
        errors.seek(0)
        payload, diagnostics = output.read(bound + 1), errors.read(65536)
    if result:
        raise ValueError(f'FFI tool {Path(argv[0]).name} failed ({result}): '
                         + diagnostics.decode(errors='replace'))
    if len(payload) > bound:
        raise ValueError('FFI tool output limit exceeded')
    return payload


class Inputs:
    def __init__(self):
        self.digest = {}

    def add(self, path, expected=None):
        path = str(Path(path).resolve(strict=True))
        actual = c.digest(path)
        if (expected is not None and actual != expected) or self.digest.setdefault(path, actual) != actual:
            raise ValueError('FFI native input/artifact drift: ' + path)
        return actual

    def verify(self):
        for path, expected in self.digest.items():
            self.add(path, expected)


def validate_type(native, *, result):
    if native == ['void'] and result:
        return 'void'
    if not isinstance(native, list) or len(native) not in (6, 7):
        raise ValueError('FFI unsupported native type')
    kind, identity, *fields = native
    if (identity not in NATIVE or
            (kind == 'integer' and (len(fields) != 5 or fields[2] not in ('signed', 'unsigned')
                                    or identity in ('float', 'double'))) or
            (kind == 'real' and (len(fields) != 4 or (identity, fields[0]) not in
                                 (('float', 'binary32'), ('double', 'binary64')))) or
            kind not in ('integer', 'real')):
        raise ValueError('FFI unsupported native type identity')
    if kind == 'integer' and (type(fields[0]) is not int or not 0 <= fields[0] <= 8):
        raise ValueError('FFI invalid native integer rank')
    sizes = fields[1:2] + fields[3:] if kind == 'integer' else fields[1:]
    if not all(type(value) is int and 0 < value <= 128 for value in sizes):
        raise ValueError('FFI invalid native layout')
    return NATIVE[identity]


def plan(root, document):
    """Validate the on1xc inspection protocol and partition canonical recipes."""
    if set(document) != {'schema', 'requests', 'sources', 'descriptors'} or document['schema'] != 'on1x.ffi-imports/v1':
        raise ValueError('FFI compiler inspection protocol mismatch')
    requests, descriptors = document['requests'], document['descriptors']
    if not 0 < len(requests) <= 1024 or not 0 < len(descriptors) <= 2048:
        raise ValueError('FFI build requires bounded, typed import requests')
    modules = house.inventory(root, registration=True)
    aliases = c.manifest_aliases(modules)
    by_partition = {'static': set(), 'dynlib': set()}
    mode, providers = {}, {}
    for request in requests:
        module = request['module']
        if not isinstance(module, str) or not module.startswith('mulib.') or type(request['unsafe']) is not bool:
            raise ValueError('FFI invalid compiler import request')
        names = aliases.get(module.removeprefix('mulib.'), ())
        if len(names) != 1:
            raise ValueError('FFI unknown or ambiguous MANIFEST module: ' + module)
        partition = request.get('linkage', 'static')
        if partition not in by_partition or mode.setdefault(module, partition) != partition:
            raise ValueError('FFI conflicting static and dynlib imports of the same module')
        providers[module] = names[0]
        by_partition[partition].add(names[0])
    seen_ids, seen_symbols, seen_members, covered = set(), set(), set(), set()
    for d in descriptors:
        if set(d) != {'id', 'module', 'provider', 'header', 'member', 'symbol', 'wrapper', 'target', 'linkage', 'definition',
                      'native_parameters', 'native_result', 'native_fingerprint', 'surface_parameters',
                      'surface_result', 'guards', 'effect'}:
            raise ValueError('FFI descriptor schema mismatch')
        module, key, symbol, linkage = d['module'], d['provider'], d['symbol'], d['linkage']
        if (providers.get(module) != key or d['header'] != Path(modules[key]['header']).name or
                not IDENTIFIER.fullmatch(symbol) or not IDENTIFIER.fullmatch(d['member']) or
                not re.fullmatch(r'on1x_[A-Za-z_0-9]{1,150}', d['wrapper']) or
                not HEX.fullmatch(d['id']) or d['id'] in seen_ids or symbol in seen_symbols or
                (module, d['member']) in seen_members or d['effect'] != 'unknown'):
            raise ValueError('FFI descriptor identity does not match the selected canonical module')
        if d['target'] != TARGET:
            raise ValueError('FFI descriptor target/ABI is unsupported or stale')
        if linkage == 'internal':
            if d['definition'] != 'defined' or mode[module] != 'static' or modules[key].get('implementation') == 'static-data':
                raise ValueError('FFI static-inline placement requires a static native import')
        elif (linkage != 'external' or d['definition'] not in ('declaration', 'defined') or
              not modules[key]['macro']):
            raise ValueError('FFI external symbol has no supported canonical implementation')
        parameters = d['native_parameters']
        surface = d['surface_parameters']
        if (len(parameters) != len(surface) or len(surface) > 16
                or any(kind not in SURFACE or kind == 'void' for kind in surface)
                or d['surface_result'] not in SURFACE):
            raise ValueError('FFI unsupported surface signature')
        for value in parameters:
            validate_type(value, result=False)
        validate_type(d['native_result'], result=True)
        guards = d['guards']
        if (len(guards) > 16 or any(len(pair) != 2 or
                not all(type(i) is int and 0 <= i < len(surface) for i in pair) for pair in guards)):
            raise ValueError('FFI invalid descriptor guard indices')
        wire = ['on1x.ffi-native/v2', d['target'], symbol, linkage, d['definition'], parameters, d['native_result']]
        if hashlib.sha256(c.dumps(wire).strip().encode('ascii')).hexdigest() != d['native_fingerprint']:
            raise ValueError('FFI descriptor native fingerprint mismatch')
        seen_ids.add(d['id'])
        seen_symbols.add(symbol)
        seen_members.add((module, d['member']))
        covered.add(module)
    if covered != providers.keys():
        raise ValueError('FFI import has no compiler-owned typed descriptor: ' + ', '.join(sorted(providers.keys() - covered)))
    # Use the same canonical planner that owns deduplication and companion/link
    # selection. Its discovery mode allows explicitly typed header-only modules
    # to participate; it cannot relax any external implementation restriction.
    recipes = {linkage: house.plan(root, sorted(keys), discovery=True)
               for linkage, keys in by_partition.items() if keys}
    by_key = {d['provider'] for d in descriptors if d['linkage'] == 'internal'}
    for recipe in recipes.values():
        for item in recipe['modules']:
            if house.header_path(root, item) != house.local(root, item['header']):
                raise ValueError('FFI resolved a noncanonical native header')
            if item['name'] in recipe.get('discovery_only', {}) and (item['macro'] or
                    (item['name'] in recipe['libraries'] and item['name'] not in by_key)):
                raise ValueError('FFI unsupported native implementation: ' + item['name'])
        if recipe['dependency_cycles']:
            raise ValueError('FFI implementation dependency cycle')
    if len(recipes) == 2:
        full = house.plan(root, sorted(providers.values()), discovery=True)
        stat, dyn = recipes['static'], recipes['dynlib']
        for field in ('implementation_macros', 'native_sources', 'link_libraries', 'include_headers',
                      'compile_definitions', 'generator_checks'):
            if set(full[field]) != set(stat[field]) | set(dyn[field]):
                raise ValueError('FFI combined closure differs from partitioned closure: ' + field)
        for field in ('implementation_macros', 'native_sources'):
            if set(stat[field]) & set(dyn[field]):
                raise ValueError('FFI static and dynlib definitions overlap: ' + field)
    return recipes


def abi_source(descriptors):
    """An independent native prototype check using literal C primitives."""
    rows = ['#include <limits.h>', '#include <float.h>',
            '#if !defined(__linux__) || !defined(__x86_64__) || !defined(__LP64__)',
            '#error "unsupported FFI target"', '#endif',
            '_Static_assert(CHAR_BIT == 8 && sizeof(void *) == 8 && _Alignof(void *) == 8, "FFI data model");']
    for header in sorted({d['header'] for d in descriptors}):
        rows.append(f'#include <{header}>')
    for index, d in enumerate(descriptors):
        result = validate_type(d['native_result'], result=True)
        params = ', '.join(validate_type(t, result=False) for t in d['native_parameters']) or 'void'
        symbol = d['symbol']
        rows += [f'typedef {result} (*on1x_abi_t_{index})({params});',
                 f'_Static_assert(_Generic(&{symbol}, on1x_abi_t_{index}: 1, default: 0), "FFI prototype: {symbol}");']
        for t in [*d['native_parameters'], d['native_result']]:
            if t != ['void']:
                spelling = validate_type(t, result=True)
                rows.append(f'_Static_assert(sizeof({spelling}) == {t[-2]} && _Alignof({spelling}) == {t[-1]}, '
                            f'"FFI primitive layout: {spelling}");')
                if t[1] == 'char':
                    op = '<' if t[-3] == 'signed' else '=='
                    rows.append(f'_Static_assert(CHAR_MIN {op} 0, "FFI plain char signedness");')
                if t[0] == 'real':
                    radix, mantissa, exp = ('FLT', 24, 128) if t[1] == 'float' else ('DBL', 53, 1024)
                    rows.append(f'_Static_assert(FLT_RADIX == 2 && {radix}_MANT_DIG == {mantissa} && '
                                f'{radix}_MAX_EXP == {exp}, "FFI real format");')
        if d['linkage'] == 'external':
            # Materialize a native relocation even if the application never
            # calls this export. A missing symbol cannot become qualified.
            rows.append(f'on1x_abi_t_{index} volatile on1x_abi_symbol_{index} = &{symbol};')
    return '\n'.join(rows) + '\n'


def probe_source(descriptors):
    """Independent host caller, with oracle values computed without calling C."""
    import random

    def c_int(value, surface):
        if surface == 'u64':
            return f'UINT64_C({value})'
        if value == -(2**63):
            return 'INT64_MIN'
        return f'-INT64_C({-value})' if value < 0 else f'INT64_C({value})'

    rows = ['#include <stdint.h>']
    checks = []
    # Qualification is intentionally narrower than representation eligibility.
    # Do not execute arbitrary extracted calls to learn their side effects.
    lo, hi = -(2**63), 2**63 - 1
    edge = (lo, lo + 1, -(2**32), -1, 0, 1, 2**32, hi - 1, hi)
    clamp = lambda value: max(lo, min(value, hi))
    for d in descriptors:
        symbol, kind = d['symbol'], d['surface_result']
        params = d['surface_parameters']
        name = d['wrapper']
        prng = random.Random(0xf102)
        if (d['module'], symbol, params, kind) in (
                ('mulib.clamp', 'mulib_clamp_add_i64', ['i64'] * 2, 'i64'),
                ('mulib.clamp', 'mulib_clamp_sub_i64', ['i64'] * 2, 'i64')):
            pairs = [(a, b) for a in edge for b in edge]
            pairs.extend((prng.randint(lo, hi), prng.randint(lo, hi)) for _ in range(256))
            answers = [(args, clamp(args[0] - args[1] if symbol.endswith('_sub_i64') else sum(args)))
                       for args in pairs]
        elif (d['module'], symbol, params, kind) == ('mulib.clamp', 'mulib_clamp_i64', ['i64'] * 3, 'i64'):
            points = [(a, b, c_) for a in edge for b in edge for c_ in edge if b <= c_]
            for _ in range(256):
                b, c_ = sorted(prng.randint(lo, hi) for _ in range(2))
                points.append((prng.randint(lo, hi), b, c_))
            answers = [(args, max(args[1], min(args[0], args[2]))) for args in points]
        elif (d['module'], symbol, params, kind) == (
                'mulib.container_core', 'mulib_ct_counter_add', ['u64'] * 2, 'u64'):
            values = (0, 1, 2, 6, 35, 2**32, 2**63, 2**64 - 1)
            pairs = [(a, b) for a in values for b in values]
            pairs.extend((prng.getrandbits(64), prng.getrandbits(64)) for _ in range(256))
            answers = [(args, min(sum(args), 2**64 - 1)) for args in pairs]
        else:
            raise ValueError('FFI no independent executable probe for ' + symbol)
        rows.append(f'{SURFACE[kind]} {name}({", ".join(SURFACE[p] for p in params)});')
        for args, answer in answers:
            values = ', '.join(c_int(value, p) for p, value in zip(params, args))
            checks.append(f'  if ({name}({values}) != {c_int(answer, kind)}) return 1;')
    return '\n'.join([*rows, 'int main(void) {', *checks, '  return 0;', '}', '']), len(checks)


def check_elf(path, kind):
    with path.open('rb') as stream:
        header = stream.read(64)
    if (len(header) != 64 or header[:6] != b'\x7fELF\x02\x01' or
            header[16:20] != bytes((kind, 0, 62, 0))):
        raise ValueError(f'FFI tool did not produce expected AMD64 ELF type {kind}: {path.name}')


class Build:
    def __init__(self, args, frontend, root, stage):
        self.args, self.root, self.stage = args, root, stage
        self.on1xc = executable(frontend[0])
        self.cc = executable(args.cc)
        self.frontend = [self.on1xc, *frontend[1:]]
        self.native_commands = []
        self.resource_trees = {}
        self.inputs = Inputs()
        for path in (self.cc, self.on1xc, __file__, ROOT / 'tools/on1x-build.py',
                     ROOT / 'tools/on1x_ffi_trust.py'):
            self.inputs.add(path)
        request = stage / 'ffi-imports.json'
        self.inspect = [self.on1xc, '--ffi-imports', self.frontend[2], '-o', request, *self.frontend[5:]]
        capture(self.inspect, Path.cwd())
        self.document = c.load(request, 8 * 1024 * 1024)
        self.recipes = plan(root, self.document)
        for entry in self.document['sources']:
            self.inputs.add(os.fsdecode(bytes.fromhex(entry['file_hex'])), entry['sha256'])
        # Application source identity is checked separately from the native review.
        self.source_paths = {str(Path(os.fsdecode(bytes.fromhex(e['file_hex']))).resolve())
                             for e in self.document['sources']}
        self.descriptors = sorted(self.document['descriptors'], key=lambda d: (d['module'], d['member']))
        self.dynamic = 'dynlib' in self.recipes
        self.safe = any(not request['unsafe'] for request in self.document['requests'])
        self.providers = dict(implementation=args.ffi_provider,
                              adapter=args.ffi_adapter_provider or args.ffi_provider)
        self.runtime = ROOT / 'runtime/on1x_runtime.c'
        self.inputs.add(self.runtime)
        self.host_flags = [*STRICT, '-I', str(root / 'include'), *args.cflag,
                           f'-ffile-prefix-map={stage}=/__on1x__/ffi']
        self.definitions = ['-D' + define for define in sorted({d for recipe in self.recipes.values()
                                                              for d in recipe['compile_definitions']})]
        self.cephyr = None
        for recipe in self.recipes.values():
            for path, digest in recipe['inputs'].items():
                self.inputs.add(path, digest)
            for record in recipe['files']:
                self.inputs.add(house.local(root, record['path']), record['sha256'])
        self.host_target = self.host('-dumpmachine').decode().strip()
        if (sys.platform != 'linux' or os.uname().machine != 'x86_64' or
                not re.fullmatch(r'x86_64-(?:[A-Za-z0-9_.]+-)?linux-gnu', self.host_target)):
            raise ValueError('FFI native execution requires AMD64 Linux GNU LP64')
        self.host_version = self.host('--version').decode(errors='replace')
        if 'cephyr' in self.providers.values():
            if args.cflag or self.definitions != ['-D_POSIX_C_SOURCE=200809L']:
                raise ValueError('FFI embedded Cephyr does not support extra C flags or feature definitions')
            self.cephyr = executable(self.on1xc.with_name('on1x-cephyr-kernel'))
            self.inputs.add(self.cephyr)
            config, cephyr, archon = map(Path, capture([self.cephyr, '--toolchain-inputs'], stage).decode().splitlines())
            self.inputs.add(config)
            # Conservative resource inventory until the embedding API returns
            # an exact dependency trace; host preprocessing is not that trace.
            for directory in (cephyr / 'stdlib/include', cephyr / 'triples',
                              archon / 'stdspec', root / 'include'):
                self.pin_tree(directory)

    def host(self, *argv):
        return capture([self.cc, *argv], self.stage, timeout=120, bound=8 * 1024 * 1024)

    def pin_tree(self, path):
        expected = {}
        for candidate in path.rglob('*'):
            if candidate.is_symlink():
                raise ValueError('FFI unsupported symlink in Cephyr resource inventory: ' + str(candidate))
            if candidate.is_file():
                if len(expected) >= 8192:
                    raise ValueError('FFI Cephyr resource file limit exceeded')
                expected[str(candidate)] = self.inputs.add(candidate)
        if not expected:
            raise ValueError('FFI missing Cephyr resources: ' + str(path))
        self.resource_trees[path] = set(expected)

    def record_command(self, argv):
        self.native_commands.append([str(token).replace(str(self.stage), '<ffi-stage>') for token in argv])

    def loaded_libraries(self, executable):
        output = capture([executable], self.stage, env=ENV | {'LD_TRACE_LOADED_OBJECTS': '1'})
        identities = {}
        for line in os.fsdecode(output).splitlines():
            match = re.fullmatch(r'\s*(?:(.*?) => )?(/.+) \(0x[0-9a-f]+\)', line)
            if match:
                name, path = match.groups()
                identities[name or Path(path).name] = Path(path).resolve(strict=True)
                self.inputs.add(path)
            elif 'not found' in line:
                raise ValueError('FFI loader dependency missing: ' + line.strip())
        return identities

    def deps(self, source, flags):
        data = self.host(*flags, '-M', '-MT', 'on1x', source)
        # The shared parser resolves names relative to its process cwd. GCC
        # emits its input using the operand spelling; pass an absolute source.
        for path in house.dep_paths(data):
            self.inputs.add(path)

    def compile(self, provider, source, output, *flags):
        src = self.stage / source
        self.inputs.add(src)
        if provider == 'host':
            self.deps(src, flags)
            argv = [*flags, '-c', source, '-o', output]
            self.record_command([self.cc, *argv])
            self.host(*argv)
        else:
            argv = [self.cephyr, '--object', self.root / 'include', self.runtime.parent]
            self.record_command([*argv, f'<stdin:{source}>', f'<stdout:{output}>'])
            with src.open('rb') as stream, (self.stage / output).open('xb') as artifact, \
                    tempfile.TemporaryFile() as errors:
                code = native_run(argv, self.stage, stream, artifact, errors, 120, ENV)
                errors.seek(0)
                diagnostic = errors.read(65536)
            if code:
                raise ValueError('FFI embedded Cephyr object compilation failed: '
                                 + diagnostic.decode(errors='replace'))
        check_elf(self.stage / output, 1)
        self.inputs.add(self.stage / output)
        return output

    def report(self):
        return dict(schema='on1x.ffi-plan/v1', target=TARGET, providers=self.providers,
                    compiler=dict(path=str(self.cc), target=self.host_target, version=self.host_version),
                    descriptors=self.descriptors, partitions=self.recipes, safe_export=False,
                    cflags=self.args.cflag, admission='not_qualified')

    def pin_host_tools(self):
        # Pin the actual GCC/Clang-selected C compiler backend and link inputs,
        # not just the executable selected by --cc.
        for name in ('as', 'ld', 'cc1'):
            path = self.host('-print-prog-name=' + name).decode().strip()
            resolved = shutil.which(path, path=ENV['PATH'])
            if resolved:
                self.inputs.add(resolved)
            elif name != 'cc1':
                raise ValueError('FFI host tool unavailable: ' + name)
        libraries = {lib for recipe in self.recipes.values() for lib in recipe['link_libraries']}
        for name in ('crt1.o', 'Scrt1.o', 'crti.o', 'crtn.o', 'crtbegin.o', 'crtbeginS.o', 'crtend.o',
                     'crtendS.o', 'libc.so', 'libc.so.6', 'libgcc.a', 'libgcc_s.so', 'libgcc_s.so.1',
                     'ld-linux-x86-64.so.2', 'libm.so', 'libm.so.6',
                     *sorted('lib' + name + suffix for name in libraries for suffix in ('.so', '.a'))):
            path = Path(self.host('-print-file-name=' + name).decode().strip())
            if path.is_absolute() and path.is_file():
                self.inputs.add(path)

    def prepare(self):
        self.pin_host_tools()
        if self.cephyr:
            loaded = self.loaded_libraries(self.cephyr)
            if not any(name.startswith('libcephyr.so') for name in loaded):
                raise ValueError('FFI embedded Cephyr shared compiler identity is unavailable')
        flags = [*self.host_flags, *self.definitions]
        abi = self.stage / 'native-abi.c'
        abi.write_text(abi_source(self.descriptors), encoding='ascii')
        self.compile('host', abi.name, 'native-abi.o', *flags)
        links = ['native-abi.o']
        for index, d in enumerate(self.descriptors):
            adapter = self.stage / f'ffi-adapter-{index}.c'
            capture([self.on1xc, '--ffi-adapter', '--ffi-id', d['id'], self.frontend[2], '-o', adapter,
                     *self.frontend[5:]], Path.cwd())
            links.append(self.compile(self.providers['adapter'], adapter.name, adapter.with_suffix('.o').name,
                                      *flags, '-I', str(self.runtime.parent), '-DON1X_KERNEL_ABI_V1=1'))
        for partition, recipe in sorted(self.recipes.items()):
            house.check_generated(self.root, recipe)
            if partition == 'dynlib' and self.providers['implementation'] != 'host':
                raise ValueError('FFI embedded Cephyr DSO provider is not qualified; select host explicitly')
            if (self.providers['implementation'] == 'cephyr' and
                    any(recipe[key] for key in ('native_sources', 'include_headers', 'link_libraries'))):
                raise ValueError('FFI embedded Cephyr has no qualified companion/external-library recipe')
            if not any(recipe[key] for key in ('implementation_macros', 'include_headers', 'native_sources')):
                continue  # static-inline definition is local to the adapter
            source = self.stage / ('mulib-' + partition + '.c')
            source.write_text(house.implementation(self.root, recipe), encoding='ascii')
            sources = [source, *[house.local(self.root, path) for path in recipe['native_sources']]]
            libs = ['-l' + lib for lib in recipe['link_libraries']]
            if partition == 'dynlib':
                for path in sources:
                    self.inputs.add(path)
                    self.deps(path, [*flags, '-fPIC'])
                argv = [*flags, '-fPIC', '-shared', '-Wl,-z,defs', '-Wl,-soname,' + DSO, *sources, *libs, '-o', DSO]
                self.record_command([self.cc, *argv])
                self.host(*argv)
                check_elf(self.stage / DSO, 3)
                self.inputs.add(self.stage / DSO)
                links += ['-L.', '-Wl,-rpath,$ORIGIN', '-Wl,-z,now', '-Wl,--push-state,--no-as-needed',
                          '-l:' + DSO, '-Wl,--pop-state']
            else:
                links.append(self.compile(self.providers['implementation'], source.name, 'mulib-static.o', *flags))
                for index, native in enumerate(sources[1:]):
                    self.inputs.add(native)
                    self.deps(native, flags)
                    obj = f'companion-{index}.o'
                    argv = [*flags, '-c', native, '-o', obj]
                    self.record_command([self.cc, *argv])
                    self.host(*argv)
                    check_elf(self.stage / obj, 1)
                    self.inputs.add(self.stage / obj)
                    links.append(obj)
                links += libs
        return links

    def subject(self, checks):
        stable, artifacts = {}, {}
        for path, sha256 in self.inputs.digest.items():
            p = Path(path)
            if p.is_relative_to(self.stage):
                artifacts[p.relative_to(self.stage).as_posix()] = sha256
            elif path not in self.source_paths:
                stable[path] = sha256
            else:
                # Never exclude an input from review merely because an
                # application happened to name it as a source input.
                if p.suffix != '.on1x':
                    raise ValueError('FFI overlap between reviewed and application inputs: ' + path)
        return dict(schema='on1x.ffi-review-subject/v1', target=TARGET, providers=self.providers,
                    compiler=dict(path=str(self.cc), target=self.host_target, version=self.host_version),
                    compiler_flags=[f.replace(str(self.stage), '<ffi-stage>') for f in self.host_flags],
                    feature_definitions=self.definitions, descriptors=self.descriptors,
                    native_commands=self.native_commands, environment=ENV,
                    partitions=self.recipes, assumptions=dict(
                        effects='unknown', proof='numeric-probe-only; authority reviews foreign effects and semantics',
                        guard='caller evaluates args once, checks descriptor guards before crossing the boundary'),
                    probe_comparisons=checks, inputs=stable, artifacts=artifacts)

    def check_sources(self):
        capture(self.inspect, Path.cwd())
        if c.dumps(c.load(self.stage / 'ffi-imports.json', 8 * 1024 * 1024)) != c.dumps(self.document):
            raise ValueError('FFI compiler source graph/descriptor drift')
        for root, expected in self.resource_trees.items():
            if {str(p) for p in root.rglob('*') if p.is_file() or p.is_symlink()} != expected:
                raise ValueError('FFI Cephyr resource inventory drift: ' + str(root))
        self.inputs.verify()

    def publish(self, output, paths):
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.on1x-ffi-', dir=output.parent) as temp:
            bundle = Path(temp) / 'out'
            if len(paths) == 1:
                shutil.copy2(self.stage / paths[0], bundle)
                if c.digest(bundle) != c.digest(self.stage / paths[0]):
                    raise ValueError('FFI artifact drift during publication')
            else:
                bundle.mkdir()
                for name in paths:
                    shutil.copy2(self.stage / name, bundle / name)
                    if c.digest(bundle / name) != c.digest(self.stage / name):
                        raise ValueError('FFI artifact drift during publication: ' + name)
            self.inputs.verify()
            c.publish_new(bundle, output)

    def run(self):
        args, stage = self.args, self.stage
        if args.ffi_static_only and self.dynamic:
            raise ValueError('FFI project profile currently accepts static imports only')
        if args.inspect_ffi:
            (stage / 'ffi-plan.json').write_text(c.dumps(self.report()), encoding='ascii')
            self.check_sources()
            return self.publish(args.output, ['ffi-plan.json'])
        if self.safe and not (args.ffi_review_candidate or args.ffi_review):
            raise ValueError('ON1X-FFI-NOT-ADMITTED: safe import requires independent signed review')
        if not self.safe and (args.ffi_review_candidate or args.ffi_review):
            raise ValueError('FFI review requires at least one safe import')
        if self.safe and self.dynamic:
            raise ValueError('ON1X-FFI-NOT-ADMITTED: generated safe DSO load policy is not qualified')
        probe, checks = probe_source(self.descriptors)
        (stage / 'ffi-probe.c').write_text(probe, encoding='ascii')
        self.inputs.add(stage / 'ffi-probe.c')
        links = self.prepare()
        self.deps(stage / 'ffi-probe.c', self.host_flags)
        self.deps(self.runtime, self.host_flags)
        argv = [*self.host_flags, 'ffi-probe.c', self.runtime, *links, '-o', 'ffi-probe']
        self.record_command([self.cc, *argv])
        self.host(*argv)
        self.inputs.add(stage / 'ffi-probe')
        loaded = self.loaded_libraries(stage / 'ffi-probe')
        if self.dynamic and loaded.get(DSO) != stage / DSO:
            raise ValueError('FFI probe did not resolve the exact staged DSO')
        subject = self.subject(checks)
        review = None
        if args.ffi_review:
            from on1x_ffi_trust import authorize
            review = authorize(subject, schema='on1x.ffi-review/v1', key=args.ffi_review_key,
                               review=args.ffi_review, signature=args.ffi_review_signature, stage=stage,
                               codec=c, diagnostic='ON1X-FFI-NOT-ADMITTED: ')
            self.inputs.add(review['verifier'], review['verifier_sha256'])
        capture([stage / 'ffi-probe'], stage, timeout=30)
        self.check_sources()
        if args.ffi_review_candidate:
            (stage / 'review-subject.json').write_text(c.dumps(subject), encoding='ascii')
            return self.publish(args.output, ['review-subject.json'])
        capture([*self.frontend, '--kernel-c11', '--ffi-stage'], Path.cwd())
        source = stage / 'main.c'
        flags = [*self.host_flags, '-I', str(self.runtime.parent), '-DON1X_KERNEL_ABI_V1=1']
        companions, libs = [], []
        text = c.read(source).decode()
        if re.search(r'\bextern\s+[^;\n]*\bbasis_native_\w+\(', text):
            for name in ('runtime/basis_native.c', 'runtime/basis_real.c'):
                path = Path(os.fsdecode(capture([self.on1xc, '--stdlib-resource', name,
                                                *self.frontend[5:]], Path.cwd()).strip()))
                companions.append(path)
                self.deps(path, flags)
            libs.append('-lm')
        self.deps(source, flags)
        self.deps(self.runtime, flags)
        self.host(*flags, source.name, self.runtime, *companions, *links, *libs, '-o', 'program')
        self.inputs.add(stage / 'program')
        self.check_sources()
        if not (review or self.dynamic):
            return self.publish(args.output, ['program'])
        outputs = ['program', DSO] if self.dynamic else ['program', 'review.ed25519.der', 'review.json', 'review.sig',
                    'ffi-probe.c', 'ffi-probe', 'native-abi.c', 'native-abi.o']
        if review:
            outputs += [p.name for p in sorted(stage.glob('ffi-adapter-*'))]
            outputs += [p.name for p in sorted(stage.glob('mulib-static.*'))]
            outputs += [p.name for p in sorted(stage.glob('companion-*.o'))]
        report = dict(self.report(), safe_export=bool(review), admission='reviewed' if review else 'unsafe',
                      probe=dict(comparisons=checks, passed=True),
                      artifacts={name: c.digest(stage / name) for name in outputs})
        if review:
            report['review'] = review
        (stage / 'admission.json').write_text(c.dumps(report), encoding='ascii')
        return self.publish(args.output, [*outputs, 'admission.json'])
