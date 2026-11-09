#!/usr/bin/env python3
"""Check MuLL's complete MuLIB registration snapshot without requiring libclang."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def check_inventory(include, bindings):
    headers = sorted(include.glob('*.h'))
    if not headers:
        raise ValueError(f'No public headers found in {include}')
    inventory = json.loads((bindings / 'Inventory.json').read_text())
    names = [header.name for header in headers]
    recorded = inventory['headers']
    if recorded != names:
        missing = sorted(set(names) - set(recorded))
        extra = sorted(set(recorded) - set(names))
        raise ValueError(f'Header registration mismatch: missing={missing}, extra={extra}')
    modules = {re.sub(r'^(include-)?mulib-', '', name.removesuffix('.h')): name
               for name in names}
    if inventory['libraries'] != len(modules) or set(inventory['modules']) != set(modules):
        raise ValueError('Library registrations do not cover the public headers')
    functions = []
    for name, header in modules.items():
        module = inventory['modules'][name]
        if module['header'] != header:
            raise ValueError(f'Incorrect header for library {name}')
        functions.extend(module['functions'])
    if len(functions) != inventory['functions'] or len(set(functions)) != len(functions):
        raise ValueError('Function registrations are inconsistent')
    fingerprint = hashlib.sha256(''.join(header.read_text() for header in headers).encode()).hexdigest()
    if inventory['fingerprint'] != fingerprint:
        raise ValueError('Header APIs changed; regenerate the MuLL bridge')
    artifacts = inventory.get('artifacts', {})
    for name in ('Generated.inc', 'Implementation.c'):
        actual = hashlib.sha256((bindings / name).read_text().encode()).hexdigest()
        if artifacts.get(name) != actual:
            raise ValueError(f'{name} does not match Inventory.json; regenerate the MuLL bridge')
    return len(names), len(modules), len(functions)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--include', type=Path, default=ROOT.parent / 'include')
    parser.add_argument('--bindings', type=Path, default=ROOT / 'mull/stdlib/Mulib')
    args = parser.parse_args()
    try:
        headers, libraries, functions = check_inventory(args.include, args.bindings)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, f'MuLL MuLIB inventory: {error}\n'
                    'Run python3 mudsl/scripts/generate_mulib.py --include include\n')
    print(f'MuLL MuLIB inventory: {headers} headers, {libraries} libraries, {functions} functions registered')


if __name__ == '__main__':
    main()
