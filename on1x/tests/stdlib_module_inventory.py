#!/usr/bin/env python3
"""List stdlib source paths and their packaging roles, not SML conformance."""

import argparse
from collections import Counter
import json
from pathlib import Path
import shlex


ON1X = Path(__file__).resolve().parents[1]
STDLIB = ON1X / 'stdlib'
SOURCE_SUFFIXES = {'.on1x', '.on1xm', '.on1xi'}
MANIFEST_ROLES = {'On1xfile.m': {'source', 'library'}, 'On1xfile.i': {'interface'}}
BUNDLE_ROLES = {'library', 'interface', 'module', 'module-interface'}

# Fixtures and the separately hosted SDK are classified by directory.
# The only other sources deliberately outside the std source bundle are:
SOURCE_TREE_ONLY = {
    'daemon/__init__.on1x': 'kernelbind',
    'daemon/daemon.on1x': 'kernelbind',
    'mcp/__init__.on1x': 'placeholder',
}


def collect(root):
    sources = {
        path.relative_to(root).as_posix(): path
        for path in root.rglob('*')
        if path.is_file() and path.suffix in SOURCE_SUFFIXES
    }
    declarations = {}
    members = {}
    policies = {}
    for line, row in enumerate((root / 'On1xfile').read_text(encoding='utf-8').splitlines(), 1):
        tokens = shlex.split(row, comments=True)
        if not tokens or tokens[0] not in ('module', 'module-interface', 'module-interface-policy'):
            continue
        if len(tokens) != 3:
            raise ValueError(f'{root}/On1xfile:{line}: expected a member name and path or policy')
        role, member, name = tokens
        if role == 'module-interface-policy':
            if member in policies:
                raise ValueError(f'duplicate interface policy: {member}')
            policies[member] = name
            continue
        expected = '.on1xm' if role == 'module' else '.on1xi'
        if name not in sources or sources[name].suffix != expected or name in declarations:
            raise ValueError(f'{root}/On1xfile:{line}: invalid or duplicate {role}: {name}')
        declarations[name] = role
        members[name] = member
    manifests = sorted(path for name in MANIFEST_ROLES for path in root.rglob(name))
    for manifest in manifests:
        for line, row in enumerate(manifest.read_text(encoding='utf-8').splitlines(), 1):
            tokens = shlex.split(row, comments=True)
            if not tokens or tokens[0] not in MANIFEST_ROLES[manifest.name]:
                continue
            if len(tokens) != 2:
                raise ValueError(f'{manifest}:{line}: expected a single source path')
            role, name = tokens
            path = (manifest.parent / name).relative_to(root).as_posix()
            if path not in sources:
                raise ValueError(f'{manifest}:{line}: source not found: {path}')
            if path in declarations:
                raise ValueError(f'{manifest}:{line}: duplicate source role: {path}')
            declarations[path] = role

    result = []
    declared_roles = {}
    for name, path in sorted(sources.items()):
        parts = path.relative_to(root).parts
        if 'tests' in parts or parts[:2] == ('daemon', 'fixtures'):
            profile = 'fixture'
        elif parts[0] == 'sdk':
            profile = 'sdk'
        else:
            profile = SOURCE_TREE_ONLY.get(name, 'stdlib')
        role = declarations.get(name)
        if profile == 'stdlib' and role is None:
            raise ValueError(f'no source bundle role or explicit disposition: {name}')
        allowed = {'.on1x': {'library'}, '.on1xm': {'module'},
                   '.on1xi': {'interface', 'module-interface'}}[path.suffix]
        if profile == 'stdlib' and role not in allowed:
            raise ValueError(f'unexpected std source bundle role {role}: {name}')
        if profile != 'stdlib' and role in BUNDLE_ROLES:
            raise ValueError(f'non-stdlib source declared in the std bundle: {name}')
        initializer = path.stem == '__init__'
        # This is a conventional path spelling, not a parser/lookup guarantee.
        # For example, base/fn.on1x uses a quoted import because fn is reserved.
        candidate = None
        if profile != 'fixture':
            segments = parts[:-1] if initializer else (*parts[:-1], path.stem)
            candidate = '.'.join(('std', *segments))
        member = members.get(name)
        if member is not None:
            if member != candidate:
                raise ValueError(f'expected canonical member {candidate} for {name}, found {member}')
            if path.with_suffix('.on1x').relative_to(root).as_posix() in sources:
                raise ValueError(f'duplicate legacy implementation for canonical member {member}')
            if role in declared_roles.setdefault(member, set()):
                raise ValueError(f'duplicate {role} for canonical member {member}')
            declared_roles[member].add(role)
        result.append(dict(path=name, suffix=path.suffix,
                           kind=('interface' if path.suffix == '.on1xi'
                                 else 'initializer' if initializer else 'source'),
                           profile=profile, bundle_role=role, path_name=candidate,
                           declared_module=member))

    for member, roles in declared_roles.items():
        if roles != {'module', 'module-interface'} or policies.get(member) != 'required':
            raise ValueError(f'{member}: canonical std member requires implementation and interface')
    if policies.keys() - declared_roles.keys():
        raise ValueError(f'interface policy without member: {min(policies.keys() - declared_roles.keys())}')

    for path in SOURCE_TREE_ONLY:
        if path not in sources:
            raise ValueError(f'stale source-tree-only disposition: {path}')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='report counts instead of JSON')
    options = parser.parse_args()
    try:
        entries = collect(STDLIB)
    except ValueError as exc:
        parser.exit(1, f'{exc}\n')
    if options.check:
        profiles = Counter(entry['profile'] for entry in entries)
        initializers = sum(entry['kind'] == 'initializer' for entry in entries)
        interfaces = sum(entry['kind'] == 'interface' for entry in entries)
        migrated = sum(entry['suffix'] == '.on1xm' for entry in entries)
        print(f'{len(entries)} sources; {initializers} initializers; {interfaces} interfaces; '
              f'{migrated} named implementations; '
              + ', '.join(f'{key}={count}' for key, count in sorted(profiles.items())))
    else:
        print(json.dumps(entries, indent=2))


if __name__ == '__main__':
    main()
