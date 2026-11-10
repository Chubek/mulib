#!/usr/bin/env python3
"""Maintain manual references; check local links and optionally compile examples.

No third-party packages required. Handwritten contract prose is never replaced.
Header excerpts are opt-in via paired manual-interface HTML comment markers.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parent.parent
IMPLEMENTATION = re.compile(
    r'^#\s*if(?:def\s+|\s+defined\s*\(\s*)(MULIB_IMPL_[A-Z0-9_]+)', re.M)
INCLUDE = re.compile(r'^\s*#\s*include\s*["<]([^">]+)[">]', re.M)
AUXILIARY = {
    'mulib-container-core': 'container-core',
    'mulib-qvm': 'qvm-core',
    'mulib-lprowse': 'prowse-control',
}


def marker(kind, end=False):
    return '<!-- ' + ('/manual-' if end else 'manual-') + kind + ' -->'


def replace_section(text, kind, body, append=False):
    start, end = marker(kind), marker(kind, True)
    count = text.count(start)
    if count != text.count(end) or count > 1:
        raise ValueError('unbalanced or duplicate ' + kind + ' markers')
    section = start + '\n' + body.rstrip() + '\n' + end
    if count:
        a, b = text.index(start), text.index(end)
        if b < a:
            raise ValueError('reversed ' + kind + ' markers')
        return text[:a] + section + text[b + len(end):]
    return text.rstrip() + '\n\n' + section + '\n' if append else text


def inventory():
    manifest = (ROOT / 'MANIFEST.yaml').read_text()
    entries = {}
    for block in re.findall(r'^- name: .*?(?=^- name: |^project_files:|\Z)',
                            manifest, re.M | re.S):
        fields = dict(re.findall(r'^  (\w+): (.+)$', block, re.M))
        header = ROOT / fields['header']
        if header in entries:
            raise ValueError('duplicate manifest header: ' + str(header))
        entries[header] = fields
    headers = {p for p in (ROOT / 'include').glob('mulib-*.h') if p.stem not in AUXILIARY}
    if set(entries) != headers:
        raise ValueError('manifest and mulib header inventory differ')
    records = []
    for header in sorted(headers | {ROOT / 'include' / (n + '.h') for n in AUXILIARY}):
        fields = entries.get(header, {})
        if fields.get('status') == 'reserved':
            continue
        stem = header.stem
        text = header.read_text()
        switch = IMPLEMENTATION.search(text)
        if fields and (not switch or fields.get('macro') != switch.group(1)):
            raise ValueError('manifest switch differs from header: ' + header.name)
        example = ROOT / fields.get('example', 'examples/modules/' +
                                    AUXILIARY.get(stem, stem[6:]) + '.c')
        manual = ROOT / 'manual' / (stem + '.md')
        if fields and fields.get('manual') != manual.relative_to(ROOT).as_posix():
            raise ValueError('manifest manual differs from header: ' + header.name)
        for path in (manual, example):
            if not path.is_file():
                raise ValueError('missing ' + path.relative_to(ROOT).as_posix())
        purpose = fields.get('purpose', '')
        if purpose.startswith('"'):
            purpose = json.loads(purpose)
        if not purpose:
            brief = re.search(r'@brief\s+([^\n]+)', text)
            purpose = brief.group(1).split('*/')[0].strip() if brief else 'Embedded support data.'
        records.append(dict(header=header, manual=manual, example=example,
                            text=text, switch=switch.group(1) if switch else None,
                            purpose=purpose))
    return records


def link(path, label=None):
    relative = path.relative_to(ROOT).as_posix()
    return '[' + (label or '`' + relative + '`') + '](../' + relative + ')'


def local_includes(record):
    result = set()
    # Ignore commented-out directives; conditional includes remain visible.
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', record['text'], flags=re.S)
    for name in INCLUDE.findall(text):
        for candidate in (record['header'].parent / name, ROOT / 'include' / name):
            candidate = candidate.resolve()
            if candidate.is_relative_to(ROOT) and candidate.is_file():
                result.add(candidate)
                break
    return sorted(result)


def reference(record, by_header):
    deps = []
    for path in local_includes(record):
        target = by_header.get(path)
        if target:
            deps.append('[' + path.stem + '](' + target['manual'].name + ')')
        elif path.name == 'virtio.h':
            deps.append('[virtio entry point](mulib-virtio.md)')
        else:
            deps.append(link(path))
    switch = ('`' + record['switch'] + '`' if record['switch'] else
              'None (support header; see scope above)')
    source = record['example'].relative_to(ROOT).as_posix()
    command = shlex.join(['cc', '-std=c11', '-Wall', '-Wextra', '-Wpedantic',
                          '-Werror', '-UNDEBUG', '-Iinclude', source,
                          '-pthread', '-lm', '-ldl', '-o', 'mulib-example'])
    return '\n'.join([
        '## Source and build reference', '',
        '[Manual index](index.md) · [Integration guide](integration.md)', '',
        '| Resource | Location |', '| --- | --- |',
        '| Header | ' + link(record['header']) + ' |',
        '| Implementation switch | ' + switch + ' |',
        '| Repository-local includes | ' + (', '.join(deps) or 'None') + ' |',
        '| Complete example | ' + link(record['example']) + ' |', '',
        'The include list includes conditional and implementation-only includes;',
        'it is not a minimal link recipe. Use the example (and any setup header it',
        'includes) for a complete switch configuration. See the integration guide',
        'for dependency placement and platform-specific linking.', '',
        'Compile the example from the repository root on Linux:', '',
        '```sh', command, '```',
    ])


def interface(record):
    text = record['text']
    match = IMPLEMENTATION.search(text)
    if not match:
        raise ValueError('cannot excerpt a header without an implementation boundary: '
                         + record['header'].name)
    return ('## Public header reference\n\n'
            'Verbatim declaration section from ' + link(record['header']) + '.\n'
            'Include the header in application code; dependency types come from its includes.\n\n'
            '<details>\n<summary>Types, constants, and declarations</summary>\n\n'
            '```c\n' + text[:match.start()].rstrip() + '\n```\n\n</details>')


def index(records):
    lines = ['# MuLIB library index', '', '[Manuals](README.md) · [Modules by task](modules.md)', '',
             'Generated by `python3 tools/manuals.py --write` from the header inventory',
             'and manifest. Historical spellings are retained; their manuals explain',
             'canonical entry points and compatibility behavior.', '',
             f'{len(records)} library and support-header manuals. `virtio.h` is an additional',
             'entry point documented by [mulib-virtio](mulib-virtio.md).', '']
    groups = sorted({r['header'].stem.removeprefix('mulib-')[0].upper() for r in records})
    lines += [' · '.join('[' + g + '](#' + g.lower() + ')' for g in groups), '']
    for group in groups:
        lines += ['## ' + group, '', '| Library | Purpose | Source and example |', '| --- | --- | --- |']
        for record in sorted(records, key=lambda r: r['header'].stem.removeprefix('mulib-')):
            name = record['header'].stem.removeprefix('mulib-')
            if name[0].upper() != group:
                continue
            purpose = record['purpose'].replace('|', '\\|')
            lines.append('| [' + name + '](' + record['manual'].name + ') | ' + purpose
                         + ' | ' + link(record['header'], 'header') + ' · '
                         + link(record['example'], 'example') + ' |')
        lines.append('')
    return '\n'.join(lines)


def prose(text):
    # Strip fenced code: examples may contain Markdown-looking strings.
    return re.sub(r'^(`{3,}|~{3,})[^\n]*\n.*?^\1\s*$', '', text, flags=re.M | re.S)


def anchors(text):
    found, counts = set(), {}
    for heading in re.findall(r'^#{1,6}\s+(.+?)\s*#*$', prose(text), re.M):
        heading = re.sub(r'\[([^]]+)\]\([^)]*\)', r'\1', heading)
        base = re.sub(r'[^\w\- ]', '', heading.lower()).replace(' ', '-')
        number = counts.get(base, 0)
        found.add(base + ('-' + str(number) if number else ''))
        counts[base] = number + 1
    found.update(re.findall(r'(?:id|name)=["\']([^"\']+)["\']', text))
    return found


def check_links(documents):
    failures = []
    for path, content in documents.items():
        # Preserve backticked link labels, but ignore inline-code examples of
        # Markdown syntax such as `[label](URL)`.
        content = re.sub(r'(`+)(.+?)\1', lambda m: '' if '](' in m.group(2) else m.group(0),
                         prose(content))
        for destination in re.findall(r'\[[^\]\n]*\]\(([^\s)]+)(?:\s+"[^"]*")?\)', content):
            url = urlsplit(destination.strip('<>'))
            if url.scheme or url.netloc:
                continue
            target = (path.parent / unquote(url.path)).resolve() if url.path else path
            if not target.exists() and target not in documents:
                failures.append(f'{path.name}: missing link {destination}')
            elif url.fragment and target.suffix == '.md':
                text = documents[target] if target in documents else target.read_text()
                if unquote(url.fragment) not in anchors(text):
                    failures.append(f'{path.name}: missing anchor {destination}')
    return failures


def compile_examples(records, jobs):
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG',
             '-I' + str(ROOT / 'include')]
    flags += ['-pthread'] if os.name == 'posix' else []
    libraries = [] if sys.platform == 'win32' else ['-lm']
    libraries += ['-ldl'] if sys.platform.startswith(('linux', 'freebsd')) else []
    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-manuals-', dir=parent) as temporary:
        def compile_one(source):
            with tempfile.TemporaryDirectory(dir=temporary) as work:
                command = compiler + flags + [str(source)] + libraries + ['-o', str(Path(work) / 'example')]
                try:
                    result = subprocess.run(command, cwd=work, capture_output=True, text=True, timeout=180)
                    return (source.name + ':\n' + result.stdout + result.stderr) if result.returncode else None
                except (OSError, subprocess.TimeoutExpired) as error:
                    return source.name + ': ' + str(error)
        sources = sorted({r['example'] for r in records})
        with ThreadPoolExecutor(max_workers=jobs) as pool:
            failures = [failure for failure in pool.map(compile_one, sources) if failure]
        print(f'Compiled {len(sources) - len(failures)}/{len(sources)} manual examples')
        return failures


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--write', action='store_true', help='refresh only generated sections and index')
    mode.add_argument('--check', action='store_true', help='verify generated content (default)')
    parser.add_argument('--compile-examples', action='store_true', help='also compile/link examples, without running them')
    parser.add_argument('-j', '--jobs', type=int, default=min(4, os.cpu_count() or 1))
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    try:
        records = inventory()
        by_header = {r['header']: r for r in records}
        documents = {p: p.read_text() for p in (ROOT / 'manual').glob('*.md')}
        updates = {}
        for record in records:
            path = record['manual']
            text = documents[path]
            if marker('interface') in text or marker('interface', True) in text:
                text = replace_section(text, 'interface', interface(record))
            text = replace_section(text, 'reference', reference(record, by_header), append=True)
            updates[path] = text
        updates[ROOT / 'manual/index.md'] = index(records)
        stale = [p for p, text in updates.items() if documents.get(p) != text]
        documents.update(updates)
        failures = check_links(documents)
        if failures:
            parser.exit(1, '\n'.join(failures) + '\n')
        if args.write:
            for path in stale:
                path.write_text(updates[path])
            print(f'Refreshed {len(stale)} manual files')
        elif stale:
            parser.exit(1, 'Stale manual references: run python3 tools/manuals.py --write\n'
                        + '\n'.join(p.name for p in stale) + '\n')
        print(f'Manual inventory, references and local links: {len(records)} headers OK')
        if args.compile_examples:
            failures = compile_examples(records, args.jobs)
            if failures:
                parser.exit(1, '\n'.join(failures) + '\n')
    except (ValueError, KeyError, OSError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
