#!/usr/bin/env python3
"""Synchronize header metadata and combined-test inclusion, retaining manual prose.

Public API source, manuals and examples remain handwritten. Comment-only headers
are explicitly reserved; existing module descriptions/extra_macros are preserved.
Static-inline and static-data modules require explicit bounded MANIFEST entries.
Implemented reservations are promoted; API removal requires a manual manifest edit.
Publishing additions contribute module entries only and are removed after a
successful sync. An implemented status describes the documented API subset, not
certification of a named format or standard.
Run after adding modules, then run tools/stratify.py and tools/check.py.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent.parent
DEPENDENCY_FIELD = re.compile(r'^  dependencies:[^\n]*(?:\n(?: {2}| {4})- [^\n]*)*', re.M)
IMPLEMENTATION_SWITCH = re.compile(
    r'^[ \t]*#[ \t]*if(?:def[ \t]+|[ \t]+defined(?:[ \t]*\([ \t]*|[ \t]+))'
    r'(MULIB_IMPL_[A-Z0-9_]+)\b', re.M)


def refresh_dependencies(block, deps):
    field = DEPENDENCY_FIELD.search(block)
    line = '  dependencies: [' + ', '.join(deps) + ']'
    if field is None:
        return block.rstrip() + '\n' + line + '\n'
    head, _, tail = field[0].partition('\n')
    inline = re.fullmatch(r'  dependencies: +\[([^\]]*)\]', head)
    previous = None
    if inline and not tail:
        previous = [d.strip() for d in inline[1].split(',')] if inline[1].strip() else []
    elif head == '  dependencies:' and tail:
        rows = [re.fullmatch(r'( {2}| {4})- (.+)', item) for item in tail.splitlines()]
        if all(rows) and len({row[1] for row in rows}) == 1:
            previous = [row[2] for row in rows]
    if previous is not None and sorted(previous) == deps:
        return block
    return block[:field.start()] + line + block[field.end():]


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
        if boundary:
            text = text[boundary.end():]
        elif not re.match(r'(?:[ \t]*(?:#[^\n]*)?\n)*- name: ', text):
            raise ValueError('additions lack modules section: ' + path.name)
        # Stop at any subsequent root key, not only project_files. None of the
        # appendage's root metadata belongs in the upstream manifest.
        entries = re.split(r'^(?=[A-Za-z_][A-Za-z0-9_]*:)', text,
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
                    if field == 'macro' and re.search(
                            r'^  implementation: static-(?:inline|data)$', block, re.M):
                        continue
                    raise ValueError('additions module lacks ' + field)
                value = match.group(1)
                if value in used[field]:
                    raise ValueError('additions collision for ' + field + ': ' + value)
                used[field].add(value)
                if field == 'header':
                    if not re.fullmatch(r'kernels/mulib-[a-z0-9][a-z0-9_+-]*\.h', value):
                        raise ValueError('invalid additions header: ' + value)
                    header = root / value
                    if not header.is_file():
                        raise ValueError('missing additions header: ' + value)
            # Match the upstream inline dependency-list convention. The normal
            # sync below verifies dependencies against the actual header includes.
            block = re.sub(r'^  dependencies:\n((?:(?: {2}| {4})- [^\n]+\n?)+)',
                           lambda m: '  dependencies: [' + ', '.join(
                               re.findall(r'^(?: {2}| {4})- (.+)$', m.group(1), re.M)) + ']\n',
                           block, flags=re.M)
            example = root / 'examples' / (header.stem + '.c')
            if (not re.search(r'^  example:', block, re.M) and example.is_file() and not
                    (root / 'examples/modules' / (header.stem[6:] + '.c')).is_file()):
                block = block.rstrip() + '\n  example: ' + example.relative_to(root).as_posix() + '\n'
            blocks.append(block.rstrip() + '\n')
    return manifest.replace('project_files:', ''.join(blocks) + 'project_files:', 1), paths


def sync(root, *, fixtures=True, kernelbind=None):
    manifest_path = root / 'MANIFEST.yaml'
    if manifest_path.is_symlink():
        raise ValueError('symlinked MANIFEST.yaml')
    manifest = manifest_path.read_text()
    if not re.search(r'^project_files:\s*$', manifest, re.M):
        raise ValueError('manifest lacks project_files section')
    manifest, additions = merge_additions(root, manifest)
    headers = sorted((root / 'kernels').glob('mulib-*.h'))
    registered = {}
    for block in re.findall(r'^- name: .*?(?=^- name: |^project_files:|\Z)',
                            manifest, re.M | re.S):
        name = re.search(r'^  header: kernels/(.+)$', block, re.M)
        if not name or name.group(1) in registered:
            raise ValueError('missing or duplicate header in manifest')
        registered[name.group(1)] = block
    if set(registered) - {p.name for p in headers}:
        raise ValueError('manifest references missing headers')
    engineering_headers = {name for name, block in registered.items()
                      if re.search(r'^  profile: engineering$', block, re.M)}
    optional_headers = {name for name, block in registered.items()
                        if re.search(r'^  optional_dependency: .+$', block, re.M)}
    switches, fixture_switches, blocks = [], [], []
    for header in headers:
        text = header.read_text()
        match = IMPLEMENTATION_SWITCH.search(text)
        inline = header.name in registered and bool(re.search(
            r'^  implementation: static-(?:inline|data)$', registered[header.name], re.M))
        if inline and match:
            raise ValueError('internal-linkage header gained an ABI switch: ' + header.name)
        if match:
            switches.append(match.group(1))
            if header.name not in engineering_headers | optional_headers:
                fixture_switches.append(match.group(1))
        elif not inline and re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S).strip():
            raise ValueError('header lacks implementation switch: ' + header.name +
                             '; internal-linkage headers require bounded MANIFEST metadata')
        name = header.stem[6:]
        deps = sorted(set(re.findall(r'^\s*#\s*include\s*["<]mulib-([^">]+)\.h[">]', text, re.M)))
        if header.name in registered:
            old = registered[header.name]
            implemented = bool(re.search(r'^  status: implemented$', old, re.M))
            if implemented and not match:
                raise ValueError('implemented header lost its API: ' + header.name)
            new = old
            if inline:
                if not re.search(r'^  status: bounded$', old, re.M):
                    raise ValueError('internal-linkage module must retain bounded status: ' + header.name)
                new = refresh_dependencies(new, deps)
            if match:
                new = refresh_dependencies(new, deps)
                if not implemented:
                    brief = re.search(r'@brief\s+([^\n]+)', text)
                    purpose = brief.group(1).split('*/')[0].strip() if brief else name + ' utilities.'
                    new = new.replace(
                        '  purpose: Reserved future module; no implemented API.\n',
                        '  purpose: ' + json.dumps(purpose) + '\n')
                fields = {'macro': match.group(1),
                          'status': 'implemented'}
                if not re.search(r'^  manual:', new, re.M) and header.name not in engineering_headers:
                    fields = {'manual': f'manual/mulib-{name}.md', **fields}
                for key, value in fields.items():
                    pattern = r'^  ' + key + r':[^\n]*$'
                    line = f'  {key}: {value}'
                    if re.search(pattern, new, re.M):
                        new = re.sub(pattern, lambda _: line, new, flags=re.M)
                    else:
                        new = new.rstrip() + '\n' + line + '\n'
            manifest = manifest.replace(old, new, 1)
            continue
        block = f'- name: {name}\n  header: kernels/{header.name}\n'
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
    inline_headers = {name for name, block in registered.items() if re.search(
        r'^  implementation: static-(?:inline|data)$', block, re.M)}
    if inline_headers:
        count = f'bounded_module_count: {len(inline_headers)}'
        manifest, replaced = re.subn(r'^bounded_module_count: \d+$', count, manifest, flags=re.M)
        if not replaced:
            manifest = re.sub(r'^modules:', count + '\nmodules:', manifest, count=1, flags=re.M)
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
    outputs = [(manifest_path, manifest)]
    if fixtures:
        # Engineering and internal-linkage headers receive generated TU checks.
        # Optional backends are included without enabling their implementation.
        generated_headers = inline_headers | engineering_headers
        implementation_path = root / 'tests/implementation.c'
        implementation = implementation_path.read_text()
        prefix = ''.join('#define ' + switch + '\n' for switch in fixture_switches
                         if '#define ' + switch + '\n' not in implementation)
        includes = ''.join('#include "' + header.name + '"\n' for header in headers
                            if header.name not in generated_headers
                            if '#include "' + header.name + '"' not in implementation)
        implementation = prefix + implementation.rstrip() + '\n' + includes
        declaration_path = root / 'tests/declarations.c'
        declaration = declaration_path.read_text()
        includes = ''.join('#include "' + header.name + '"\n' for header in headers
                            if header.name not in generated_headers
                            if '#include "' + header.name + '"' not in declaration)
        if includes:
            anchor = re.search(r'^\s*int\s+main\s*\(\s*void\s*\)', declaration, re.M)
            if not anchor:
                raise ValueError('declaration test lacks main(void) insertion point')
            declaration = declaration[:anchor.start()] + includes + declaration[anchor.start():]
        outputs.extend(((implementation_path, implementation),
                        (declaration_path, declaration)))
    # Validate the prospective MANIFEST and catalog before publishing either.
    if kernelbind is not None:
        catalog = kernelbind.generate(root, raw_manifest=manifest.encode('utf-8'))
        kernelbind.update(root / kernelbind.CATALOG, catalog, check=False)
    for path, content in outputs:
        if path.read_text() != content:
            path.write_text(content)
    for path in additions:
        path.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT,
                        help="MuLIB checkout to read and update (default: script's checkout)")
    parser.add_argument('--metadata-only', action='store_true',
                        help='synchronize MANIFEST.yaml and KernelBind discovery without reading or editing C test fixtures')
    args = parser.parse_args()
    # Use the generator shipped with this tool, not executable code in --root.
    spec = importlib.util.spec_from_file_location('extend_kernelbind', ROOT / 'tools/extend-kernelbind.py')
    kernelbind = importlib.util.module_from_spec(spec)
    sys.dont_write_bytecode = True
    try:
        spec.loader.exec_module(kernelbind)
        sync(args.root.resolve(), fixtures=not args.metadata_only, kernelbind=kernelbind)
    except (OSError, RuntimeError, ValueError) as error:
        parser.exit(1, f'sync_inventory: {error}\n')


if __name__ == '__main__':
    main()
