#!/usr/bin/env python3
"""Source-backed On1x contract profiles; developer tooling, never a safe importer.

Snapshots use real on1x-bind/ExoML discovery, retain full contracts and pin every
input/artifact. Recheck requires an explicitly authorized --bind executable;
never execute a program path obtained from a lock file.
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

HOME = Path(__file__).resolve().parents[1]
CONTRACTS = HOME/'contracts-exoml'
EXOTRACT_CATALOG = HOME/'contracts-exotract/catalog.json'
KERNEL_AUTO_CATALOG = HOME/'contracts-exotract/kernel-auto.json'
MAX_BYTES = 64 * 1024 * 1024


def reject(message):
    raise ValueError(message)


def read(path, limit=MAX_BYTES):
    path = Path(path)
    if not path.is_file() or path.stat().st_size > limit:
        reject(f'not a regular bounded input: {path}')
    with path.open('rb') as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        reject(f'input byte limit exceeded: {path}')
    return data


def digest(path):
    return hashlib.sha256(read(path)).hexdigest()


def unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            reject('duplicate JSON key: ' + key)
        result[key] = value
    return result


def loads(data):
    value = json.loads(data, object_pairs_hook=unique,
                       parse_constant=lambda x: reject('invalid JSON scalar: ' + x))
    pending = [(value, 0)]
    count = 0
    while pending:
        node, depth = pending.pop()
        count += 1
        if depth >= 64 or count > 1048576:
            reject('JSON depth/node limit exceeded')
        if isinstance(node, dict):
            pending.extend((child, depth + 1) for child in node.values())
        elif isinstance(node, list):
            pending.extend((child, depth + 1) for child in node)
    return value


def load(path, limit=MAX_BYTES):
    return loads(read(path, limit))


def dumps(value):
    return json.dumps(value, ensure_ascii=True, sort_keys=True, separators=(',', ':'), allow_nan=False) + '\n'


def keys(value, allowed):
    if not isinstance(value, dict) or set(value) - set(allowed):
        reject('invalid/unknown profile keys')


def contract_catalog():
    data = load(CONTRACTS/'catalog.json', 2*1024*1024)
    keys(data, ['schema', 'safe_export', 'inventory', 'profiles'])
    if (data['schema'] != 'on1x.contract-catalog/v1' or data['safe_export'] is not False
            or data['inventory'] != '../contracts-exotract/catalog.json'):
        reject('unsupported/unsafe catalog')
    return data


def exotract_catalog():
    data = load(EXOTRACT_CATALOG, 2*1024*1024)
    keys(data, ['schema', 'manifest', 'headers', 'implementation_translation_units',
                'safe_export', 'discovery_only_prefixes', 'profiles', 'default_recipe'])
    if (data['schema'] != 'on1x.exotract-catalog/v1' or data['safe_export'] is not False
            or data['manifest'] != 'MANIFEST.yaml' or data['headers'] != 'include/mulib-*.h'
            or data['default_recipe'] != 'manifest' or data['implementation_translation_units'] != 1
            or not isinstance(data['profiles'], dict) or not isinstance(data['discovery_only_prefixes'], dict)):
        reject('unsupported Exotract catalog')
    for prefix, reason in data['discovery_only_prefixes'].items():
        if not re.fullmatch(r'[a-z][a-z0-9]*-', prefix) or not isinstance(reason, str) or not reason:
            reject('invalid discovery-only kernel policy')
    if any(discovery_only_reason(name, data) for name in data['profiles']):
        reject('discovery-only kernel cannot have an archive profile')
    return data


def discovery_only_reason(name, data):
    return next((reason for prefix, reason in data['discovery_only_prefixes'].items()
                 if name.startswith(prefix)), None)


def kernel_auto_catalog(manifest):
    """Validate the generated declaration registry against the MANIFEST snapshot."""
    if KERNEL_AUTO_CATALOG.resolve() != KERNEL_AUTO_CATALOG:
        reject('symlinked automatic kernel catalog: '+str(KERNEL_AUTO_CATALOG))
    raw = read(KERNEL_AUTO_CATALOG, 2*1024*1024)
    data = loads(raw)
    keys(data, ['schema', 'safe_export', 'modules'])
    modules = data.get('modules')
    if (data.get('schema') != 'on1x.kernel-auto/v1' or data.get('safe_export') is not False
            or not isinstance(modules, dict)):
        reject('unsupported/unsafe automatic kernel catalog')
    if modules.keys() != manifest.keys():
        missing = sorted(manifest.keys() - modules.keys())
        extra = sorted(modules.keys() - manifest.keys())
        sample = (('missing '+missing[0]) if missing else ('unregistered '+extra[0]))
        reject('automatic kernel catalog/MANIFEST registration drift: '+sample+'; regenerate kernel-auto.json')
    for name, item in manifest.items():
        expected = {field: item[field] for field in ('header', 'status', 'implementation', 'optional_dependency')
                    if field in item}
        expected['dependencies'] = item['manifest_dependencies']
        if item['macro'] is not None:
            expected['macro'] = item['macro']
        if modules[name] != expected:
            reject('automatic kernel catalog/MANIFEST entry drift: '+name+'; regenerate kernel-auto.json')
    return modules, hashlib.sha256(raw).hexdigest()


def catalog(root, selected=None):
    data = contract_catalog()
    obligations = load(CONTRACTS/'obligations.json', 2*1024*1024)
    if obligations['schema'] != 'on1x.contract-obligations/v1' or obligations['safe_export'] is not False:
        reject('unsupported/unsafe obligations')
    for name, profile in data['profiles'].items():
        if selected is not None and name not in selected:
            continue
        keys(profile, ['contract', 'header', 'header_sha256', 'manual', 'manual_sha256', 'evidence_sha256', 'function_prefix', 'function_count', 'implementation_modules', 'active_optional_macros', 'inactive_optional_macros', 'link_libraries'])
        if not re.fullmatch('[a-z][a-z0-9_-]*', name):
            reject('invalid profile name')
        for field in ['header', 'manual']:
            path = Path(profile[field])
            if path.is_absolute() or '..' in path.parts or digest(root/path) != profile[field+'_sha256']:
                reject(f'{name}: source/manual fingerprint drift: {path}; audit before refreshing')
        evidence = profile.get('evidence_sha256', {})
        if not isinstance(evidence, dict) or len(evidence) > 64:
            reject(f'{name}: invalid source evidence inventory')
        for filename, expected in evidence.items():
            path = Path(filename)
            if (path.is_absolute() or '..' in path.parts or not path.parts
                    or not (root/path).resolve().is_relative_to(root.resolve())
                    or digest(root/path) != expected):
                reject(f'{name}: evidence fingerprint drift: {path}; audit before refreshing')
        if Path(profile['contract']).name != profile['contract']:
            reject('contract path must be local to contracts-exoml/')
        active = profile.get('active_optional_macros', [])
        optional = {macro for module in profile['implementation_modules']
                    for macro in module_info(root, module)['extra_macros']}
        if (not isinstance(active, list) or not all(isinstance(m, str) for m in active)
                or len(active) != len(set(active))
                or set(active) - optional or set(active) & set(profile['inactive_optional_macros'])):
            reject(f'{name}: invalid/conflicting explicit optional implementation macros')
        bundle = load(CONTRACTS/profile['contract'], 2*1024*1024)
        if bundle['schema_version'] != 2 or bundle['state'] != 'annotated' or bundle['library'] != name:
            reject('seed must be an annotated ExoML v2 profile')
        if bundle['bundle_provenance']['review_status'] != 'unreviewed' or bundle['bundle_provenance']['revision'] != 'sha256:'+profile['header_sha256']:
            reject('seed review/revision is not source-backed')
        functions = bundle['functions']
        names = [f['c_name'] for f in functions]
        if len(names) != profile['function_count'] or len(names) != len(set(names)) or not all(n.startswith(profile['function_prefix']) for n in names):
            reject(f'{name}: duplicate/missing/out-of-profile functions')
        for f in functions:
            if f['provenance']['review_status'] != 'unreviewed' or not f['provenance']['evidence_method']:
                reject('seed must not fabricate independent review')
            if not isinstance(f['effects']['unknown'], bool) or not f['concurrency']['blocking_behavior']:
                reject('effects/concurrency annotations must be explicit')
            if [p['index'] for p in f['parameters']] != list(range(len(f['parameters']))):
                reject('seed parameters must be contiguous and ordered')
    return data


def manifest_list(entry, key, token, *, optional=False):
    fields = list(re.finditer(r'^  '+key+r':([^\n]*)$', entry, re.M))
    if not fields and optional:
        return []
    if len(fields) == 1:
        field = fields[0]
        inline = re.fullmatch(r' +\[([^\]]*)\]', field[1])
        tail = entry[field.end():].removeprefix('\n').splitlines()
        block = []
        indent = None
        for line in tail:
            if not line.startswith(('   ', '  -')):
                break
            item = re.fullmatch(r'( {2}| {4})- (.+)', line)
            if not item or (indent is not None and item[1] != indent):
                reject('unsupported MANIFEST '+key+' layout')
            indent = item[1]
            block.append(item[2])
        if inline is not None and not block:
            values = [v.strip() for v in inline[1].split(',')] if inline[1].strip() else []
        elif not field[1] and block:
            values = block
        else:
            reject('unsupported MANIFEST '+key+' layout')
        if all(re.fullmatch(token, value) for value in values) and len(values) == len(set(values)):
            return values
    reject('unsupported MANIFEST '+key+' layout')


def manifest_scalar(entry, key, token, *, optional=False):
    fields = re.findall(r'^  '+key+r':([^\n]*)$', entry, re.M)
    if optional and not fields:
        return None
    if len(fields) != 1 or not fields[0].startswith(' ') or not re.fullmatch(token, fields[0].lstrip(' ')):
        reject('unsupported or duplicate MANIFEST field: '+key)
    return fields[0].lstrip(' ')


def module_fields(name, entry, *, registration=False):
    fields = {key: manifest_scalar(entry, key, r'[A-Za-z0-9_./+-]+') for key in ('header', 'manual')}
    macro = manifest_scalar(entry, 'macro', r'MULIB_IMPL_[A-Z0-9_]+', optional=True)
    implementation = manifest_scalar(entry, 'implementation', r'static-inline|static-data', optional=True)
    if (macro is None) == (implementation is None):
        reject('unsupported MANIFEST implementation: '+name)
    fields['macro'] = macro
    if implementation is not None:
        fields['implementation'] = implementation
    fields['manifest_dependencies'] = manifest_list(entry, 'dependencies', r'[a-z0-9][a-z0-9_+-]*')
    fields['extra_macros'] = manifest_list(entry, 'extra_macros', r'MULIB_IMPL_[A-Z0-9_]+', optional=True)
    if macro is None and fields['extra_macros']:
        reject('non-external module declares implementation switches: '+name)
    if registration:
        fields['status'] = manifest_scalar(entry, 'status', r'implemented|bounded')
        if (fields['status'] == 'bounded') != (implementation in ('static-inline', 'static-data')):
            reject('MANIFEST implementation metadata does not match status: '+name)
        optional_dependency = manifest_scalar(entry, 'optional_dependency', r'[A-Za-z0-9][ -~]{0,511}',
                                              optional=True)
        if optional_dependency is not None:
            if re.search(r':(?:\s|$)', optional_dependency):
                reject('unsupported MANIFEST optional_dependency scalar: '+name)
            fields['optional_dependency'] = optional_dependency
    return fields


def module_info(root, name, manifest=None):
    # This is the repository's plain MANIFEST subset, not a general YAML parser.
    if manifest is None:
        manifest = read(root/'MANIFEST.yaml', 8*1024*1024).decode('utf-8')
    manifest = manifest.split('\nproject_files:', 1)[0]
    match = re.search(r'^- name: '+re.escape(name)+r'\n(.*?)(?=^- name:|\Z)', manifest, re.M|re.S)
    if not match:
        reject('module missing from MANIFEST: ' + name)
    return module_fields(name, match[1])


def manifest_alias(name):
    logical = name.replace('-', '_').replace('+', '_plus_')
    return 'k_'+logical if logical[0].isdigit() else logical


def manifest_aliases(modules):
    """Index source spellings without letting one canonical name shadow another."""
    aliases = {}
    for name in modules:
        aliases.setdefault(manifest_alias(name), []).append(name)
    return aliases


def local(root, name):
    path = Path(name)
    if path.is_absolute() or '..' in path.parts or not path.parts:
        reject('nonlocal MuLIB path: '+str(name))
    resolved = (root/path).resolve()
    if not resolved.is_relative_to(root.resolve()) or not resolved.is_file():
        reject('missing/escaping MuLIB input: '+str(name))
    return resolved


def manifest_inventory(root, *, raw=None, registration=False, reservations=None):
    if raw is None:
        raw = read(local(root, 'MANIFEST.yaml'), 8*1024*1024)
    # Strip plain trailing comments as the generator does. Preserve everything
    # else in the snapshot hash, including comments and unrelated top-level data.
    text = '\n'.join(line for raw_line in raw.decode('utf-8').splitlines()
                     if (line := re.split(r'(?:^|[ \t]+)#', raw_line, maxsplit=1)[0].rstrip()))
    sections = re.split(r'^modules: *$', text, flags=re.M)
    if len(sections) != 2 or b'\0' in raw:
        reject('missing/duplicate/invalid MANIFEST modules section')
    text = re.split(r'^[a-zA-Z_][a-zA-Z0-9_-]*:', sections[1], maxsplit=1, flags=re.M)[0]
    records = re.split(r'^- name: ', text, flags=re.M)
    if records[0].strip() or not 1 < len(records) <= 4097:
        reject('missing/duplicate/excessive MANIFEST modules')
    modules, names = {}, set()
    for record in records[1:]:
        name, _, entry = record.partition('\n')
        if (name in names or not re.fullmatch(r'[a-z0-9][a-z0-9_+-]*', name) or
                any(line and not line.startswith('  ') for line in entry.splitlines())):
            reject('invalid/duplicate MANIFEST module: '+name)
        names.add(name)
        status = manifest_scalar(entry, 'status', r'implemented|bounded|reserved',
                                 optional=not registration)
        if status == 'reserved':
            if (manifest_scalar(entry, 'header', r'[A-Za-z0-9_./+-]+') != 'include/mulib-'+name+'.h'
                    or re.search(r'^  (macro|implementation):', entry, re.M)):
                reject('invalid MANIFEST reservation: '+name)
            if manifest_list(entry, 'dependencies', r'[a-z0-9][a-z0-9_+-]*'):
                reject('reserved MANIFEST module cannot declare dependencies: '+name)
            if reservations is not None:
                reservations.add('include/mulib-'+name+'.h')
            continue
        item = module_fields(name, entry, registration=registration)
        if item['header'] != 'include/mulib-'+name+'.h':
            reject('noncanonical module header: '+name)
        local(root, item['header'])
        local(root, item['manual'])
        modules[name] = item
    for name, item in modules.items():
        if set(item['manifest_dependencies']) - names:
            reject('unknown MANIFEST dependency: '+name)
        if set(item['manifest_dependencies']) - modules.keys():
            reject('reserved MANIFEST dependency: '+name)
    return modules


def inventory(root):
    """Inventory only: report existing sidecars, never infer contracts from C."""
    reservations = set()
    modules = manifest_inventory(root, reservations=reservations)
    exo = exotract_catalog()
    core = contract_catalog()['profiles']
    if (core.keys() | exo['profiles'].keys()) - modules.keys():
        reject('contract catalog references an unknown MANIFEST module')
    known = {m['header'] for m in modules.values()}
    headers = {p.relative_to(root).as_posix() for p in root.glob(exo['headers'])}
    for header in headers - known:
        local(root, header)
    sidecars, source_matches, non_kernel = {}, {}, []
    paths = sorted([*CONTRACTS.glob('*.exocontract.json'), *CONTRACTS.glob('*/*.exocontract.json')])
    for path in paths:
        relative = path.relative_to(CONTRACTS).as_posix()
        bundle = load(local(CONTRACTS, relative), 2*1024*1024)
        name = bundle['library']
        if name not in modules or bundle['headers'] != [Path(modules[name]['header']).name]:
            non_kernel.append(relative)
            continue
        if name in sidecars or module_discovery_only_reason(name, modules[name], exo):
            reject('conflicting kernel sidecar registration: '+name)
        sidecars[name] = relative
        source_matches[name] = bundle['bundle_provenance']['revision'] == 'sha256:'+digest(local(root, modules[name]['header']))
    for name, profile in core.items():
        if sidecars.get(name) != profile['contract']:
            reject('missing/mismatched core sidecar registration: '+name)
    for name, profile in exo['profiles'].items():
        if 'sandbox_contract' in profile and sidecars.get(name) != profile['sandbox_contract']:
            reject('missing/mismatched sandbox sidecar registration: '+name)
    coverage = {}
    for name in modules:
        reason = module_discovery_only_reason(name, modules[name], exo)
        coverage[name] = dict(semantic_sidecar=sidecars.get(name), sidecar_header_matches=source_matches.get(name),
                              snapshot_profile=name in core,
                              archive_recipe=None if reason else exo['profiles'].get(name, {}).get('recipe', exo['default_recipe']),
                              discovery_only_reason=reason)
    return dict(modules=modules, auxiliary_headers=sorted(headers - known - reservations),
                reserved_headers=sorted(headers & reservations), coverage=coverage,
                non_kernel_sidecars=non_kernel, safe_export=False)


def module_discovery_only_reason(name, module, exo):
    if module.get('implementation') == 'static-data':
        return 'static-data header has no callable C API or supported external archive ABI'
    if module['macro'] is None:
        return 'static-inline header has no supported external archive ABI'
    return discovery_only_reason(name, exo)


def plan(root, data, libraries):
    modules = []
    for library in libraries:
        for module in data['profiles'][library]['implementation_modules']:
            if module not in modules:
                modules.append(module)
    entries = [dict(name=name, **module_info(root, name)) for name in modules]
    if any(m['macro'] is None for m in entries):
        reject('static-inline headers are unsupported in contract archives')
    macros = [m['macro'] for m in entries]
    active = {macro for name in libraries for macro in data['profiles'][name].get('active_optional_macros', [])}
    for name in libraries:
        for macro in data['profiles'][name].get('active_optional_macros', []):
            if macro not in macros:
                macros.append(macro)
    if len(macros) != len(set(macros)):
        reject('duplicate implementation switch')
    links = sorted({link for n in libraries for link in data['profiles'][n]['link_libraries']})
    inactive = sorted({m for n in libraries for m in data['profiles'][n]['inactive_optional_macros']} - active)
    return {'schema': 'on1x.contract-build-plan/v1', 'safe_export': False,
            'libraries': libraries, 'modules': entries, 'implementation_macros': macros,
            'implementation_translation_units': 1, 'include_directory': str(root/'include'),
            'link_libraries': links, 'inactive_optional_macros': inactive,
            'note': 'Explicit manual-backed profiles. Optional MANIFEST switches require active_optional_macros in the selected profile; core Parsec alone does not enable its owned PARSER API.'}


def invoke(bind, *args):
    # Only the caller's explicit executable is authorized, never lock metadata.
    p = subprocess.run([str(bind), *map(str, args)], capture_output=True, timeout=90)
    if len(p.stdout) > MAX_BYTES or len(p.stderr) > 4*1024*1024:
        reject('contract tool output limit exceeded')
    if p.returncode:
        reject(f'on1x-bind failed ({p.returncode}): '+p.stderr[:65536].decode('utf-8', errors='replace'))
    return p.stdout


def publish_new(stage, output):
    # rename() can overwrite an empty directory created between check/rename.
    # Linux RENAME_NOREPLACE publishes atomically without that destructive race.
    if not sys.platform.startswith('linux'):
        reject('atomic snapshot publication currently requires Linux renameat2')
    libc = ctypes.CDLL(None, use_errno=True)
    rename = getattr(libc, 'renameat2', None)
    if rename is None:
        reject('atomic no-replace directory rename is unavailable')
    rename.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
    rename.restype = ctypes.c_int
    if rename(-100, os.fsencode(stage), -100, os.fsencode(output), 1):
        error = ctypes.get_errno()
        raise OSError(error, os.strerror(error), str(output))


def snapshot(root, data, libraries, bind, output, resource, target):
    if output.exists() or output.is_symlink():
        reject('snapshot destination already exists; existing evidence is never overwritten')
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.on1x-contracts-', dir=output.parent))
    try:
        inputs = {}
        def observe(path, expected=None):
            path = str(Path(path).resolve())
            actual = digest(path)
            if expected and actual != expected or path in inputs and inputs[path] != actual:
                reject('input changed during discovery: ' + path)
            inputs[path] = actual
        for path in [root/'MANIFEST.yaml', CONTRACTS/'catalog.json', CONTRACTS/'obligations.json', bind, Path(__file__), Path(sys.executable)]:
            observe(path)
        fingerprints = {}
        for name in libraries:
            profile = data['profiles'][name]
            seed = CONTRACTS/profile['contract']
            observe(seed); observe(root/profile['manual']); observe(root/profile['header'], profile['header_sha256'])
            evidence = [root/path for path in profile.get('evidence_sha256', {})]
            for path, expected in profile.get('evidence_sha256', {}).items():
                observe(root/path, expected)
            args = ['extract', root/profile['header'], '--revision', 'sha256:'+profile['header_sha256'], '--include', root/'include']
            if resource:
                args += ['--resource-dir', resource]
            if target:
                args += ['--target', target]
            for provenance in [root/'MANIFEST.yaml', root/profile['manual'], seed, CONTRACTS/'catalog.json', CONTRACTS/'obligations.json', *evidence]:
                args += ['--provenance', provenance]
            wire = invoke(bind, *args); unit = loads(wire)
            if unit['schema_version'] != 2 or not unit['complete'] or not unit['fingerprint'] or unit['header'] != str((root/profile['header']).resolve()):
                reject('incomplete/wrong CDecl identity')
            names = {d['name'] for d in unit['declarations'] if d['kind'] == 'function' and d['name'].startswith(profile['function_prefix'])}
            if names != {f['c_name'] for f in load(seed)['functions']}:
                reject(name+': contract coverage differs from compiler-discovered declarations')
            for entry in unit['inputs']:
                observe(entry['path'], entry['sha256'])
            decl = stage/(name+'.cdecl.json'); decl.write_bytes(wire)
            pinned = stage/(name+'.exocontract.json'); pinned.write_bytes(invoke(bind, 'pin', decl, seed))
            bundle = load(pinned)
            if bundle['state'] != 'annotated' or any(f['provenance']['review_status'] != 'unreviewed' for f in bundle['functions']):
                reject('pinning must not promote review status')
            check = loads(invoke(bind, 'check', decl, pinned))
            if check['safe_export'] is not False or check['gate_b'] is not True or check['fingerprint'] != unit['fingerprint']:
                reject('invalid gate report')
            (stage/(name+'.check.json')).write_text(dumps(check))
            fingerprints[name] = unit['fingerprint']
        (stage/'build-plan.json').write_text(dumps(plan(root, data, libraries)))
        for path, expected in inputs.items():
            if digest(path) != expected:
                reject('input drift before snapshot publication: ' + path)
        artifacts = {p.name: digest(p) for p in sorted(stage.iterdir())}
        lock = {'schema': 'on1x.contract-lock/v1', 'safe_export': False, 'root': str(root),
                'libraries': libraries, 'fingerprints': fingerprints,
                'bind_sha256': digest(bind), 'python_version': sys.version, 'artifacts': artifacts,
                'inputs': inputs, 'resource_directory': resource, 'target': target}
        (stage/'lock.json').write_text(dumps(lock))
        # Destination is new; publishing the complete directory is one rename.
        publish_new(stage, output)
    finally:
        if stage.exists():
            shutil.rmtree(stage)


def recheck(root, data, bind, directory):
    lock = load(directory/'lock.json', 2*1024*1024)
    keys(lock, ['schema', 'safe_export', 'root', 'libraries', 'fingerprints', 'bind_sha256', 'python_version', 'artifacts', 'inputs', 'resource_directory', 'target'])
    if lock['schema'] != 'on1x.contract-lock/v1' or lock['safe_export'] is not False or lock['root'] != str(root):
        reject('unsupported/unsafe/wrong-root contract lock')
    libraries = lock['libraries']
    if not libraries or len(set(libraries)) != len(libraries) or set(libraries) - data['profiles'].keys():
        reject('invalid locked libraries')
    if digest(bind) != lock['bind_sha256']:
        reject('explicit contract tool identity drift; regenerate evidence')
    if lock['python_version'] != sys.version:
        reject('Python toolchain identity drift; regenerate evidence')
    for path in [bind, Path(__file__), Path(sys.executable), root/'MANIFEST.yaml', CONTRACTS/'catalog.json', CONTRACTS/'obligations.json']:
        if lock['inputs'].get(str(path.resolve())) != digest(path):
            reject('tool/provenance input drift or omission: ' + str(path))
    for path, expected in lock['inputs'].items():
        if digest(path) != expected:
            reject('source/toolchain/provenance input drift: ' + path)
    expected = {'build-plan.json'} | {n+suffix for n in libraries for suffix in ['.cdecl.json', '.exocontract.json', '.check.json']}
    if set(lock['artifacts']) != expected or {p.name for p in directory.iterdir()} != expected | {'lock.json'}:
        reject('snapshot inventory drift')
    for name, checksum in lock['artifacts'].items():
        if Path(name).name != name or digest(directory/name) != checksum:
            reject('snapshot artifact drift: ' + name)
    for name in libraries:
        decl = directory/(name+'.cdecl.json')
        pinned = directory/(name+'.exocontract.json')
        unit = load(decl)
        profile = data['profiles'][name]
        if unit['header'] != str((root/profile['header']).resolve()) or unit['revision'] != 'sha256:'+profile['header_sha256']:
            reject('locked declaration source drift: ' + name)
        discovered = {d['name'] for d in unit['declarations'] if d['kind'] == 'function' and d['name'].startswith(profile['function_prefix'])}
        if discovered != {f['c_name'] for f in load(CONTRACTS/profile['contract'])['functions']}:
            reject('locked declaration coverage drift: ' + name)
        for entry in unit['inputs']:
            if lock['inputs'].get(entry['path']) != entry['sha256']:
                reject('lock omits or changes a CDecl input: ' + entry['path'])
        if loads(invoke(bind, 'pin', decl, CONTRACTS/profile['contract'])) != load(pinned):
            reject('pinned contract no longer matches source annotations: ' + name)
        report = loads(invoke(bind, 'check', decl, pinned))
        if report != load(directory/(name+'.check.json')) or report['fingerprint'] != lock['fingerprints'][name]:
            reject('contract gate/fingerprint drift: ' + name)
    if load(directory/'build-plan.json') != plan(root, data, libraries):
        reject('build-plan drift')
    return {'schema': 'on1x.contract-recheck/v1', 'libraries': libraries,
            'gate_b': True, 'inputs_unchanged': True, 'safe_export': False}


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--root', type=Path, default=HOME.parent, help='canonical MuLIB checkout, not a vendored snapshot')
    commands = cli.add_subparsers(dest='command', required=True)
    commands.add_parser('list', help='MANIFEST-backed MuLIB kernel coverage, not ABI extraction or review')
    commands.add_parser('lint', help='source/annotation inventory only; not ExoML Gate B')
    build = commands.add_parser('build-plan')
    build.add_argument('libraries', nargs='*', default=[])
    snap = commands.add_parser('snapshot')
    snap.add_argument('output', type=Path)
    snap.add_argument('--bind', type=Path, required=True)
    snap.add_argument('--resource-dir', default='')
    snap.add_argument('--target', default='')
    snap.add_argument('--library', action='append', default=[])
    check = commands.add_parser('check')
    check.add_argument('directory', type=Path)
    check.add_argument('--bind', type=Path, required=True)
    args = cli.parse_args()
    root = args.root.resolve()
    if args.command == 'list':
        sys.stdout.write(dumps(dict(schema='on1x.contract-inventory/v1', **inventory(root))))
        return
    data = catalog(root)
    libraries = getattr(args, 'library', []) or getattr(args, 'libraries', []) or list(data['profiles'])
    if len(set(libraries)) != len(libraries) or set(libraries) - data['profiles'].keys():
        reject('unknown/duplicate requested library')
    if args.command == 'lint':
        result = {'schema': 'on1x.contract-lint/v1', 'profiles': {k: v['function_count'] for k, v in data['profiles'].items()}, 'gate_b': False, 'safe_export': False}
    elif args.command == 'build-plan':
        result = plan(root, data, libraries)
    elif args.command == 'snapshot':
        snapshot(root, data, libraries, args.bind.resolve(), args.output.absolute(), args.resource_dir, args.target)
        result = {'snapshot': str(args.output.absolute()), 'safe_export': False}
    else:
        result = recheck(root, data, args.bind.resolve(), args.directory.resolve())
    sys.stdout.write(dumps(result))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, TypeError, OSError, RecursionError, UnicodeError, subprocess.SubprocessError) as error:
        print('on1x-contracts: '+str(error), file=sys.stderr)
        sys.exit(3)
