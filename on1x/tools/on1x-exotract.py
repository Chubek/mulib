#!/usr/bin/env python3
"""MuLIB static archives and typed import manifests through real Exotract.

No generated On1x ABI guesses: Exotract emits typed metadata. C11 clients link
the generated static archive; On1x clients still require an admitted ABI adapter.
"""
import argparse
import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HOME = Path(__file__).resolve().parents[1]
CATALOG = HOME/'contracts-exotract/catalog.json'
KERNEL_PROFILE = HOME/'contracts-exotract/kernel-discovery-v1.lua'
CLAMP_PARAMETERS = {
    'mulib_clamp_add_i64': ('a', 'b'),
    'mulib_clamp_sub_i64': ('a', 'b'),
    'mulib_clamp_i64': ('value', 'low', 'high'),
}
CLAMP_EXPORTS = tuple(CLAMP_PARAMETERS)
CLAMP_PROBE_COMPARISONS = 2 * (2 * (9**2 + 256) + 9 * (9 * 10 // 2) + 256)
KERNEL_CODEGEN_FLAGS = frozenset(('-O0', '-O1', '-O2', '-O3', '-g', '-fno-omit-frame-pointer',
                                  '-fsanitize=address,undefined', '-fno-sanitize-recover=all'))
CLAMP_ADAPTER_PREFIX = 'on1x_kernel_scalar_v1_'
HISTORY_EXPORTS = ('mulib_histprice_ema', 'mulib_histprice_holt')
HISTORY_IMPL = ['MULIB_IMPL_STATSPROC', 'MULIB_IMPL_HISTPRICE']
PRICING_EXPORTS = ('mulib_priceopt_optimize', 'mulib_priceopt_candidates')
PRICING_IMPL = ['MULIB_IMPL_PRICEOPT']
IPC_RAW_PROFILE = 'unsafe-ipc-framed-local-pair/v1'
IPC_IMPL = ['MULIB_IMPL_SANDBOX_PROFILE', 'MULIB_IMPL_IPC_FRAMED']
# Profile v1's known entry points are sanity checks, not binding allowlists.
KERNEL_MODULES = {
    'clamp': (None, list(CLAMP_EXPORTS)),
    'histprice': (None, ['mulib_histprice_ema', 'mulib_histprice_holt']),
    'statsprice': (None, ['mulib_statsprice_estimate']),
    'priceopt': (None, ['mulib_priceopt_optimize', 'mulib_priceopt_candidates']),
    'money': (None, ['mulib_money_make']),
    'json': ('json.exocontract.json', []),
    'prettysexpr': ('prettysexpr.exocontract.json', ['mulib_pretty_render_expr', 'mulib_pretty_parse_expr']),
    'ini': (None, ['mulib_ini_parse']),
    'cryptohash': (None, ['mulib_blake3', 'mulib_blake2s']),
    'socket': (None, ['mulib_socket_connect', 'mulib_socket_pair', 'mulib_socket_send',
                      'mulib_socket_receive', 'mulib_socket_close']),
    'channel': (None, ['mulib_channel_create', 'mulib_channel_sender', 'mulib_channel_receiver',
                       'mulib_channel_send', 'mulib_channel_receive', 'mulib_channel_close',
                       'mulib_channel_destroy']),
}
# On1x source identifiers cannot contain hyphens. These named sandbox profiles
# (and explicitly configured discovery-only prefixes) map underscores to dashes.
SANDBOX_MODULES = {name.replace('-', '_'): name for name in (
    'sandbox-profile', 'seccomp', 'landlock', 'namespace', 'cgroup2',
    'ipc-framed', 'capbroker', 'sandbox-process',
)}
spec = importlib.util.spec_from_file_location('contracts', HOME/'tools/on1x-contracts.py')
contracts = importlib.util.module_from_spec(spec)
spec.loader.exec_module(contracts)
spec = importlib.util.spec_from_file_location('mulib_housekeeping', HOME/'stdlib/mulib/housekeeping.py')
housekeeping = importlib.util.module_from_spec(spec)
spec.loader.exec_module(housekeeping)


def plan(root, libraries, fixtures=(), paths=(), *, discovery=False):
    if not libraries:raise ValueError('select MuLIB modules for typed extraction')
    recipe=housekeeping.plan(root,libraries,fixtures,paths,discovery=discovery)
    recipe['schema']='on1x.exotract-plan/v1'
    return recipe


def invoke(argv):
    p = subprocess.run([str(a) for a in argv], capture_output=True, timeout=180)
    if p.returncode:
        raise ValueError(f'{argv[0]} failed ({p.returncode}): '+p.stderr[:65536].decode(errors='replace'))
    if len(p.stdout)>64*1024*1024:
        raise ValueError('tool output limit')
    return p.stdout


def executable(value):
    resolved = shutil.which(str(value))
    if not resolved:
        raise ValueError('executable unavailable: '+str(value))
    return Path(resolved).resolve()


def require_lossless_contract(source, normalized, path='contract'):
    """Detect fields/types the canonical SDK discarded; do not interpret semantics."""
    if isinstance(source, dict):
        if not isinstance(normalized, dict) or set(source) - set(normalized):
            raise ValueError('ON1X-KERNEL-NOT-REPRESENTABLE: discarded ExoML fields at '+path)
        for key, value in source.items():
            require_lossless_contract(value, normalized[key], path+'.'+key)
    elif isinstance(source, list):
        if not isinstance(normalized, list) or len(source) != len(normalized):
            raise ValueError('ON1X-KERNEL-NOT-REPRESENTABLE: discarded ExoML sequence at '+path)
        for i, (left, right) in enumerate(zip(source, normalized)):
            require_lossless_contract(left, right, path+f'[{i}]')
    elif contracts.dumps(source) != contracts.dumps(normalized):
        raise ValueError('ON1X-KERNEL-NOT-REPRESENTABLE: changed ExoML value at '+path)


def sandbox_profile(name):
    if name not in SANDBOX_MODULES.values():
        return None
    profile = contracts.load(CATALOG)['profiles'][name]
    if profile['recipe'] != 'manifest' or profile['sandbox_contract'] != f'sandbox/{name}.exocontract.json':
        raise ValueError('invalid sandbox ExoLib recipe: '+name)
    return profile


def kernel_options(cflags):
    defines, undefines, flags = [], [], []
    for flag in cflags:
        if flag in KERNEL_CODEGEN_FLAGS:
            continue  # Recorded separately; these do not change the C declaration profile.
        if 'MULIB_IMPL_' in flag:
            raise ValueError('implementation macros belong to the generated TU, not KernelBind discovery')
        if flag.startswith('-D') and len(flag) > 2:
            defines.append(flag[2:])
        elif flag.startswith('-U') and len(flag) > 2:
            undefines.append(flag[2:])
        else:
            flags.append(flag)  # The real Exotract SDK allowlists ABI flags.
    return defines, undefines, flags


def extract_exolib(name, recipe, root, exotract, compiler, resource, target,
                   options, observe, *, sidecar_name, required):
    manifest_input = observe(housekeeping.local(root, 'MANIFEST.yaml'))
    lua_profile = observe(KERNEL_PROFILE)
    discovery_only = recipe.get('discovery_only', {}).get(name)
    if discovery_only and sidecar_name:
        raise ValueError('discovery-only kernel cannot attach a semantic sidecar: '+name)
    for path, digest in recipe['inputs'].items():
        observe(path, digest)
    if recipe['dependency_cycles'] or recipe['native_sources'] or recipe['include_headers'] or recipe['generator_checks']:
        raise ValueError('discovery profile has unsupported build requirements')
    modules = {}
    for item in recipe['modules']:
        header = housekeeping.local(root, item['header'])
        if root/item['header'] != header:
            raise ValueError('symlinked MuLIB kernel header: '+item['header'])
        # Reject additional candidates, even identical copies or broken symlinks.
        for directory in recipe['include_directories']:
            candidate = Path(directory)/header.name
            if candidate != header and (candidate.exists() or candidate.is_symlink()):
                raise ValueError('ambiguous MuLIB kernel header: '+str(candidate))
        if housekeeping.header_path(root, item) != header:
            raise ValueError('noncanonical MuLIB header: '+item['header'])
        doc = observe(housekeeping.local(root, item['manual']), item['manual_sha256'])
        modules[item['name']] = (observe(header, item['header_sha256']), doc, item['header_sha256'])
    header, manual, revision = modules[name]
    sandbox = sandbox_profile(name)
    defines, undefines, flags = options
    if sandbox:
        if not re.fullmatch(r'(?:x86_64|aarch64)(?:-[a-z0-9_]+)?-linux-gnu', target):
            raise ValueError('UNSUPPORTED: sandbox ExoLib requires a Linux x86-64 or AArch64 GNU target')
        if defines or undefines or flags:
            raise ValueError('sandbox ExoLib only accepts the documented codegen flags; declaration features are fixed')
        defines = recipe['compile_definitions']
    sidecar = None
    dependencies = [h for mod, (h, _, _) in modules.items() if mod != name]
    pins = [manifest_input, lua_profile, manual, *dependencies]
    if sandbox or discovery_only:
        pins += [observe(CATALOG), *(doc for mod, (_, doc, _) in modules.items() if mod != name)]
    if sidecar_name:
        sidecar = observe(housekeeping.local(contracts.CONTRACTS, sidecar_name))
        pins.append(sidecar)
        original_contract = contracts.load(sidecar)  # Reject duplicate keys before the SDK.
        if sandbox and original_contract['bundle_provenance']['revision'] != 'sha256:'+revision:
            raise ValueError('sandbox ExoML sidecar header revision drift: '+name)
    required = sorted(set(required + recipe['required_symbols'][name]))
    # Do not pass ExoLib's `target` override: it adds --target to C
    # preprocessing and native GCC does not accept that option.
    globs = dict(KERNEL=name, HEADER=header, MANUAL=manual, REVISION='sha256:'+revision,
                 COMPILER=compiler, RESOURCE=resource)
    if sidecar:
        globs['CONTRACT'] = sidecar
    for key, values in dict(INCLUDES=[root/'include'], PINS=pins,
                            REQUIRED=required, FLAGS=flags, DEFINES=defines,
                            UNDEFINES=undefines, DEPENDENCIES=dependencies,
                            IMPL=recipe['implementation_macros']).items():
        globs[key+'_COUNT'] = len(values)
        globs.update({f'{key}_{i}': v for i, v in enumerate(values, 1)})
    argv = [exotract, 'lua', lua_profile]
    for k, v in globs.items():
        if '$GLOBAL{' in str(v):
            raise ValueError('nested ExoLib global substitution is not allowed')
        argv += ['--global', f'{k}={v}']
    wire = contracts.loads(invoke(argv))
    profile, on1x, contract = (wire[key] for key in ('profile', 'on1x', 'contract'))
    decl = on1x['cdecl']
    if (profile['schema'] != 'moonstone.exotract/2' or profile['name'] != name or
        profile['header'] != str(header) or profile['revision'] != globs['REVISION'] or
        profile['required_symbols'] != required or
        profile['safe_export'] is not False or profile['complete'] is not True or
        profile['syntax_checked'] is not True or
        on1x['schema'] != 'moonstone.exotract.on1x/2' or on1x['library'] != name or
        on1x['unsafe_by_default'] is not True or on1x['requires_review'] is not True or
        decl['schema_version'] != 2 or decl['complete'] is not True or decl['safe_export'] is not False or
        decl['header'] != str(header) or decl['revision'] != globs['REVISION'] or
        decl['target']['triple'] != target):
        raise ValueError('incomplete or mismatched ExoLib/CDecl identity')
    impl = profile['header_only']
    if (impl['implementation_translation_units'] != 1 or
        impl['implementation_defines'] != recipe['implementation_macros'] or
        impl['dependencies'] != [str(h) for h in dependencies]):
        raise ValueError('mismatched ExoLib implementation profile')
    decl_inputs = set()
    for record in decl['inputs']:
        path = observe(record['path'], record['sha256'])
        if path in decl_inputs:
            raise ValueError('duplicate CDecl input')
        decl_inputs.add(path)
    if not {header, *pins} <= decl_inputs:
        raise ValueError('incomplete CDecl provenance pins')
    preprocessing = decl['preprocessing']
    active = {m['name']: m for m in preprocessing['final_macros'] if m['defined']}
    if preprocessing['final_snapshot_complete'] is not True or any(n.startswith('MULIB_IMPL_') for n in active):
        raise ValueError('not a complete declaration-only CDecl macro snapshot')
    if sandbox and not {'_GNU_SOURCE', '_POSIX_C_SOURCE'} <= active.keys():
        raise ValueError('incomplete sandbox declaration feature macros')
    if sidecar and (sandbox or name in ('clamp', 'histprice', 'priceopt')):
        require_lossless_contract(original_contract, contract)
    if (sidecar is None and contract is not None) or (sidecar is not None and (
        not isinstance(contract, dict) or contract['schema_version'] != 2 or contract['library'] != name or
        contract['bundle_provenance']['revision'] != globs['REVISION'])):
        raise ValueError('mismatched ExoML semantic sidecar')
    declarations = [d for d in decl['declarations'] if d['kind'] == 'function']
    functions = {d['name']: d for d in declarations}
    if discovery_only:
        inline = next(item for item in recipe['modules'] if item['name'] == name)['macro'] is None
        # Static-inline bodies are real definitions with internal linkage, not
        # externally callable archive exports. Preserve the compiler's evidence;
        # never rewrite them into extern declarations to satisfy this check.
        invalid = any(d['source']['file'] != str(header) or
                      (not d['definition'] or not d['inline'] or d['linkage'] != '2'
                       if inline else d['definition']) for d in declarations)
        if (profile['parser'] != 'exoml/libclang' or profile['externally_imported'] is not False or
                (not functions and not inline) or len(functions) != len(declarations) or invalid):
            raise ValueError('discovery-only kernel requires canonical CDecl linkage: '+name)
    type_ids = {entry['id'] for entry in decl['types'] if entry['kind'] == 'function'}
    for entry in on1x['imports']:
        if entry['safe'] is not False or entry['c_name'] not in functions or entry['c_type_id'] not in type_ids:
            raise ValueError('unexpected Exotract import metadata')
    if sandbox:
        # The SDK validates structure but does not fully bind names or review
        # state. Require full coverage and exact parameter names; never admit
        # a sandbox sidecar, regardless of its annotations.
        claimed = contract['functions']
        if (profile['parser'] != 'exoml/libclang' or profile['externally_imported'] is not False or
            decl['target']['pointer_bits'] != 64 or not re.fullmatch('[0-9a-f]{64}', decl['fingerprint']) or
            set(functions) != set(required) or len(declarations) != len(required) or len(claimed) != len(required) or
            {f['c_name'] for f in claimed} != set(required) or
            contract['headers'] != [header.name] or contract['state'] != 'annotated' or
            contract['bundle_provenance']['header'] != header.name or
            contract['bundle_provenance']['review_status'] != 'unreviewed'):
            raise ValueError('sandbox CDecl identity, ExoML export coverage or unreviewed provenance mismatch')
        if (len(on1x['imports']) != len(required) or
            {e['c_name']: e['c_type_id'] for e in on1x['imports']} != {n: d['type'] for n, d in functions.items()}):
            raise ValueError('sandbox Exotract import/CDecl mismatch')
        for f in claimed:
            d = functions[f['c_name']]
            if (d['source']['file'] != str(header) or d['definition'] or
                f['provenance']['header'] != header.name or f['provenance']['review_status'] != 'unreviewed' or
                [(p['index'], p['name']) for p in f.get('parameters', [])] !=
                list(enumerate(p['name'] for p in d['parameters']))):
                raise ValueError('sandbox ExoML declaration mismatch: '+f['c_name'])
    status = 'not_representable' if sandbox or discovery_only else 'missing_review'
    reason = discovery_only or ('no checked sandbox ABI, ownership/effect lowering or independent review' if sandbox else
              'no independent KernelBind authorization' if sidecar else
              'no ExoML semantic sidecar or independent KernelBind authorization')
    return dict(exolib=profile, metadata=on1x, contract=contract, callable=False, status=status,
                safe_export=False, symbols={sym: dict(status=status, callable=False, reason=reason) for sym in functions},
                sidecar=str(sidecar) if sidecar else None)


def manifest_discovery_name(logical, modules, profiles):
    """Resolve only metadata-eligible MANIFEST names, never an archive recipe."""
    candidates = {n for n in (logical, logical.replace('_', '-')) if n in modules}
    if len(candidates) > 1:
        raise ValueError('ambiguous manifest alias: mulib.'+logical)
    name = next(iter(candidates), None)
    if name is not None and contracts.module_discovery_only_reason(name, modules[name], profiles):
        return name
    return None


def kernel_discovery(requests, root, paths, exotract, compiler, resource_dir, cflags, pins_before_load, *, review=False):
    """Resolve *compiler* source requests. This produces no interface or link input."""
    def location(request):
        return f"{request['file']}:{request['line']}:{request['column']}: "

    if requests['schema'] != 'on1x.kernel-requests/v1':
        raise ValueError('unsupported compiler KernelBind request schema')
    sources = {os.fsdecode(bytes.fromhex(s['file_hex'])): s['sha256'] for s in requests['sources']}
    imports = [{k: v for k, v in r.items() if k != 'file_hex'} |
               {'file': os.fsdecode(bytes.fromhex(r['file_hex']))} for r in requests['requests']]
    if not imports or len(imports) > 1024:
        raise ValueError('KernelBind inspection requires 1..1024 source requests')
    # MANIFEST registration is not permission to build or call a kernel.
    # Bounded static-inline modules may expose compiler-derived metadata only;
    # ABI modules still require an explicit profile or discovery-only policy.
    catalog_hash = contracts.digest(CATALOG)
    profiles = contracts.exotract_catalog()
    manifest = None
    selected = {}
    for request in imports:
        match = re.fullmatch(r'mulib\.([a-z][a-z0-9_]*)', request['module'])
        logical = match[1] if match else ''
        if logical in KERNEL_MODULES or logical in SANDBOX_MODULES:
            name = SANDBOX_MODULES.get(logical, logical)
        elif logical:
            if manifest is None:
                manifest = housekeeping.inventory(root)
            try:
                name = manifest_discovery_name(logical, manifest, profiles)
            except ValueError as error:
                raise ValueError(location(request)+'ON1X-KERNEL-PROFILE: '+str(error)) from error
        else:
            name = None
        if name is None:
            raise ValueError(location(request)+'ON1X-KERNEL-PROFILE: no discovery profile: '+request['module'])
        selected.setdefault(logical, (request, name))
    observed = {}

    def observe(path, expected=None):
        real = Path(path).resolve()
        digest = contracts.digest(real)
        if (expected is not None and digest != expected) or (real in observed and digest != observed[real]):
            raise ValueError('KernelBind discovery input drift: '+str(real))
        observed[real] = digest
        return real

    for path, digest in sources.items():
        observe(path, digest)
    for path, digest in pins_before_load.items():
        observe(path, digest)
    observe(CATALOG, catalog_hash)
    exotract, compiler = map(executable, [exotract, compiler])
    for tool in [exotract, compiler, Path(__file__)]:
        observe(tool)
    resource = Path(resource_dir or invoke([compiler, '-print-resource-dir']).decode().strip()).resolve()
    if not (resource/'include/stddef.h').is_file():
        raise ValueError('provide --resource-dir from a compatible clang')
    target = invoke([compiler, '-dumpmachine']).decode().strip()
    options = kernel_options(cflags)

    discovered = {}
    for logical, (request, name) in selected.items():
        try:
            sandbox = sandbox_profile(name)
            sidecar_name, required = (sandbox['sandbox_contract'], []) if sandbox else KERNEL_MODULES.get(name, (None, []))
            if name == 'clamp' and any(r['module'] == 'mulib.clamp' and not r['unsafe'] for r in imports):
                sidecar_name = 'clamp.exocontract.json'
            if name in ('histprice', 'priceopt') and review:
                sidecar_name = name+'.exocontract.json'
            discovered['mulib.'+logical] = extract_exolib(
                name, plan(root, [name], paths=paths, discovery=True), root, exotract, compiler, resource, target,
                options, observe, sidecar_name=sidecar_name, required=required)
        except (ValueError, OSError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
            raise ValueError(location(request)+'ON1X-KERNEL-DISCOVERY: '+str(error)) from error
    compiler_version = invoke([compiler, '--version']).decode()
    for path, digest in list(observed.items()):
        observe(path, digest)
    return dict(schema='on1x.kernel-discovery/v1', safe_export=False, requests=imports,
                source_inputs=sources,
                modules=discovered, inputs={str(p): h for p, h in observed.items()},
                exotract=str(exotract), compiler=str(compiler), target=target, cflags=cflags,
                compiler_version=compiler_version, resource_directory=str(resource))


def clamp_scalar_interface(report, *, unsafe):
    """Project the exact, deliberately narrow executable CDecl shape.

    Only use an in-process discovery result with pinned inputs; this function
    is not a reader for public discovery reports or admission certificates.
    """
    for request in report['requests']:
        if request['unsafe'] is not unsafe or request['module'] != 'mulib.clamp':
            code = 'UNSAFE-UNSUPPORTED' if request['unsafe'] else 'NOT-ADMITTED'
            raise ValueError(f"{request['file']}:{request['line']}:{request['column']}: "
                             f"ON1X-KERNEL-{code}: {request['module']}")
    module = report['modules']['mulib.clamp']
    cdecl = module['metadata']['cdecl']
    types = {t['id']: t for t in cdecl['types']}
    if (len(types) != len(cdecl['types']) or not cdecl['fingerprint'] or
        not re.fullmatch(r'x86_64(?:-[a-z0-9_]+)?-linux-gnu', report['target']) or
        cdecl['target']['pointer_bits'] != 64 or
        module['exolib']['header_only']['implementation_defines'] != ['MULIB_IMPL_CLAMP']):
        raise ValueError('ON1X-KERNEL-ABI: unsupported clamp CDecl target or implementation profile')

    def scalar(type_id):
        t = types[type_id]
        c = types[t['canonical_id']]
        return (t['kind'] == 'typedef' and t['spelling'] == 'int64_t' and
                c['kind'] == 'integer' and c['signed'] is True and
                all(x['size'] == 8 and x['alignment'] == 8 and
                    not (x['const'] or x['volatile'] or x['restrict'] or x['attributes'] or
                         x['unsupported_reason']) for x in (t, c)))

    exports = {}
    for symbol in CLAMP_EXPORTS:
        candidates = [d for d in cdecl['declarations'] if d['name'] == symbol]
        if len(candidates) != 1:
            raise ValueError('ON1X-KERNEL-ABI: missing or ambiguous declaration for '+symbol)
        d = candidates[0]
        f = types[d['type']]
        if (d['kind'] != 'function' or d['definition'] or d['inline'] or d['linkage'] != '4' or
            d['source']['file'] != cdecl['header'] or d['unsupported_reason'] or
            f['kind'] != 'function' or f['calling_convention'] != 1 or f['variadic'] or
            not f['prototype'] or f['attributes'] or f['unsupported_reason'] or
            len(f['arguments']) != len(CLAMP_PARAMETERS[symbol]) or
            [p['name'] for p in d['parameters']] != list(CLAMP_PARAMETERS[symbol]) or
            [p['type'] for p in d['parameters']] != f['arguments'] or
            not all(scalar(t) for t in [f['result'], *f['arguments']])):
            raise ValueError('ON1X-KERNEL-ABI: incompatible CDecl signature for '+symbol)
        exports[symbol.removeprefix('mulib_clamp_')] = dict(
            c_name=symbol, c_type_id=d['type'], declaration_id=d['id'],
            declaration_source=d['source'],
            parameters=['i64'] * len(CLAMP_PARAMETERS[symbol]), result='i64',
            status='unsafe' if unsafe else 'missing_review', effect='unsafe_foreign' if unsafe else 'foreign_call')
        if symbol == 'mulib_clamp_i64':
            exports['i64']['precondition'] = {
                'require': 'low <= high', 'enforcement': 'On1x checks before the C call; assertion failure exits 70'}
    return dict(schema='on1x.kernel-interface/v1', profile='unsafe-clamp-i64/v2' if unsafe else 'reviewed-clamp-i64/v2',
                module='mulib.clamp', safe_export=False, target=report['target'],
                cdecl_fingerprint=cdecl['fingerprint'], header=cdecl['header'],
                header_revision=cdecl['revision'], imports=report['requests'], exports=exports,
                rejected={})


def ipc_raw_interface(report):
    """Admit only the ABI of the fixed local-pair adapter, never a safe export."""
    for request in report['requests']:
        if request['unsafe'] is not True or request['module'] != 'mulib.ipc_framed':
            code = 'UNSAFE-UNSUPPORTED' if request['unsafe'] else 'NOT-ADMITTED'
            raise ValueError(f"{request['file']}:{request['line']}:{request['column']}: "
                             f"ON1X-KERNEL-{code}: {request['module']} cannot join the raw framed transport profile")
    module = report['modules']['mulib.ipc_framed']
    cdecl = module['metadata']['cdecl']
    types = {t['id']: t for t in cdecl['types']}

    def require(test, item):
        if not test:
            raise ValueError('ON1X-KERNEL-ABI: incompatible framed transport CDecl: '+item)

    require(len(types) == len(cdecl['types']) and
            re.fullmatch(r'x86_64(?:-[a-z0-9_]+)?-linux-gnu', report['target']) and
            cdecl['target']['pointer_bits'] == 64 and
            cdecl['target']['triple'] == report['target'] and
            module['exolib']['header_only']['implementation_defines'] == IPC_IMPL, 'target/implementation')

    scalar_types = {'int': (4, True), 'uint32_t': (4, False), 'int64_t': (8, True),
                    'uint64_t': (8, False), 'size_t': (8, False)}

    def clean(t, const=False):
        return t['const'] is const and not (
            t['volatile'] or t['restrict'] or t['attributes'] or t['unsupported_reason'])

    def has_type(t, spelling):
        if t['spelling'] != spelling or not clean(t, spelling == 'const void'):
            return False
        canonical = types[t['canonical_id']]
        if not clean(canonical, spelling == 'const void'):
            return False
        if spelling.endswith(' *'):
            return (t['kind'] == canonical['kind'] == 'pointer' and
                    t['size'] == t['alignment'] == canonical['size'] == canonical['alignment'] == 8 and
                    has_type(types[t['element']], spelling[:-2]) and
                    types[t['element']]['canonical_id'] == canonical['element'])
        if spelling in scalar_types:
            size, signed = scalar_types[spelling]
            return (t['kind'] == ('integer' if spelling == 'int' else 'typedef') and
                    t['size'] == t['alignment'] == canonical['size'] == canonical['alignment'] == size and
                    canonical['kind'] == 'integer' and canonical['signed'] is signed)
        if spelling == 'mulib_ipc_framed_t':
            fields = canonical['fields']
            return (t['kind'] == 'typedef' and canonical['kind'] == 'record' and
                    canonical['spelling'] == 'struct mulib_ipc_framed' and not canonical['union'] and
                    t['size'] == canonical['size'] == 12 and
                    t['alignment'] == canonical['alignment'] == 4 and len(fields) == 3 and
                    all(f['name'] == name and f['offset_bits'] == offset and f['bit_width'] is None and
                        f['source']['file'] == cdecl['header'] and has_type(types[f['type']], field_type)
                        for f, (name, offset, field_type) in zip(fields, (
                            ('fd', 0, 'int'), ('poisoned', 32, 'int'), ('max_frame', 64, 'uint32_t')))))
        return spelling in ('void', 'const void') and t['kind'] == canonical['kind'] == 'void'

    stream = 'mulib_ipc_framed_t *'
    signatures = {
        'mulib_ipc_framed_init': ('int', (stream, 'int', 'uint32_t')),
        'mulib_ipc_deadline': ('int', ('uint64_t', 'int64_t *')),
        'mulib_ipc_send': ('int', (stream, 'const void *', 'size_t', 'int64_t')),
        'mulib_ipc_receive': ('int', (stream, 'void *', 'size_t', 'size_t *', 'int64_t')),
    }
    dependencies = {}
    for name, (result, arguments) in signatures.items():
        matches = [d for d in cdecl['declarations'] if d['name'] == name]
        require(len(matches) == 1, name)
        decl = matches[0]
        f = types[decl['type']]
        require(decl['kind'] == 'function' and not decl['definition'] and not decl['inline'] and
                decl['linkage'] == '4' and decl['source']['file'] == cdecl['header'] and
                not decl['unsupported_reason'] and f['kind'] == 'function' and clean(f) and
                f['calling_convention'] == 1 and f['prototype'] and not f['variadic'] and
                [p['type'] for p in decl['parameters']] == f['arguments'] and
                len(f['arguments']) == len(arguments) and has_type(types[f['result']], result) and
                all(has_type(types[t], want) for t, want in zip(f['arguments'], arguments)), name)
        dependencies[name] = dict(declaration_id=decl['id'], c_type_id=f['id'], declaration_source=decl['source'])
    return dict(schema='on1x.kernel-interface/v1', profile=IPC_RAW_PROFILE, module='mulib.ipc_framed',
                safe_export=False, target=report['target'], cdecl_fingerprint=cdecl['fingerprint'],
                header=cdecl['header'], header_revision=cdecl['revision'], sidecar=module['sidecar'],
                imports=report['requests'], c_dependencies=dependencies, exports={
                    'open_pair': dict(c_name='on1x_kernel_ipc_v1_open_pair', parameters=[],
                                      result='Result<opaque move-only owner,i64>', effect='unsafe_foreign',
                                      status='unsafe', live_pair_limit=128),
                     'roundtrip': dict(c_name='on1x_kernel_ipc_v1_roundtrip',
                                       parameters=['consumed opaque owner', 'u8', 'i64', '(i64)->i64'],
                                       result='Result<i64,i64>', effect='unsafe_foreign', status='unsafe',
                                       callback='same-thread, synchronous, zero or one invocation after close',
                                       timeout_ms=[1, 1000]),
                     'open_session': dict(c_name='on1x_kernel_ipc_v1_open_session', parameters=[],
                                          result='Result<own<Session>,i64>', effect='unsafe_foreign',
                                          status='unsafe', shared_live_pair_limit=128),
                     'register_callback': dict(c_name='on1x_kernel_ipc_v1_register_callback',
                                               parameters=['exclusive session loan', '(i64)->i64'],
                                               result='Result<i64,i64>', effect='unsafe_foreign',
                                               status='unsafe', retained_callbacks_per_owner=1),
                     'revoke': dict(c_name='on1x_kernel_ipc_v1_revoke',
                                    parameters=['exclusive session loan'],
                                    result='Result<i64,i64>', effect='unsafe_foreign', status='unsafe'),
                     'send': dict(c_name='on1x_kernel_ipc_v1_send',
                                  parameters=['exclusive session loan', 'u8', 'i64'],
                                  result='Result<i64,i64>', effect='unsafe_foreign', status='unsafe',
                                  pending_frame_limit=1, timeout_ms=[1, 1000]),
                     'poll': dict(c_name='on1x_kernel_ipc_v1_poll',
                                  parameters=['exclusive session loan', 'i64'],
                                  result='Result<i64,i64>', effect='unsafe_foreign', status='unsafe',
                                  callback='opening thread, at most once, no reentry on the same owner',
                                  timeout_ms=[1, 1000]),
                     'finish_send': dict(c_name='on1x_kernel_ipc_v1_finish_send',
                                         parameters=['exclusive session loan'],
                                         result='Result<i64,i64>', effect='unsafe_foreign', status='unsafe')}, rejected={
                    'direct_functions': 'The canonical C API remains unadmitted.',
                    'safe_import': 'No independent semantic review or safe usage verification.'})


def history_interface(report):
    module = report['modules']['mulib.histprice']
    cdecl = module['metadata']['cdecl']
    types = {t['id']: t for t in cdecl['types']}

    def require(condition, what):
        if not condition:
            raise ValueError('ON1X-KERNEL-ABI: incompatible histprice '+what)

    require(len(types) == len(cdecl['types']) and re.fullmatch('[0-9a-f]{64}', cdecl['fingerprint']) and
            re.fullmatch(r'x86_64(?:-[a-z0-9_]+)?-linux-gnu', report['target']) and
            cdecl['target']['pointer_bits'] == 64 and
            module['exolib']['header_only']['implementation_defines'] == HISTORY_IMPL, 'target/implementation')

    def shape(t, kind, spelling=None, size=8, align=8, const=False):
        return (t['kind'] == kind and (spelling is None or t['spelling'] == spelling) and
                t['size'] == size and t['alignment'] == align and t['const'] is const and
                not any(t[k] for k in ('volatile', 'restrict', 'attributes', 'unsupported_reason')))

    def scalar(t, spelling='double', const=False):
        if spelling == 'size_t':
            c = types[t['canonical_id']]
            return shape(t, 'typedef', 'size_t') and shape(c, 'integer') and c['signed'] is False
        if spelling == 'int':
            return shape(t, 'integer', 'int', 4, 4) and t['signed'] is True
        return shape(t, 'floating', 'const double' if const else 'double', const=const)

    def declaration(symbol, kind):
        rows = [d for d in cdecl['declarations'] if d['name'] == symbol]
        require(len(rows) == 1 and rows[0]['kind'] == kind and rows[0]['source']['file'] == cdecl['header']
                and not rows[0]['unsupported_reason'], symbol+' declaration')
        return rows[0]

    td = declaration('mulib_histprice_forecast_t', 'typedef')
    ty = types[td['type']]
    record = types[ty['canonical_id']]
    require(shape(ty, 'typedef', 'mulib_histprice_forecast_t', 16) and
            shape(record, 'record', 'struct mulib_histprice_forecast', 16) and
            not record['union'] and len(record['fields']) == 2, 'forecast record layout')
    for i, name in enumerate(('point', 'variance')):
        field = record['fields'][i]
        require(field['name'] == name and field['offset_bits'] == i*64 and field['bit_width'] is None and
                scalar(types[field['type']]), 'forecast field '+name)

    exports = {}
    for symbol in HISTORY_EXPORTS:
        d = declaration(symbol, 'function')
        f = types[d['type']]
        names = ['prices', 'n', 'alpha', *(['beta', 'phi'] if symbol.endswith('_holt') else []), 'horizon', 'out']
        require(not d['definition'] and not d['inline'] and d['linkage'] == '4' and
                shape(f, 'function', size=None, align=None) and f['calling_convention'] == 1 and
                not f['variadic'] and f['prototype'] and scalar(types[f['result']], 'int') and
                [p['name'] for p in d['parameters']] == names and
                [p['type'] for p in d['parameters']] == f['arguments'], symbol+' signature')
        args = [types[t] for t in f['arguments']]
        require(shape(args[0], 'pointer') and scalar(types[args[0]['element']], const=True) and
                scalar(args[1], 'size_t') and scalar(args[-2], 'size_t') and
                all(scalar(t) for t in args[2:-2]) and
                shape(args[-1], 'pointer') and args[-1]['element'] == ty['id'], symbol+' wire types')
        member = symbol.removeprefix('mulib_histprice_')
        exports[member] = dict(c_name=symbol, c_type_id=d['type'], declaration_id=d['id'],
                               declaration_source=d['source'], c_output_type_id=record['id'],
                               parameters=['slice<f64,N:0..256>', *(['f64']*(len(args)-4)), 'i64'],
                               result='Result<Forecast,Error>', effects=['read','write','foreign_call'],
                               status='missing_review')
    rejected = {d['name'].removeprefix('mulib_histprice_'): dict(
                    c_name=d['name'], status='unsupported', reason='no checked adapter in this profile')
                for d in cdecl['declarations']
                if d['kind'] == 'function' and d['name'].startswith('mulib_histprice_') and d['name'] not in HISTORY_EXPORTS}
    return dict(schema='on1x.kernel-interface/v1', profile='reviewed-histprice-f64/v1',
                module='mulib.histprice', target=report['target'], safe_export=False,
                cdecl_fingerprint=cdecl['fingerprint'], header=cdecl['header'], header_revision=cdecl['revision'],
                imports=report['requests'], exports=exports, rejected=rejected,
                types={'Forecast': {'point':'f64', 'variance':'f64'}, 'Error':['Rejected'], 'Result':['Ok(T)','Err(E)']})


PRICING_ENUMS = {
    'demand_kind': ['LINEAR', 'ISOELASTIC', 'LOGISTIC'],
    'objective': ['PROFIT', 'REVENUE'],
}
PRICING_RECORDS = {
    'demand_model': [('kind', 'demand_kind'), ('scale', 'double'), ('shape', 'double'), ('reference', 'double')],
    'constraints': [(name, 'double') for name in (
        'min_price', 'max_price', 'unit_cost', 'min_unit_margin', 'previous_price', 'max_relative_change', 'tick')],
    'result': [('price', 'double'), ('score', 'double'), ('expected_demand', 'double'), ('evaluations', 'size_t')],
}


def pricing_interface(report):
    module = report['modules']['mulib.priceopt']
    cdecl = module['metadata']['cdecl']
    types = {t['id']: t for t in cdecl['types']}

    def require(test, where):
        if not test:
            raise ValueError('ON1X-KERNEL-ABI: incompatible priceopt '+where)

    require(len(types) == len(cdecl['types']) and re.fullmatch('[0-9a-f]{64}', cdecl['fingerprint']) and
            re.fullmatch(r'x86_64(?:-[a-z0-9_]+)?-linux-gnu', report['target']) and
            cdecl['target']['pointer_bits'] == 64 and
            module['exolib']['header_only']['implementation_defines'] == PRICING_IMPL, 'target/implementation')

    def clean(t, kind, width, align, *, const=False):
        return (t['kind'] == kind and t['size'] == width and t['alignment'] == align and t['const'] is const
                and not any(t[k] for k in ('attributes', 'unsupported_reason', 'restrict', 'volatile')))

    def decl(name, kind):
        hits = [d for d in cdecl['declarations'] if d['name'] == name]
        require(len(hits) == 1 and hits[0]['kind'] == kind and not hits[0]['unsupported_reason']
                and hits[0]['source']['file'] == cdecl['header'], name+' declaration')
        return hits[0]

    def typedef(name, width, align, kind):
        spelling = 'mulib_priceopt_'+name
        t = types[decl(spelling+'_t', 'typedef')['type']]
        c = types[t['canonical_id']]
        require(clean(t, 'typedef', width, align) and clean(c, kind, width, align)
                and t['spelling'] == spelling+'_t' and c['spelling'] == ('enum ' if kind == 'enum' else 'struct ')+spelling,
                name+' typedef')
        return t, c

    projections = {}
    for name, values in PRICING_ENUMS.items():
        t, c = typedef(name, 4, 4, 'enum')
        underlying = types[c['element']]
        require(clean(underlying, 'integer', 4, 4) and c['signed'] is False and
                underlying['signed'] is False and c['enumerators'] ==
                [dict(name='MULIB_PRICEOPT_'+tag, value=str(i)) for i, tag in enumerate(values)], name+' enum')
        projections[name] = t

    def scalar(t, name, const=False):
        if name == 'double':
            return clean(t, 'floating', 8, 8, const=const) and t['spelling'] == ('const double' if const else 'double')
        if name == 'int':
            return clean(t, 'integer', 4, 4) and t['signed'] is True and t['spelling'] == 'int'
        if name == 'size_t':
            c = types[t['canonical_id']]
            return clean(t, 'typedef', 8, 8) and t['spelling'] == 'size_t' and clean(c, 'integer', 8, 8) and c['signed'] is False
        p = projections[name]
        return (clean(t, 'typedef', p['size'], p['alignment'], const=const) and
                t['declaration_id'] == p['declaration_id'] and
                t['spelling'] == ('const ' if const else '')+p['spelling'])

    for name, fields in PRICING_RECORDS.items():
        t, c = typedef(name, len(fields)*8, 8, 'record')
        require(not c['union'] and len(c['fields']) == len(fields), name+' record')
        for i, ((field, spelling), actual) in enumerate(zip(fields, c['fields'])):
            require(actual['name'] == field and actual['offset_bits'] == 64*i and actual['bit_width'] is None
                    and scalar(types[actual['type']], spelling), name+'.'+field)
        projections[name] = t

    exports = {}
    for name in PRICING_EXPORTS:
        candidates = name.endswith('_candidates')
        d = decl(name, 'function')
        t = types[d['type']]
        names = ['prices','predicted_units','n'] if candidates else ['model']
        names += ['constraints','objective','out']
        shapes = [('double', True, True), ('double', True, True), ('size_t', False, False)] if candidates else [
            ('demand_model', True, True)]
        shapes += [('constraints', True, True), ('objective', False, False), ('result', True, False)]
        require(not d['definition'] and not d['inline'] and d['linkage'] == '4' and
                clean(t, 'function', None, None) and t['calling_convention'] == 1 and t['prototype'] and not t['variadic']
                and [p['name'] for p in d['parameters']] == names and
                [p['type'] for p in d['parameters']] == t['arguments'] and scalar(types[t['result']], 'int'), name+' signature')
        for i, (spelling, pointer, const) in enumerate(shapes):
            arg = types[t['arguments'][i]]
            if pointer:
                require(clean(arg, 'pointer', 8, 8), name+' pointer')
                arg = types[arg['element']]
            require(scalar(arg, spelling, const), name+' '+names[i]+' type')
        exports[name.removeprefix('mulib_priceopt_')] = dict(
            c_name=name, c_type_id=d['type'], declaration_id=d['id'], declaration_source=d['source'],
            c_type_ids={key: t['canonical_id'] for key, t in projections.items()},
            parameters=(['slice<f64,N:0..256>']*2 if candidates else ['DemandModel'])+['Constraints','Objective'],
            result='Result<Decision,Error>', effects=['read','write','foreign_call'], status='missing_review')
    rejected = {d['name'].removeprefix('mulib_priceopt_'): dict(
                    c_name=d['name'], status='unsupported', reason='no checked adapter in this profile')
                for d in cdecl['declarations']
                if d['kind'] == 'function' and d['name'].startswith('mulib_priceopt_') and d['name'] not in PRICING_EXPORTS}
    return dict(schema='on1x.kernel-interface/v1', profile='reviewed-priceopt-f64/v1', module='mulib.priceopt',
                target=report['target'], safe_export=False, cdecl_fingerprint=cdecl['fingerprint'],
                header=cdecl['header'], header_revision=cdecl['revision'], imports=report['requests'],
                exports=exports, rejected=rejected,
                types={'DemandKind': ['Linear','Isoelastic','Logistic'], 'Objective': ['Profit','Revenue'],
                       'DemandModel': {'kind':'DemandKind', 'scale':'f64', 'shape':'f64', 'reference':'f64'},
                       'Constraints': {name:'f64' for name, _ in PRICING_RECORDS['constraints']},
                       'Decision': {'price':'f64','score':'f64','expected_demand':'f64','evaluations':'u64'},
                       'Error':['Rejected'], 'Result':['Ok(T)','Err(E)']})


def reviewed_interface(report):
    modules = {}
    for r in report['requests']:
        if r['unsafe'] and r['module'] == 'mulib.clamp':
            continue
        if r['unsafe'] or r['module'] not in ('mulib.clamp', 'mulib.histprice', 'mulib.priceopt'):
            code = 'UNSAFE-UNSUPPORTED' if r['unsafe'] else 'NOT-ADMITTED'
            raise ValueError(f"{r['file']}:{r['line']}:{r['column']}: ON1X-KERNEL-{code}: {r['module']}")
        modules.setdefault(r['module'], []).append(r)
    if not modules:
        raise ValueError('ON1X-KERNEL-NOT-ADMITTED: review mode requires at least one safe kernel import')
    interfaces = {}
    for name, requests in sorted(modules.items()):
        scoped = dict(report, requests=requests)
        interfaces[name] = (history_interface(scoped) if name == 'mulib.histprice'
                            else pricing_interface(scoped) if name == 'mulib.priceopt'
                            else clamp_scalar_interface(scoped, unsafe=False))
    if len(interfaces) == 1:
        return next(iter(interfaces.values()))
    return dict(schema='on1x.kernel-interface-set/v1', profile='reviewed-kernel-set/v1',
                target=report['target'], safe_export=False, modules=interfaces)


def interface_members(interface):
    return interface['modules'] if 'modules' in interface else {interface['module']: interface}


def review_candidate(report):
    """Describe what an independent reviewer must approve; does not admit anything."""
    interface = reviewed_interface(report)
    members = interface_members(interface)
    if len(members) > 1:
        return dict(schema='on1x.kernel-review-subject-set/v1', profile=interface['profile'],
                    modules={name: review_subject(report, item) for name, item in members.items()})
    return review_subject(report, interface)


def review_subject(report, interface):
    module = report['modules'][interface['module']]
    sidecar = module['contract']
    if not sidecar:
        raise ValueError('ON1X-KERNEL-NOT-ADMITTED: no validated ExoML contract')
    functions = {f['c_name']: f for f in sidecar['functions']}
    expected_return = dict(kind='value', nullable=False, nullable_means_success=False,
                           alias_of='', allocation_source='', deallocator_family='', destroy_with='')
    expected_errors = dict(kind='none', cleanup_required_on_failure=False, error_parameter='',
                           out_valid_on_failure=False, sentinel_value='')
    expected_concurrency = dict(thread_safety='thread_safe', blocking_behavior='nonblocking',
                                callback_origin='', registration_thread_affine=False, signal_safe=False,
                                suspension_safe=True)
    scalar_param = dict(ownership='scalar', nullable=False, alignment=None,
                        length_parameter=None, count_parameter=None, encoding='', transfer_predicate='')
    history = interface['module'] == 'mulib.histprice'
    pricing = interface['module'] == 'mulib.priceopt'
    symbols = HISTORY_EXPORTS if history else PRICING_EXPORTS if pricing else CLAMP_EXPORTS
    if set(functions) != set(symbols) or len(sidecar['functions']) != len(symbols):
        raise ValueError('ON1X-KERNEL-NOT-REPRESENTABLE: unexpected ExoML export set')
    for name, f in functions.items():
        errors = expected_errors
        effects = []
        if history or pricing:
            if history:
                names = ['prices', 'n', 'alpha', *(['beta', 'phi'] if name.endswith('_holt') else []), 'horizon', 'out']
                borrows = [(0, 1)]
            elif name.endswith('_candidates'):
                names = ['prices', 'predicted_units', 'n', 'constraints', 'objective', 'out']
                borrows = [(0, 2), (1, 2), (3, None)]
            else:
                names = ['model', 'constraints', 'objective', 'out']
                borrows = [(0, None), (1, None)]
            params = [dict(scalar_param, index=i, name=p) for i, p in enumerate(names)]
            lifetime = dict(origin='explicit_region', parameter_index=0, region_name='call')
            for index, length in borrows:
                params[index].update(ownership='borrowed_immutable', lifetime=lifetime, length_parameter=length, alignment=8)
            params[-1].update(ownership='out', lifetime=lifetime, alignment=8)
            errors = dict(expected_errors, kind='boolean_return')
            effects = ['read', 'write']
        else:
            params = [dict(scalar_param, index=i, name=p) for i, p in enumerate(CLAMP_PARAMETERS[name])]
        if (set(f) - {'c_name', 'return', 'parameters', 'errors', 'effects', 'concurrency',
                     'callbacks', 'destructors', 'transfers', 'invalidations', 'provenance'}
            or f['return'] != expected_return or f['parameters'] != params or f['errors'] != errors
            or f['effects'] != dict(effects=effects, unknown=False) or f['concurrency'] != expected_concurrency
            or any(f.get(k, []) != [] for k in ('callbacks', 'destructors', 'transfers', 'invalidations'))):
            raise ValueError('ON1X-KERNEL-NOT-REPRESENTABLE: nontrivial or unknown ExoML obligations for '+name)
    if set(report['cflags']) - KERNEL_CODEGEN_FLAGS:
        raise ValueError('ON1X-KERNEL-ABI: unsupported flags for reviewed C11')
    cdecl = module['metadata']['cdecl']
    # Build-tool and input identity is exact. A reviewer does not sign the
    # importing program or the aliases: only the foreign interface and its TCB.
    excluded_paths = set(report['source_inputs'])
    if excluded_paths & {item['path'] for item in cdecl['inputs']}:
        raise ValueError('ON1X-KERNEL-ABI: overlapping application and review inputs')
    inputs = {k: v for k, v in report['inputs'].items() if k not in excluded_paths}
    def fingerprint(value):
        return hashlib.sha256(contracts.dumps(value).encode('ascii')).hexdigest()
    return dict(schema='on1x.kernel-review-subject/v1', profile=interface['profile'],
                module=interface['module'], header=interface['header'], header_revision=interface['header_revision'],
                target=cdecl['target'], compiler=report['compiler'], compiler_version=report['compiler_version'],
                cflags=report['cflags'], inputs=inputs, cdecl_fingerprint=cdecl['fingerprint'],
                cdecl_sha256=fingerprint(cdecl), exolib_sha256=fingerprint(module['exolib']),
                contract_sha256=fingerprint(sidecar), contract_path=module['sidecar'],
                symbols=interface['exports'] if history or pricing else {
                    name: {k: entry[k] for k in ('c_name', 'c_type_id', 'declaration_id', 'precondition') if k in entry}
                    for name, entry in interface['exports'].items()})


def authorize_review(report, key, review, signature, stage):
    """Verify evidence using the operator-selected Ed25519 trust anchor."""
    def reject(reason):
        request = report['requests'][0]
        raise ValueError(f"{request['file']}:{request['line']}:{request['column']}: "
                         'ON1X-KERNEL-NOT-ADMITTED: '+reason)
    raw = contracts.read(review, 1024*1024)
    public = contracts.read(key, 1024)
    sig = contracts.read(signature, 1024)
    # Only DER SubjectPublicKeyInfo for Ed25519; reject alternate algorithms,
    # private keys, trailing data and ambiguous key encodings.
    if len(public) != 44 or not public.startswith(bytes.fromhex('302a300506032b6570032100')) or len(sig) != 64:
        reject('expected an Ed25519 DER public key and 64-byte detached signature')
    evidence = contracts.loads(raw)
    if (not isinstance(evidence, dict) or set(evidence) != {'schema', 'reviewer', 'review_reference', 'subject'}
        or evidence['schema'] != 'on1x.kernel-review/v1'
        or not isinstance(evidence['subject'], dict)
        or not all(isinstance(evidence[k], str) and 0 < len(evidence[k].strip()) <= 1024
                   for k in ('reviewer', 'review_reference'))):
        reject('invalid independent review schema')
    for name, data in [('review.ed25519.der', public), ('review.json', raw), ('review.sig', sig)]:
        (stage/name).write_bytes(data)
    openssl = executable('openssl')
    digest = contracts.digest(openssl)
    verified = subprocess.run([openssl, 'pkeyutl', '-verify', '-pubin', '-keyform', 'DER',
                               '-inkey', stage/'review.ed25519.der', '-rawin', '-in', stage/'review.json',
                               '-sigfile', stage/'review.sig'], capture_output=True, timeout=30)
    if verified.returncode or contracts.digest(openssl) != digest:
        reject('independent review signature verification failed')
    if contracts.dumps(evidence['subject']) != contracts.dumps(review_candidate(report)):
        reject('independent review does not match the current binding inputs')
    return dict(**evidence, signature_hex=sig.hex(), key_der_hex=public.hex(),
                review_sha256=hashlib.sha256(raw).hexdigest(), signed_review_hex=raw.hex(),
                verifier=str(openssl), verifier_sha256=digest)


def clamp_adapter(interface):
    output = ['#include <mulib-clamp.h>',
              '#if !defined(__linux__) || !defined(__x86_64__) || !defined(__LP64__)',
              '#error "KernelBind scalar profile requires native Linux x86-64"', '#endif',
              '_Static_assert(CHAR_BIT == 8 && sizeof(int64_t) == 8 && _Alignof(int64_t) == 8'
              ' && sizeof(void *) == 8 && INT64_MIN == -INT64_MAX - 1, "KernelBind target ABI drift");']
    for export in interface['exports'].values():
        symbol = export['c_name']
        names = CLAMP_PARAMETERS[symbol]
        types = ', '.join('int64_t' for _ in names)
        params = ', '.join('int64_t '+name for name in names)
        output.extend([
            f'_Static_assert(_Generic(&{symbol}, int64_t (*)({types}): 1, default: 0), '
            '"KernelBind prototype drift");',
            f'int64_t {CLAMP_ADAPTER_PREFIX}{symbol}({params}) {{ return {symbol}({", ".join(names)}); }}',
        ])
    return '\n'.join(output)+'\n'


def clamp_probe():
    """Independent integer oracle, compiled against the exact generated adapter."""
    import random
    low, high = -(1 << 63), (1 << 63)-1
    edges = [low, low+1, -2, -1, 0, 1, 2, high-1, high]
    pairs = [(a, b) for a in edges for b in edges]
    rng = random.Random(0xC1A0)
    pairs += [(rng.randrange(low, high+1), rng.randrange(low, high+1)) for _ in range(256)]
    intervals = [(value, lo, hi) for value in edges for lo in edges for hi in edges if lo <= hi]
    for _ in range(256):
        value, a, b = [rng.randrange(low, high+1) for _ in range(3)]
        intervals.append((value, min(a, b), max(a, b)))
    def literal(n):
        if n == low:
            return 'INT64_MIN'
        return ('-' if n < 0 else '')+f'INT64_C({abs(n)})'
    cases = [(symbol, (a, b), min(high, max(low, expected)))
             for a, b in pairs for symbol, expected in zip(CLAMP_EXPORTS[:2], (a+b, a-b))]
    cases += [('mulib_clamp_i64', (value, lo, hi), min(hi, max(lo, value)))
              for value, lo, hi in intervals]
    if 2 * len(cases) != CLAMP_PROBE_COMPARISONS:
        raise ValueError('KernelBind clamp differential case-count drift')
    output = ['#include <stdint.h>', '#include <mulib-clamp.h>']
    for symbol, names in CLAMP_PARAMETERS.items():
        types = ', '.join('int64_t' for _ in names)
        output.append(f'extern int64_t {CLAMP_ADAPTER_PREFIX}{symbol}({types});')
    output.append('int main(void) {')
    for symbol, values, expected in cases:
        args = ', '.join(map(literal, values))
        output.append(f'if ({CLAMP_ADAPTER_PREFIX}{symbol}({args}) != {literal(expected)}'
                      f' || {symbol}({args}) != {literal(expected)}) return 1;')
    output += ['return 0;', '}']
    return '\n'.join(output)+'\n'


def history_adapter(interface):
    source = [
        '#include <mulib-histprice.h>', '#include <float.h>', '#include <limits.h>',
        '#include <math.h>', '#include <stdbool.h>', '#include <stdint.h>',
        '#if !defined(__linux__) || !defined(__x86_64__) || !defined(__LP64__) || defined(__FAST_MATH__)',
        '#error "unsupported KernelBind history ABI"', '#endif',
        '_Static_assert(CHAR_BIT == 8 && sizeof(int) == 4 && _Alignof(int) == 4 && INT_MAX == 2147483647, "status ABI");',
        '_Static_assert(sizeof(size_t) == 8 && _Alignof(size_t) == 8 && SIZE_MAX == UINT64_MAX, "size ABI");',
        '_Static_assert(sizeof(double) == 8 && _Alignof(double) == 8 && FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024, "binary64 ABI");',
        '_Static_assert(sizeof(void *) == 8 && sizeof(int64_t) == 8 && _Alignof(int64_t) == 8, "adapter ABI");',
        '_Static_assert(sizeof(mulib_histprice_forecast_t) == 16 && _Alignof(mulib_histprice_forecast_t) == 8, "forecast layout");',
        '_Static_assert(offsetof(mulib_histprice_forecast_t, point) == 0 && offsetof(mulib_histprice_forecast_t, variance) == 8, "forecast offsets");',
        '_Static_assert(_Generic(&((mulib_histprice_forecast_t *)0)->point, double *:1, default:0) && _Generic(&((mulib_histprice_forecast_t *)0)->variance, double *:1, default:0), "forecast fields");',
    ]
    for export in interface['exports'].values():
        symbol = export['c_name']
        holt = symbol.endswith('_holt')
        extra = 'double, double, ' if holt else ''
        source += [
            f'_Static_assert(_Generic(&{symbol}, int (*)(const double *, size_t, double, {extra}size_t, mulib_histprice_forecast_t *):1, default:0), "forecast prototype");',
            f'bool {CLAMP_ADAPTER_PREFIX}{symbol}(const double *backing, int64_t capacity, int64_t start, int64_t n, double alpha, '
            + ('double beta, double phi, ' if holt else '')+'int64_t horizon, double *point, double *variance) {',
            '  if (!backing || !point || !variance || capacity < 0 || capacity > 256 || start < 0 || start > capacity ||'
            f' n < {2 if holt else 1} || n > capacity - start || horizon <= 0 || horizon > 1000000) return false;',
            '  if (!(alpha >= 0.0 && alpha <= 1.0)) return false;',
        ]
        if holt:
            source.append('  if (!(beta >= 0.0 && beta <= 1.0 && phi > 0.0 && phi <= 1.0)) return false;')
        source += [
            '  const double *samples = backing + (size_t)start;',
            '  for (size_t i = 0; i < (size_t)n; ++i) if (!isfinite(samples[i]) || samples[i] < 0) return false;',
            '  mulib_histprice_forecast_t output = {NAN, NAN};',
            f'  if ({symbol}(samples, (size_t)n, alpha, '+('beta, phi, ' if holt else '')+
            '(size_t)horizon, &output) != 1) return false;',
            '  if (!isfinite(output.point) || output.point < 0 || !isnan(output.variance)) return false;',
            '  *point = output.point;',
            '  *variance = output.variance;',
            '  return true;', '}',
        ]
    return '\n'.join(source)+'\n'


def history_probe():
    return '''#include <mulib-histprice.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
extern bool on1x_kernel_scalar_v1_mulib_histprice_ema(const double *, int64_t, int64_t, int64_t, double, int64_t, double *, double *);
extern bool on1x_kernel_scalar_v1_mulib_histprice_holt(const double *, int64_t, int64_t, int64_t, double, double, double, int64_t, double *, double *);
int main(void) {
    const double prices[] = {NAN, 10, 12, 14, NAN};
    double point = -1, variance = -1;
    mulib_histprice_forecast_t direct;
    if (!mulib_histprice_ema(prices+1, 3, 0.0, 2, &direct)) return 1;
    if (!on1x_kernel_scalar_v1_mulib_histprice_ema(prices, 5, 1, 3, 0.0, 2, &point, &variance)
        || point != 10 || point != direct.point || !isnan(variance) || !isnan(direct.variance)) return 2;
    if (!mulib_histprice_holt(prices+1, 3, .5, .2, 1.0, 2, &direct)) return 3;
    if (!on1x_kernel_scalar_v1_mulib_histprice_holt(prices, 5, 1, 3, .5, .2, 1.0, 2, &point, &variance)
        || point != 18 || point != direct.point || !isnan(variance) || !isnan(direct.variance)) return 4;
    point = -1; variance = -2;
    if (on1x_kernel_scalar_v1_mulib_histprice_ema(prices, 5, 0, 5, .5, 1, &point, &variance)
        || on1x_kernel_scalar_v1_mulib_histprice_ema(prices, 5, 1, 0, .5, 1, &point, &variance)
        || on1x_kernel_scalar_v1_mulib_histprice_ema(prices, 5, 1, 3, .5, INT64_MAX, &point, &variance)
        || on1x_kernel_scalar_v1_mulib_histprice_ema(prices, 5, 1, 3, .5, -1, &point, &variance)
        || on1x_kernel_scalar_v1_mulib_histprice_ema(prices, 5, 1, 3, INFINITY, 1, &point, &variance)
        || on1x_kernel_scalar_v1_mulib_histprice_ema(prices, 5, INT64_MAX, INT64_MAX, .5, 1, &point, &variance)
        || on1x_kernel_scalar_v1_mulib_histprice_holt(prices, 5, 1, 1, .5, .2, 1., 1, &point, &variance)
        || on1x_kernel_scalar_v1_mulib_histprice_holt(prices, 5, 1, 3, .5, .2, 0., 1, &point, &variance)
        || point != -1 || variance != -2) return 5;
    return 0;
}
'''


def pricing_adapter(interface):
    source = ['#include <mulib-priceopt.h>', '#include <float.h>', '#include <limits.h>',
              '#include <math.h>', '#include <stdbool.h>', '#include <stdint.h>',
              '#if !defined(__linux__) || !defined(__x86_64__) || !defined(__LP64__) || defined(__FAST_MATH__)',
              '#error "unsupported KernelBind pricing ABI"', '#endif',
              '_Static_assert(CHAR_BIT == 8 && sizeof(int) == 4 && _Alignof(int) == 4 && INT_MAX == 2147483647, "status ABI");',
              '_Static_assert(sizeof(size_t) == 8 && _Alignof(size_t) == 8 && SIZE_MAX == UINT64_MAX, "size ABI");',
              '_Static_assert(sizeof(double) == 8 && _Alignof(double) == 8 && FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024, "binary64 ABI");']
    for name, values in PRICING_ENUMS.items():
        ty = f'mulib_priceopt_{name}_t'
        source += [f'_Static_assert(sizeof({ty}) == 4 && _Alignof({ty}) == 4 &&'
                   f' _Generic(({ty})0, unsigned int:1, default:0), "enum ABI");']
        for i, value in enumerate(values):
            source += [f'_Static_assert(MULIB_PRICEOPT_{value} == {i}, "enum value drift");']
    for name, fields in PRICING_RECORDS.items():
        ty = f'mulib_priceopt_{name}_t'
        source += [f'_Static_assert(sizeof({ty}) == {len(fields)*8} && _Alignof({ty}) == 8, "record ABI");']
        for i, (field, spelling) in enumerate(fields):
            expect = f'mulib_priceopt_{spelling}_t' if spelling in PRICING_ENUMS else spelling
            source += [f'_Static_assert(offsetof({ty}, {field}) == {i*8} &&'
                       f' _Generic(&(({ty} *)0)->{field}, {expect} *:1, default:0), "record field drift");']
    # The only implementation switch is in the planner's separate TU.
    for export in interface['exports'].values():
        symbol = export['c_name']
        candidates = symbol.endswith('_candidates')
        prefix = 'const double *, const double *, size_t' if candidates else 'const mulib_priceopt_demand_model_t *'
        source += [f'_Static_assert(_Generic(&{symbol}, int (*)({prefix}, const mulib_priceopt_constraints_t *,'
                   ' mulib_priceopt_objective_t, mulib_priceopt_result_t *):1, default:0), "pricing prototype");']
        prefix = ('const double *prices, int64_t pcap, int64_t ps, int64_t pn, '
                  'const double *units, int64_t ucap, int64_t us, int64_t un' if candidates else
                  'int64_t tag, double reference, double scale, double shape')
        source += [
            f'bool {CLAMP_ADAPTER_PREFIX}{symbol}({prefix}, double cap, double change, double floor, double margin, '
            'double previous, double tick, double cost, int64_t objective, double *demand, double *price,'
            ' double *score, uint64_t *evaluations) {',
            '  if (!demand || !price || !score || !evaluations) return false;',
            '  mulib_priceopt_objective_t selected;',
            '  switch (objective) {',
            '  case 0: selected = MULIB_PRICEOPT_PROFIT; break;',
            '  case 1: selected = MULIB_PRICEOPT_REVENUE; break;',
            '  default: return false;',
            '  }',
            '  const mulib_priceopt_constraints_t limits = {.min_price=floor, .max_price=cap, .unit_cost=cost,'
            ' .min_unit_margin=margin, .previous_price=previous, .max_relative_change=change, .tick=tick};',
        ]
        if candidates:
            source += [
                '  if (!prices || !units || pcap < 0 || pcap > 256 || ucap < 0 || ucap > 256 ||'
                ' ps < 0 || ps > pcap || us < 0 || us > ucap || pn <= 0 || pn != un || pn > pcap - ps || un > ucap - us) return false;',
            ]
            first = 'prices + (size_t)ps, units + (size_t)us, (size_t)pn'
        else:
            source += [
                '  mulib_priceopt_demand_kind_t kind;',
                '  switch (tag) {',
                '  case 0: kind = MULIB_PRICEOPT_ISOELASTIC; break;',
                '  case 1: kind = MULIB_PRICEOPT_LINEAR; break;',
                '  case 2: kind = MULIB_PRICEOPT_LOGISTIC; break;',
                '  default: return false;',
                '  }',
                '  const mulib_priceopt_demand_model_t model = {.kind=kind, .scale=scale, .shape=shape, .reference=reference};',
            ]
            first = '&model'
        source += [
            '  mulib_priceopt_result_t out = {NAN, NAN, NAN, 0};',
            f'  if ({symbol}({first}, &limits, selected, &out) != 1) return false;',
            '  if (!isfinite(out.price) || !isfinite(out.score) || !isfinite(out.expected_demand) || !out.evaluations) return false;',
            '  *demand = out.expected_demand; *price = out.price; *score = out.score; *evaluations = out.evaluations;',
            '  return true;',
            '}',
        ]
    return '\n'.join(source)+'\n'


def pricing_probe():
    return '''#include <mulib-priceopt.h>
#include <stdbool.h>
#include <stdint.h>
extern bool on1x_kernel_scalar_v1_mulib_priceopt_optimize(int64_t, double, double, double, double, double, double, double, double, double, double, int64_t, double *, double *, double *, uint64_t *);
extern bool on1x_kernel_scalar_v1_mulib_priceopt_candidates(const double *, int64_t, int64_t, int64_t, const double *, int64_t, int64_t, int64_t, double, double, double, double, double, double, double, int64_t, double *, double *, double *, uint64_t *);
int main(void) {
    const mulib_priceopt_constraints_t limits = {.min_price=6, .max_price=20, .unit_cost=4, .tick=1};
    const mulib_priceopt_demand_model_t model = {.kind=MULIB_PRICEOPT_LINEAR, .scale=20, .shape=1};
    mulib_priceopt_result_t direct;
    double demand, price, score;
    uint64_t evaluations;
    if (!mulib_priceopt_optimize(&model, &limits, MULIB_PRICEOPT_PROFIT, &direct) ||
        !on1x_kernel_scalar_v1_mulib_priceopt_optimize(1, 0, 20, 1, 20, 0, 6, 0, 0, 1, 4, 0,
         &demand, &price, &score, &evaluations) || price != 12 || score != 64 ||
        price != direct.price || score != direct.score || demand != direct.expected_demand ||
        evaluations != direct.evaluations) return 1;
    const double p[]={-999, 10, 12, 15}, q[]={12, 9, 4};
    if (!mulib_priceopt_candidates(p+1, q, 3, &limits, MULIB_PRICEOPT_PROFIT, &direct) ||
        !on1x_kernel_scalar_v1_mulib_priceopt_candidates(p,4,1,3,q,3,0,3,20,0,6,0,0,1,4,0,
         &demand,&price,&score,&evaluations) || price != 10 ||
        price != direct.price || score != direct.score || demand != direct.expected_demand ||
        evaluations != direct.evaluations) return 2;
    price=-1; demand=-2; score=-3; evaluations=4;
    if (on1x_kernel_scalar_v1_mulib_priceopt_optimize(3,0,20,1,20,0,6,0,0,1,4,0,&demand,&price,&score,&evaluations) ||
        on1x_kernel_scalar_v1_mulib_priceopt_optimize(1,0,20,1,6,0,20,0,0,1,4,0,&demand,&price,&score,&evaluations) ||
        on1x_kernel_scalar_v1_mulib_priceopt_candidates(p,4,1,3,q,3,0,2,20,0,6,0,0,1,4,0,&demand,&price,&score,&evaluations) ||
        price!=-1 || demand!=-2 || score!=-3 || evaluations!=4) return 3;
    return 0;
}
'''


def reviewed_profile_tools(name):
    """Closure, adapter and executable boundary test for a reviewed profile."""
    return {
        'mulib.clamp': (['MULIB_IMPL_CLAMP'], clamp_adapter, clamp_probe, 'clamp-boundaries-seeded/v2'),
        'mulib.histprice': (HISTORY_IMPL, history_adapter, history_probe, 'histprice-differential-boundaries/v1'),
        'mulib.priceopt': (PRICING_IMPL, pricing_adapter, pricing_probe, 'priceopt-differential-boundaries/v1'),
    }[name]


def build(args, root):
    recipe = plan(root, args.libraries, args.fixture,args.paths)
    housekeeping.check_generated(root, recipe)
    output = args.output.absolute()
    if output.exists() or output.is_symlink():
        raise ValueError('output already exists')
    exotract, compiler, ar, nm = map(executable, [args.exotract, args.compiler, args.ar, args.nm])
    # An implementation macro in command-line flags would corrupt declaration
    # discovery and bypass the exactly-one-TU model.
    if any('MULIB_IMPL_' in f for f in args.cflag):
        raise ValueError('implementation macros belong to the generated TU')
    # Exotract accepts only ABI switches in compiler_arguments. Definitions use
    # its dedicated API; these specific instrumentation options affect only
    # archive code generation and are separately recorded in the lock.
    codegen_only={'-g','-g0','-g1','-g2','-g3','-fsanitize=address,undefined',
                  '-fno-omit-frame-pointer','-fno-sanitize-recover=all'}
    discovery=[]
    for flag in args.cflag:
        if flag in codegen_only:continue
        if flag.startswith('-D') and len(flag)>2:discovery += ['--define',flag[2:]]
        elif flag.startswith('-U') and len(flag)>2:discovery += ['--undefine',flag[2:]]
        else:discovery += ['--compile-arg',flag]
    resource = Path(args.resource_dir or invoke([compiler, '-print-resource-dir']).decode().strip()).resolve()
    if not (resource/'include/stddef.h').is_file():
        raise ValueError('provide --resource-dir from a compatible clang')
    compiler_target = invoke([compiler, '-dumpmachine']).decode().strip()
    observed = {}
    def observe(path, expected=None):
        real = Path(path).resolve(); key = str(real); digest = contracts.digest(real)
        if expected and digest != expected or key in observed and observed[key] != digest:
            raise ValueError('input drift: '+key)
        observed[key] = digest
        return real
    for path in [CATALOG, housekeeping.CATALOG, Path(housekeeping.__file__), root/'MANIFEST.yaml', Path(__file__), HOME/'tools/on1x-contracts.py', exotract, compiler, ar, nm]:
        observe(path)
    for path,digest in recipe['inputs'].items():observe(path,digest)
    for entry in recipe['modules']:
        observe(housekeeping.header_path(root,entry), entry['header_sha256'])
        observe(root/entry['manual'], entry['manual_sha256'])
    for record in recipe['files']:observe(root/record['path'], record['sha256'])
    for name in args.libraries:
        if name in contracts.load(CATALOG)['profiles'] and contracts.load(CATALOG)['profiles'][name]['recipe']=='exoml-core':
            for path in contracts.CONTRACTS.iterdir():
                if path.is_file():observe(path)
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.exotract-', dir=output.parent))
    try:
        housekeeping.materialize(root, recipe, stage)
        imports={}
        headers={m['name']:housekeeping.header_path(root,m) for m in recipe['modules']}
        sandbox_profiles = {name: profile for name in headers if (profile := sandbox_profile(name))}
        requests=[(name,headers[name]) for name in args.libraries if name not in sandbox_profiles]
        requests += [('fixture-'+Path(path).parent.name+'-'+Path(path).stem,root/path) for path in recipe['include_headers']]
        for name, profile in sandbox_profiles.items():
            evidence = extract_exolib(name, plan(root, [name], paths=args.paths), root, exotract, compiler,
                                       resource, compiler_target, kernel_options(args.cflag), observe,
                                       sidecar_name=profile['sandbox_contract'], required=[])
            (stage/(name+'.sandbox-evidence.json')).write_text(contracts.dumps(evidence))
            (stage/(name+'.on1x-import.json')).write_text(contracts.dumps(evidence['metadata']))
            (stage/(name+'.cdecl.json')).write_text(contracts.dumps(evidence['metadata']['cdecl']))
            imports[name] = list(evidence['symbols'])
        for name,header in requests:
            common=[exotract, '--name', name, '--header', header,
                    '--compiler', compiler, '--resource-dir', resource, '--preprocessor', 'compiler',
                    '--revision', 'sha256:'+contracts.digest(header), '--provenance', root/'MANIFEST.yaml',
                    '--provenance', CATALOG]
            for directory in recipe['include_directories']:common += ['--include',directory]
            common += discovery
            profile = contracts.load(CATALOG)['profiles'].get(name, {})
            if profile.get('recipe') == 'exoml-core':
                core = contracts.catalog(root)['profiles'][name]
                for path in core.get('evidence_sha256', {}):
                    common += ['--provenance', root/path]
            for symbol in recipe['required_symbols'].get(name,[]):common += ['--require-symbol', symbol]
            try:
                wire=invoke([common[0], 'on1x', *common[1:]])
            except ValueError as error:
                # The upstream on1x command intentionally emits no incomplete
                # import manifest. Inspect retains the detailed diagnostics.
                diagnosis=subprocess.run([str(v) for v in [common[0], 'inspect', *common[1:]]],capture_output=True,timeout=180)
                raise ValueError(str(error)+'\n'+diagnosis.stdout[:65536].decode(errors='replace')) from error
            manifest=contracts.loads(wire)
            if manifest.get('schema')!='moonstone.exotract.on1x/2' or manifest.get('unsafe_by_default') is not True:
                raise ValueError('invalid Exotract On1x manifest')
            decl=manifest['cdecl']
            if decl.get('schema_version')!=2 or decl.get('complete') is not True or decl.get('header')!=str(header):
                raise ValueError('incomplete/mismatched CDecl')
            for item in decl['inputs']:observe(item['path'], item['sha256'])
            (stage/(name+'.on1x-import.json')).write_bytes(wire)
            (stage/(name+'.cdecl.json')).write_text(contracts.dumps(decl))
            imports[name]=[entry['c_name'] for entry in manifest['imports']]
        # All selected modules share one implementation TU, avoiding duplicate
        # definitions when several requested libraries depend on the same kernel.
        last=recipe['modules'][-1]
        command=[exotract,'emit-source','--name','MuLIB','--header',housekeeping.header_path(root,last)]
        for macro in recipe['implementation_macros']:command += ['--impl-define',macro]
        for entry in recipe['modules'][:-1]:command += ['--dependency',housekeeping.header_path(root,entry)]
        for header in recipe['include_headers']:command += ['--dependency',root/header]
        definitions=''.join('#define '+item.replace('=', ' ', 1)+'\n' for item in recipe['compile_definitions'])
        (stage/'mulib.c').write_bytes(definitions.encode()+invoke(command))
        (stage/'mulib.h').write_text('/* Declaration-only static binding surface. */\n#pragma once\n'+
            ''.join('#include <mulib-'+name+'.h>\n' for name in args.libraries)+
            ''.join('#include '+json.dumps(path)+'\n' for path in recipe['include_headers']))
        strict = ['-Wall', '-Wextra', '-Wpedantic', '-Werror'] if sandbox_profiles else []
        flags=[*args.cflag,*strict,*[part for p in recipe['include_directories'] for part in ['-I',p]],
               *['-D'+item for item in recipe['compile_definitions']]]
        objects=[]
        for i,source in enumerate([stage/'mulib.c',*[root/path for path in recipe['native_sources']]]):
            obj=stage/('mulib.o' if i==0 else 'fixture_'+str(i)+'.o')
            for path,digest in housekeeping.compile_inputs(compiler,flags,source).items():
                if not Path(path).is_relative_to(stage):observe(path,digest)
            invoke([compiler,'-std=c11',*flags,'-c',source,'-o',obj]);objects.append(obj)
        invoke([ar,'rcsD',stage/'libmulib.a',*objects])
        # Header declarations can include optional APIs whose implementation
        # switch is not in this recipe. Record actual linker exports separately
        # while preserving Exotract's complete upstream metadata unmodified.
        symbols=''.join(invoke([nm,'--extern-only','--defined-only','--format=posix',obj]).decode() for obj in objects)
        exports=set()
        for line in symbols.splitlines():
            fields=line.split()
            if len(fields)<2 or len(fields[1])!=1:
                raise ValueError('unsupported nm output')
            exports.add(fields[0])
        surfaces={name:dict(archive_exports=sorted(set(names)&exports),
                           declaration_only=sorted(set(names)-exports))
                  for name,names in imports.items()}
        for name,required in recipe['required_symbols'].items():
            if not set(required)<=exports:raise ValueError('required archive symbols missing: '+name)
        for name, profile in sandbox_profiles.items():
            if not set(profile['required_symbols']) <= exports:
                raise ValueError('required sandbox archive symbols missing: '+name)
        (stage/'symbols.json').write_text(contracts.dumps(dict(schema='on1x.exotract-symbols/v1',
            libraries=surfaces, defined_symbols=sorted(exports), safe_export=False)))
        (stage/'build-plan.json').write_text(contracts.dumps(recipe))
        for path,digest in observed.items():
            if contracts.digest(path)!=digest:raise ValueError('input changed before publication: '+path)
        artifacts={p.relative_to(stage).as_posix():contracts.digest(p) for p in sorted(stage.rglob('*')) if p.is_file()}
        (stage/'lock.json').write_text(contracts.dumps(dict(schema='on1x.exotract-lock/v1',
            safe_export=False, inputs=observed, artifacts=artifacts, compiler=str(compiler),
            compiler_version=invoke([compiler,'--version']).decode(), cflags=args.cflag,
            compiler_target=compiler_target,
            declaration_options=discovery,
            resource_directory=str(resource),
            **(dict(sandbox_compile_flags=strict, sandbox_profiles=list(sandbox_profiles)) if sandbox_profiles else {}))))
        contracts.publish_new(stage,output)
    finally:
        if stage.exists():shutil.rmtree(stage)


def main():
    cli=argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--root',type=Path)
    cli.add_argument('--mulib-path',action='append',default=[],metavar='DIR:DIR')
    sub=cli.add_subparsers(dest='mode',required=True)
    sub.add_parser('list')
    p=sub.add_parser('plan');p.add_argument('libraries',nargs='+');p.add_argument('--fixture',action='append',default=[])
    b=sub.add_parser('build');b.add_argument('libraries',nargs='+');b.add_argument('--output',type=Path,required=True)
    b.add_argument('--exotract',required=True);b.add_argument('--compiler',default='clang');b.add_argument('--ar',default='ar');b.add_argument('--nm',default='nm')
    b.add_argument('--resource-dir');b.add_argument('--cflag',action='append',default=[])
    b.add_argument('--fixture',action='append',default=[])
    args=cli.parse_args()
    try:
        args.paths=housekeeping.search_paths(args.mulib_path)
        root=housekeeping.kernel_root(args.root,HOME.parent,args.paths)
        if args.mode=='list':
            print(contracts.dumps(dict(schema='on1x.exotract-inventory/v1', **contracts.inventory(root))),end='')
        elif args.mode=='plan':print(contracts.dumps(plan(root,args.libraries,args.fixture,args.paths,discovery=True)),end='')
        else:build(args,root);print(args.output)
    except (ValueError,OSError,KeyError,subprocess.TimeoutExpired) as error:
        print('on1x-exotract: '+str(error),file=sys.stderr);return 1
    return 0


if __name__=='__main__':sys.exit(main())
