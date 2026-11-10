#!/usr/bin/env python3
"""Synchronize header metadata and combined-test inclusion, retaining manual prose.

Public API source, manuals and examples remain handwritten. Comment-only headers
are explicitly reserved; existing module descriptions/extra_macros are preserved.
Implemented reservations are promoted; API removal requires a manual manifest edit.
Publishing additions contribute module entries only and are removed after a
successful sync. An implemented status describes the documented API subset, not
certification of a named format or standard.
Run after adding modules, then run tools/stratify.py and tools/check.py.
"""
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
SUPPORT_HEADERS = {'mulib-container-core.h', 'mulib-qvm.h', 'mulib-lprowse.h'}


def merge_additions(root, manifest):
    paths = [root / name for name in ('MANIFEST.yaml.additions', 'MANIFESTS.yaml.additions')
             if (root / name).is_file()]
    blocks = []
    existing = manifest.split('project_files:', 1)[0]
    used = {field: set(re.findall(pattern, existing, re.M)) for field, pattern in (
        ('name', r'^- name: (.+)$'), ('header', r'^  header: (.+)$'),
        ('macro', r'^  macro: (.+)$'))}
    for path in paths:
        text = path.read_text()
        boundary = re.search(r'^modules:[ \t]*\n', text, re.M)
        if not boundary:
            raise ValueError('additions lack modules section: ' + path.name)
        # Stop at any subsequent root key, not only project_files. None of the
        # appendage's root metadata belongs in the upstream manifest.
        entries = re.split(r'^(?=[A-Za-z_][A-Za-z0-9_]*:)', text[boundary.end():],
                           maxsplit=1, flags=re.M)[0]
        additions = re.findall(r'^- name: .*?(?=^- name: |\Z)', entries, re.M | re.S)
        if not additions:
            raise ValueError('additions contain no modules: ' + path.name)
        for block in additions:
            for field, pattern in (('name', r'^- name: (.+)$'),
                                   ('header', r'^  header: (.+)$'),
                                   ('macro', r'^  macro: (.+)$')):
                match = re.search(pattern, block, re.M)
                if not match:
                    raise ValueError('additions module lacks ' + field)
                value = match.group(1)
                if value in used[field]:
                    raise ValueError('additions collision for ' + field + ': ' + value)
                used[field].add(value)
                if field == 'header':
                    if not re.fullmatch(r'include/mulib-[A-Za-z0-9_-]+\.h', value):
                        raise ValueError('invalid additions header: ' + value)
                    header = root / value
                    if not header.is_file():
                        raise ValueError('missing additions header: ' + value)
            # Match the upstream inline dependency-list convention. The normal
            # sync below verifies dependencies against the actual header includes.
            block = re.sub(r'^  dependencies:\n((?:  - [^\n]+\n?)+)',
                           lambda m: '  dependencies: [' + ', '.join(
                               re.findall(r'^  - (.+)$', m.group(1), re.M)) + ']\n',
                           block, flags=re.M)
            example = root / 'examples' / (header.stem + '.c')
            if (not re.search(r'^  example:', block, re.M) and example.is_file() and not
                    (root / 'examples/modules' / (header.stem[6:] + '.c')).is_file()):
                block = block.rstrip() + '\n  example: ' + example.relative_to(root).as_posix() + '\n'
            blocks.append(block.rstrip() + '\n')
    return manifest.replace('project_files:', ''.join(blocks) + 'project_files:', 1), paths


def sync(root):
    manifest_path = root / 'MANIFEST.yaml'
    manifest = manifest_path.read_text()
    if not re.search(r'^project_files:\s*$', manifest, re.M):
        raise ValueError('manifest lacks project_files section')
    manifest, additions = merge_additions(root, manifest)
    headers = sorted(p for p in (root / 'include').glob('mulib-*.h') if p.name not in SUPPORT_HEADERS)
    registered = {}
    for block in re.findall(r'^- name: .*?(?=^- name: |^project_files:|\Z)',
                            manifest, re.M | re.S):
        name = re.search(r'^  header: include/(.+)$', block, re.M)
        if not name or name.group(1) in registered:
            raise ValueError('missing or duplicate header in manifest')
        registered[name.group(1)] = block
    if set(registered) - {p.name for p in headers}:
        raise ValueError('manifest references missing headers')
    switches, blocks = [], []
    for header in headers:
        text = header.read_text()
        match = re.search(r'^#\s*if(?:def\s+|\s+defined\s*\(\s*)'
                          r'(MULIB_IMPL_[A-Z0-9_]+)', text, re.M)
        if match:
            switches.append(match.group(1))
        elif re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S).strip():
            raise ValueError('header lacks implementation switch: ' + header.name)
        name = header.stem[6:]
        deps = sorted({d for d in re.findall(r'^\s*#\s*include\s*["<]mulib-([^">]+)\.h[">]', text, re.M)
                       if 'mulib-' + d + '.h' not in SUPPORT_HEADERS})
        if header.name in registered:
            old = registered[header.name]
            implemented = bool(re.search(r'^  status: implemented$', old, re.M))
            if implemented and not match:
                raise ValueError('implemented header lost its API: ' + header.name)
            new = old
            if match:
                if not implemented:
                    brief = re.search(r'@brief\s+([^\n]+)', text)
                    purpose = brief.group(1).split('*/')[0].strip() if brief else name + ' utilities.'
                    new = new.replace(
                        '  purpose: Reserved future module; no implemented API.\n',
                        '  purpose: ' + json.dumps(purpose) + '\n')
                fields = {'manual': f'manual/mulib-{name}.md',
                          'macro': match.group(1),
                          'dependencies': '[' + ', '.join(deps) + ']',
                          'status': 'implemented'}
                for key, value in fields.items():
                    pattern = r'^  ' + key + r':[^\n]*$'
                    line = f'  {key}: {value}'
                    if key == 'dependencies':
                        current = re.search(r'^  dependencies: \[([^\]]*)\]$', new, re.M)
                        if current and {d.strip() for d in current.group(1).split(',')
                                        if d.strip()} == set(deps):
                            continue
                    if re.search(pattern, new, re.M):
                        new = re.sub(pattern, lambda _: line, new, flags=re.M)
                    else:
                        new = new.rstrip() + '\n' + line + '\n'
            manifest = manifest.replace(old, new, 1)
            continue
        block = f'- name: {name}\n  header: include/{header.name}\n'
        if match:
            brief = re.search(r'@brief\s+([^\n]+)', text)
            purpose = brief.group(1).split('*/')[0].strip() if brief else name + ' utilities.'
            block += (f'  manual: manual/mulib-{name}.md\n  macro: {match.group(1)}\n'
                      f'  purpose: {json.dumps(purpose)}\n  dependencies: [{", ".join(deps)}]\n'
                      '  status: implemented\n')
        else:
            block += '  purpose: Reserved future module; no implemented API.\n  dependencies: []\n  status: reserved\n'
        blocks.append(block)
    manifest = re.sub(r'^module_count: \d+$', f'module_count: {len(headers)}', manifest, flags=re.M)
    manifest = re.sub(r'^implemented_module_count: \d+$', f'implemented_module_count: {len(switches)}', manifest, flags=re.M)
    manifest = manifest.replace('project_files:', ''.join(blocks) + 'project_files:', 1)
    registered_files = set(re.findall(r'^- file: (.+)$', manifest, re.M))
    candidates = [(p, 'Standalone checked usage example for ' + p.stem + '.')
                   for p in sorted((root / 'examples/modules').glob('*.c'))
                   + sorted((root / 'examples').glob('mulib-*.c'))]
    candidates += [(p, 'Focused regression suite for ' + p.stem + '.')
                   for p in sorted((root / 'tests').glob('test_*.c'))
                   + sorted((root / 'tests').glob('test-*.c'))]
    candidates += [(root / 'tools/sync_inventory.py', 'Additive header inventory and combined-test synchronization.'),
                   (root / 'manual/extension-layers.md', 'Shared storage, binary, document and container contracts and limits.')]
    for path, purpose in candidates:
        relative = path.relative_to(root).as_posix()
        if path.is_file() and relative not in registered_files:
            manifest += f'- file: {relative}\n  purpose: {json.dumps(purpose)}\n'
    if len(set(switches)) != len(switches):
        raise ValueError('implementation switch shared by multiple headers')
    implementation_path = root / 'tests/implementation.c'
    implementation = implementation_path.read_text()
    prefix = ''.join('#define ' + switch + '\n' for switch in switches
                     if '#define ' + switch + '\n' not in implementation)
    includes = ''.join('#include "' + header.name + '"\n' for header in headers
                       if '#include "' + header.name + '"' not in implementation)
    implementation = prefix + implementation.rstrip() + '\n' + includes
    declaration_path = root / 'tests/declarations.c'
    declaration = declaration_path.read_text()
    includes = ''.join('#include "' + header.name + '"\n' for header in headers
                       if '#include "' + header.name + '"' not in declaration)
    if includes:
        anchor = re.search(r'^\s*int\s+main\s*\(\s*void\s*\)', declaration, re.M)
        if not anchor:
            raise ValueError('declaration test lacks main(void) insertion point')
        declaration = declaration[:anchor.start()] + includes + declaration[anchor.start():]
    # Validate/read every input before publishing any updated inventory file.
    for path, content in ((manifest_path, manifest),
                          (implementation_path, implementation),
                          (declaration_path, declaration)):
        if path.read_text() != content:
            path.write_text(content)
    for path in additions:
        path.unlink()


if __name__ == '__main__':
    sync(ROOT)
