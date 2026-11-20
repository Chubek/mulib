#!/usr/bin/env python3
"""Check MuLL's discovery inventory and pinned raw bridge without libclang."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
LIBRARIES = b'static const mb_library mb_libraries[] = {\n'
METADATA_ROW = re.compile(rb'\{"([^"]+)","([^"]+\.h)",NULL,0\},\n')


def module_name(header):
    return re.sub(r'^(include-)?mulib-', '', header.removesuffix('.h'))


def header_fingerprint(include):
    """Pin the public headers and their repository-local include inputs."""
    h = hashlib.sha256()
    for path in sorted(p for p in include.rglob('*') if p.is_file()):
        name = path.relative_to(include).as_posix().encode('utf-8')
        data = path.read_bytes()
        for part in (name, data):
            h.update(len(part).to_bytes(8, 'big'))
            h.update(part)
    return h.hexdigest()


def bridge_surface(generated):
    """Separate metadata-only rows from the generated legacy C surface."""
    before, after = generated.split(LIBRARIES, 1)
    table, after = after.split(b'};\n', 1)
    discovery_rows = METADATA_ROW.findall(table)
    discovery = {module.decode(): header.decode() for module, header in discovery_rows}
    if len(discovery) != len(discovery_rows):
        raise ValueError('Duplicate discovery-only library row')
    raw = before + LIBRARIES + METADATA_ROW.sub(b'', table) + b'};\n' + after
    return discovery, hashlib.sha256(raw).hexdigest()


def check_inventory(include, bindings):
    headers = sorted(include.glob('*.h'))
    if not headers:
        raise ValueError(f'No public headers found in {include}')
    inventory = json.loads((bindings / 'Inventory.json').read_bytes())
    names = [header.name for header in headers]
    recorded = inventory['headers']
    if recorded != names:
        missing = sorted(set(names) - set(recorded))
        extra = sorted(set(recorded) - set(names))
        raise ValueError(f'Header registration mismatch ({len(missing)} missing, {len(extra)} extra): '
                         f'missing={missing[:12]}, extra={extra[:12]}')
    modules = {module_name(name): name for name in names}
    if (len(modules) != len(names) or inventory['libraries'] != len(modules)
            or set(inventory['modules']) != set(modules)):
        raise ValueError('Library registrations do not cover the public headers')
    policy_text = (bindings / 'LegacyExports.json').read_bytes()
    if inventory['legacy_policy'] != hashlib.sha256(policy_text).hexdigest():
        raise ValueError('Legacy export policy changed; regenerate the MuLL bridge')
    legacy = json.loads(policy_text)
    policy = legacy['headers']
    if not set(policy) <= set(names):
        raise ValueError('Legacy export policy references a missing header')
    functions = []
    declaration_count = 0
    for name, header in modules.items():
        module = inventory['modules'][name]
        if module['header'] != header:
            raise ValueError(f'Incorrect header for library {name}')
        raw = module['functions']
        declared = module['declarations']
        profile = 'legacy-raw' if header in policy else 'discovery-only'
        allowed = policy[header]['functions'] if header in policy else []
        if module['profile'] != profile or sorted(raw) != sorted(allowed):
            raise ValueError(f'Legacy function registrations do not match policy for {header}')
        if declared != sorted(set(declared)) or not set(raw) <= set(declared):
            raise ValueError(f'Function declarations are inconsistent for {header}')
        declaration_count += len(declared)
        functions.extend(raw)
    if len(functions) != inventory['functions'] or len(set(functions)) != len(functions):
        raise ValueError('Function registrations are inconsistent')
    if declaration_count != inventory['declared_functions']:
        raise ValueError('Function declaration count is inconsistent')
    if inventory['fingerprint'] != header_fingerprint(include):
        raise ValueError('Header inputs changed; regenerate the MuLL bridge')
    artifacts = inventory.get('artifacts', {})
    for name in ('Generated.inc', 'Implementation.c'):
        actual = hashlib.sha256((bindings / name).read_bytes()).hexdigest()
        if artifacts.get(name) != actual:
            raise ValueError(f'{name} does not match Inventory.json; regenerate the MuLL bridge')
    discovery, digest = bridge_surface((bindings / 'Generated.inc').read_bytes())
    if discovery != {module_name(h): h for h in names if h not in policy}:
        raise ValueError('Generated C discovery rows do not match the inventory')
    if digest != legacy['surface_sha256']:
        raise ValueError('Generated C legacy surface differs from LegacyExports.json')
    return len(names), declaration_count, len(functions)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--include', type=Path, default=ROOT.parent / 'include')
    parser.add_argument('--bindings', type=Path, default=ROOT / 'mull/bridge/Mulib')
    args = parser.parse_args()
    try:
        headers, declared, functions = check_inventory(args.include, args.bindings)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, f'MuLL MuLIB inventory: {error}\n'
                    'Run python3 mudsl/scripts/generate_mulib.py --include include\n')
    print(f'MuLL MuLIB inventory: {headers} headers, {declared} discovered function declarations, '
          f'{functions} legacy raw functions')


if __name__ == '__main__':
    main()
