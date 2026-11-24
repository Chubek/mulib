"""Shared MuLIB kernel/fixture planning. No C declaration parsing or ABI guesses."""
from collections import namedtuple
from copy import deepcopy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys

PACKAGE = Path(__file__).resolve().parent
HOME = PACKAGE.parents[1]
CATALOG = PACKAGE/'catalog.json'
spec = importlib.util.spec_from_file_location('on1x_mulib_contracts', HOME/'tools/on1x-contracts.py')
contracts = importlib.util.module_from_spec(spec)
spec.loader.exec_module(contracts)
MAX_FILES = 8192
MAX_BYTES = 64*1024*1024
_PlanInputs = namedtuple('_PlanInputs', 'root digests modules config profiles')


def search_paths(values=(), environment='ON1X_MULIB_PATH'):
    """Ordered, bounded colon lists; empty entries never select CWD."""
    result = []
    for value in [*values, os.environ.get(environment, '')]:
        if len(os.fsencode(value))>65536:raise ValueError('search path byte limit exceeded')
        for entry in value.split(':'):
            if not entry:continue
            if len(os.fsencode(entry))>4096 or len(result)>=256:raise ValueError('search path count/entry limit exceeded')
            path = Path(entry).resolve()
            if path not in result:result.append(path)
    return result


def kernel_root(explicit, default, paths):
    if explicit is not None:return explicit.resolve()
    if os.environ.get('ON1X_MULIB_ROOT'):return Path(os.environ['ON1X_MULIB_ROOT']).resolve()
    for path in paths:
        for root in [path, path.parent]:
            if (root/'MANIFEST.yaml').is_file():return root.resolve()
    return default.resolve()


def include_paths(root, paths):
    result = []
    for path in [*paths, root/'kernels']:
        directory = path/'kernels' if (path/'kernels').is_dir() else path
        if directory not in result:result.append(directory)
    return result


def header_path(root, module):
    return Path(module['header_path']) if 'header_path' in module else local(root, module['header'])


local = contracts.local
inventory = contracts.manifest_inventory


def catalog(root, modules):
    data = contracts.load(CATALOG)
    contracts.keys(data, ['schema', 'kernel_links', 'fixtures'])
    if data['schema'] != 'on1x.mulib-fixtures/v1':
        raise ValueError('unsupported MuLIB fixture catalog')
    for kernel, links in data['kernel_links'].items():
        if kernel not in modules or any(not re.fullmatch('[A-Za-z0-9_]+', link) for link in links):
            raise ValueError('invalid kernel link requirement: '+kernel)
    for name, group in data['fixtures'].items():
        contracts.keys(group, ['kernels', 'kind', 'files', 'checks', 'requires', 'native_sources', 'include_headers', 'implementation_macros'])
        if not re.fullmatch('[a-z0-9][a-z0-9-]*', name) or not group['kernels'] or any(n not in modules for n in [*group['kernels'], *group.get('requires', [])]):
            raise ValueError('invalid fixture/kernel association: '+name)
        if group['kind'] not in ('runtime', 'profiles', 'embedded-source', 'amalgamated-source', 'native', 'provider', 'test-data', 'empty'):
            raise ValueError('unknown fixture kind: '+name)
        for pattern in group['files']:
            if not pattern.startswith('library/'+name+'/') or '..' in Path(pattern).parts:
                raise ValueError('invalid fixture pattern: '+pattern)
        for field in ['native_sources', 'include_headers']:
            for path in group.get(field, []):
                if not path.startswith('library/'+name+'/'):
                    raise ValueError('nonlocal fixture compilation input')
        for macro in group.get('implementation_macros', []):
            if not re.fullmatch('MULIB_IMPL_[A-Z0-9_]+', macro):
                raise ValueError('invalid fixture implementation macro')
        for check in group.get('checks', []):
            if not check.startswith('tools/') or not check.endswith('.py'):
                raise ValueError('invalid fixture generator')
    return data


def fixture_files(root, group):
    files = set()
    for pattern in group['files']:
        matches = sorted(root.glob(pattern))
        if not matches:
            raise ValueError('missing fixture files: '+pattern)
        for path in matches:
            name = path.relative_to(root).as_posix()
            local(root, name)
            files.add(name)
            if len(files)>MAX_FILES:
                raise ValueError('fixture file count limit')
    return sorted(files)


def prepare_plan(root):
    """Prepare common canonical inputs once for a batch of independent plans."""
    root = root.resolve()
    digests = {str(path):contracts.digest(path) for path in
               [root/'MANIFEST.yaml', CATALOG, Path(__file__), HOME/'tools/on1x-contracts.py',
                HOME/'contracts-exotract/catalog.json']}
    modules = inventory(root)
    config = catalog(root, modules)
    profiles = contracts.exotract_catalog()
    for path, digest in digests.items():
        if contracts.digest(path)!=digest:raise ValueError('planning input drift: '+path)
    return _PlanInputs(root, digests, modules, config, profiles)


def plan(root, libraries, fixtures=(), paths=(), *, discovery=False, prepared=None):
    root = root.resolve()
    common = prepare_plan(root) if prepared is None else prepared
    if common.root != root:raise ValueError('prepared MuLIB planner root mismatch')
    modules, config, profiles = common.modules, common.config, common.profiles
    inputs = dict(common.digests)
    if len(libraries)!=len(set(libraries)) or any(n not in modules for n in libraries):
        raise ValueError('select unique known MuLIB modules')
    if len(fixtures)!=len(set(fixtures)) or any(n not in config['fixtures'] for n in fixtures):
        raise ValueError('select unique known MuLIB fixtures')
    ordered, expanded, active, cycles, extra, links = [], set(), [], [], [], set()
    def add(name):
        if name not in ordered:
            ordered.append(name)
    def visit(name):
        if name in active:
            cycle = active[active.index(name):]+[name]
            if cycle not in cycles: cycles.append(cycle)
            return
        if name in expanded:return
        if len(active)>=128:raise ValueError('dependency depth limit')
        active.append(name)
        for dep in modules[name]['manifest_dependencies']:visit(dep)
        active.pop(); expanded.add(name); add(name)
        extra.extend(modules[name]['extra_macros'])
    required = {}
    def select(name):
        profile = profiles['profiles'].get(name, {})
        required[name] = profile.get('required_symbols', [])
        recipe = profile.get('recipe', profiles['default_recipe'])
        if recipe=='exoml-core':
            # A kernel plan depends on the selected contract, not the source
            # pins of every unrelated catalog entry.
            core = contracts.catalog(root, selected={name})['profiles'][name]
            for path in ('catalog.json', 'obligations.json', core['contract']):
                path = contracts.CONTRACTS/path
                inputs[str(path)] = contracts.digest(path)
            for dep in core['implementation_modules']:add(dep)
            extra.extend(core.get('active_optional_macros', []))
            for path, expected in core.get('evidence_sha256', {}).items():
                inputs[str(local(root, path))] = expected
            links.update(core['link_libraries'])
        elif recipe=='manifest':visit(name)
        else:raise ValueError('unknown kernel recipe')
        links.update(profile.get('link_libraries', []))
    for name in libraries:select(name)
    selected = set(fixtures)
    for name in fixtures:
        group = config['fixtures'][name]
        if group['kind']=='empty':raise ValueError('fixture directory is empty: '+name)
        # A multi-kernel association is not a demand to enable every consumer.
        # Explicit fixture selection is not an escape from a source-pinned
        # implementation recipe or its required-symbol/evidence obligations.
        select(group['kernels'][0])
    while True:
        previous = (len(ordered), len(selected))
        for name, group in config['fixtures'].items():
            if set(group['kernels']) & set(ordered) and group['kind'] not in ('empty', 'test-data', 'provider'):
                selected.add(name)
        for name in sorted(selected):
            for dep in config['fixtures'][name].get('requires', []):visit(dep)
        if previous==(len(ordered), len(selected)):break
    discovery_only = {name: reason for name in ordered
                      if (reason := contracts.module_discovery_only_reason(name, modules[name], profiles))}
    # Internal-linkage dependencies supply definitions to their consumer's TU;
    # they do not require (or acquire) an external callable archive ABI. Keep
    # rejecting explicit requests for those kernels and unsupported external
    # dependencies, while allowing ordinary switched kernels to include them.
    requested = set(libraries) | {config['fixtures'][name]['kernels'][0] for name in fixtures}
    blocked = [name for name in discovery_only
               if name in requested or modules[name]['macro'] is not None]
    if blocked and not discovery:
        name = blocked[0]
        raise ValueError('discovery-only MuLIB kernel '+name+': '+discovery_only[name])
    directories=include_paths(root,paths)
    entries=[]
    for n in ordered:
        item=modules[n]
        header=next((p/Path(item['header']).name for p in directories if (p/Path(item['header']).name).is_file()), None)
        if header is None:raise ValueError('MuLIB kernel not found in search paths: '+n)
        header=header.resolve()
        digest=contracts.digest(header)
        # Core source-contract recipes remain bound to their canonical header.
        if profiles['profiles'].get(n, {}).get('recipe')=='exoml-core' and digest!=contracts.digest(local(root,item['header'])):
            raise ValueError('source-pinned MuLIB kernel differs in search path: '+n)
        entries.append(dict(name=n, **item, header_path=str(header), header_sha256=digest,
                            manual_sha256=contracts.digest(local(root,item['manual']))))
        inputs[str(header)]=digest
    macros = [m['macro'] for m in entries if m['macro'] is not None]
    if len(macros)!=len(set(macros)):raise ValueError('duplicate kernel implementation switch')
    groups, files, sources, headers, checks = [], {}, [], [], set()
    for name in sorted(selected):
        group = config['fixtures'][name]
        paths = fixture_files(root, group)
        if not set([*group.get('native_sources', []), *group.get('include_headers', [])])<=set(paths):
            raise ValueError('fixture compilation inputs must be inventoried: '+name)
        groups.append(dict(name=name, kind=group['kind'], kernels=group['kernels'], files=paths))
        for path in paths:files[path] = name
        sources.extend(group.get('native_sources', []))
        headers.extend(group.get('include_headers', []))
        extra.extend(group.get('implementation_macros', []))
        checks.update(group.get('checks', []))
    if len(files)>MAX_FILES:raise ValueError('fixture file count limit')
    size = 0; records = []
    for path, group in sorted(files.items()):
        data = contracts.read(local(root, path));size += len(data)
        if size>MAX_BYTES:raise ValueError('fixture byte limit')
        records.append(dict(path=path, group=group, bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
    for macro in extra:
        if macro not in macros:macros.append(macro)
    for name in ordered:
        # Dependencies need their native link requirements just as explicitly
        # selected kernels do (for example, a consumer of prettyemit needs dl).
        links.update(config['kernel_links'].get(name, []))
        links.update(profiles['profiles'].get(name, {}).get('link_libraries', []))
    for check in checks:
        path=local(root,check);inputs[str(path)]=contracts.digest(path)
    for path,digest in inputs.items():
        if contracts.digest(path)!=digest:raise ValueError('planning input drift: '+path)
    # Linux declarations (unshare, O_PATH, pipe2, etc.) must be visible
    # before the TU's first include.
    definitions = ['_POSIX_C_SOURCE=200809L']
    if 'process-stream' in ordered or any(
            profiles['profiles'].get(name, {}).get('sandbox_contract') for name in ordered):
        definitions.append('_GNU_SOURCE=1')
    result = dict(schema='on1x.mulib-plan/v1', libraries=list(libraries), requested_fixtures=list(fixtures),
                modules=entries, implementation_macros=macros, implementation_translation_units=1,
                required_symbols=required, dependency_cycles=cycles, fixtures=groups, files=records,
                native_sources=sources, include_headers=headers, generator_checks=sorted(checks),
                link_libraries=sorted(links), include_directories=[str(p) for p in directories],
                compile_definitions=definitions, inputs=inputs, safe_export=False,
                **({'discovery_only': discovery_only} if discovery_only else {}))
    # Results cannot mutate the shared manifest or profile structures.
    return deepcopy(result) if prepared is not None else result


def declaration_plan(root, name, modules, paths):
    """Plan canonical MANIFEST inputs only, not an implementation or fixture."""
    ordered, expanded, active = [], set(), set()

    def visit(module):
        # MANIFEST describes implementation edges too. A cycle there does not
        # prevent the compiler from reading the selected declaration header.
        if module in expanded or module in active:
            return
        if len(active) >= 128:
            raise ValueError('declaration dependency depth limit')
        if module not in modules:
            raise ValueError('UNSUPPORTED: no registered declaration dependency: '+module)
        active.add(module)
        for dependency in modules[module]['manifest_dependencies']:
            visit(dependency)
        active.remove(module)
        expanded.add(module)
        item = modules[module]
        header = local(root, item['header'])
        ordered.append(dict(name=module, **item, header_sha256=contracts.digest(header),
                            manual_sha256=contracts.digest(local(root, item['manual']))))

    visit(name)
    return dict(modules=ordered, inputs={},
                include_directories=include_paths(root, paths),
                implementation_macros=[], required_symbols={name: []},
                dependency_cycles=[], native_sources=[], include_headers=[], generator_checks=[])


def check_generated(root, recipe):
    for check in recipe['generator_checks']:
        p = subprocess.run([sys.executable, local(root, check), '--check'], capture_output=True, timeout=120)
        if p.returncode:
            raise ValueError('fixture/generated kernel mismatch: '+check+'\n'+(p.stderr+p.stdout)[:65536].decode(errors='replace'))


def implementation(root, recipe):
    """Implementation placement only; all declarations remain canonical C headers."""
    for module in recipe['modules']:
        if contracts.digest(header_path(root,module))!=module['header_sha256']:
            raise ValueError('kernel input drift: '+module['header'])
    return ('/* One MuLIB implementation TU; ABI declarations are not synthesized. */\n'+
            ''.join('#define '+item.replace('=', ' ', 1)+'\n' for item in recipe['compile_definitions'])+
            ''.join('#define '+macro+'\n' for macro in recipe['implementation_macros'])+
            ''.join('#include '+json.dumps(str(path))+'\n' for path in
                    [*[header_path(root,m) for m in recipe['modules']], *[local(root,p) for p in recipe['include_headers']]]))


def materialize(root, recipe, output):
    """Copy the selected library fixtures with their repository-relative names."""
    for record in recipe['files']:
        data = contracts.read(local(root, record['path']))
        if hashlib.sha256(data).hexdigest()!=record['sha256']:
            raise ValueError('fixture input drift: '+record['path'])
        dest = output/record['path'];dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)


def metadata_source(root, recipe):
    out = ['#include '+json.dumps(str(PACKAGE/'runtime.h'))]
    descriptions=[]
    for i, record in enumerate(recipe['files']):
        data = contracts.read(local(root, record['path']))
        if hashlib.sha256(data).hexdigest()!=record['sha256']:raise ValueError('fixture input drift')
        descriptions.append((len(data), b'\0' not in data))
        # Byte arrays preserve binary fixtures and avoid ISO C string-literal limits.
        out.append('static const unsigned char fixture_'+str(i)+'[] = {')
        for offset in range(0,len(data),32):out.append(','.join(str(b) for b in data[offset:offset+32])+',')
        out.append('0};')
    out.append('static const struct on1x_mulib_kernel kernels[] = {')
    for module in recipe['modules']:
        out.append('{'+','.join(json.dumps(v) for v in [module['name'],module['header'],module['header_sha256']])+ '},')
    out.append('{NULL,NULL,NULL}};')
    out.append('static const struct on1x_mulib_fixture fixtures[] = {')
    for i, record in enumerate(recipe['files']):
        size,text=descriptions[i]
        out.append('{'+','.join([json.dumps(record['path']),json.dumps(record['group']),json.dumps(record['sha256']),
                                'fixture_'+str(i),str(size), 'true' if text else 'false'])+'},')
    out.append('{NULL,NULL,NULL,NULL,0,false}};')
    out.append('#include '+json.dumps(str(PACKAGE/'runtime.c')))
    return '\n'.join(out)+'\n'


def dep_paths(data):
    """GCC/Clang -MM -MT on1x dependency output (make escaping, including spaces)."""
    text = data.decode().replace('\\\n', '')
    if not text.startswith('on1x:'):raise ValueError('unsupported compiler dependency output')
    paths, current, escaped = [], [], False
    for ch in text[len('on1x:'):]:
        if escaped:current.append(ch);escaped=False
        elif ch=='\\':escaped=True
        elif ch.isspace():
            if current:paths.append(Path(''.join(current).replace('$$','$')).resolve());current=[]
        else:current.append(ch)
    if escaped:raise ValueError('invalid compiler dependency escape')
    if current:paths.append(Path(''.join(current).replace('$$','$')).resolve())
    return sorted(set(paths))


def compile_inputs(compiler, flags, source):
    p = subprocess.run([str(compiler), '-std=c11', *map(str, flags), '-MM', '-MT', 'on1x', str(source)], capture_output=True, timeout=180)
    if p.returncode:raise ValueError('native dependency discovery failed: '+p.stderr[:65536].decode(errors='replace'))
    if len(p.stdout)>8*1024*1024:raise ValueError('native dependency output limit')
    return {str(path):contracts.digest(path) for path in dep_paths(p.stdout)}
