#!/usr/bin/env python3
"""Record the existing Amb3r slice's source relationships, not conformance claims.

The 83-module implementation fixture defines the slice; the legacy ImGTK host is
also included. Profiles are quoted from manuals, limits from source. Runtime test
results and reviewed gaps belong in manual/amb3r-status.md.
"""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / 'manual/amb3r-inventory.json'


def inventory():
    fixture = ROOT / 'tests/amb3r_implementation.c'
    names = re.findall(r'^#include "mulib-(.*?)\.h"', fixture.read_text(), re.M)
    test_sources = {p.relative_to(ROOT).as_posix(): p.read_text()
                    for p in (ROOT / 'tests').glob('test_*')
                    if p.suffix in ('.c', '.cpp')}
    modules = []
    for name in sorted(set(names + ['immgtk'])):
        header = ROOT / f'include/mulib-{name}.h'
        manual = ROOT / f'manual/mulib-{name}.md'
        text, doc = header.read_text(), manual.read_text()
        public = text.split('#if defined(MULIB_IMPL_', 1)[0]
        profile = re.search(r'## Purpose and supported profile\n+(.*?)(?=\n## |\Z)',
                            doc, re.S)
        tests = sorted(path for path, source in test_sources.items()
                       if f'"mulib-{name}.h"' in source or
                       ('"amb3r_implementation.c"' in source and name in names))
        modules.append({
            'module': name,
            'owner': name.split('-')[0],
            'state': 'existing-limited',
            'presence': 'present',
            'capability': 'partial',
            'header': header.relative_to(ROOT).as_posix(),
            'source_sha256': hashlib.sha256(header.read_bytes()).hexdigest(),
            'manual': manual.relative_to(ROOT).as_posix(),
            'documented_profile': profile.group(1).strip() if profile else
                                  'See host manual; not part of the 83-module slice.',
            'dependencies': sorted(set(re.findall(r'#include "mulib-(.*?)\.h"', text))),
            'public_types': sorted(set(re.findall(r'\bmulib_\w+_t\b', public))),
            'resource_limit_source': [
                {'line': i, 'text': line.strip()} for i, line in enumerate(text.splitlines(), 1)
                if re.search(r'\[[1-9][0-9]*\]|SIZE_MAX|UINT\d+_MAX|0xffffff|^#define .*MAX_', line)
            ],
            'test_sources': tests,
            'example': f'examples/modules/{name}.c',
            'acceptance_gate': 'M0 baseline smoke only; see amb3r-status.md',
            'unverified_behavior': 'Full roadmap profile; source references are not runtime evidence',
            'next_dependency': 'Complete M0 behavioral audit; see reviewed owner map in amb3r-status.md',
        })
    return {
        'schema': 1,
        'baseline_sha': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT,
                                                text=True).strip(),
        'scope_fixture': fixture.relative_to(ROOT).as_posix(),
        'slice_module_count': len(names),
        'additional_modules': ['immgtk'],
        'repository_counts': {pattern: len(list(ROOT.glob(pattern))) for pattern in
                              ['include/mulib-*.h', 'tests/test_*.c', 'manual/mulib-*.md']},
        'interpretation': 'Extracted dependency/type/profile map. No inferred adapters, '
                          'runtime results, standards conformance, or missing-module claims.',
        'modules': modules,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--write', action='store_true')
    group.add_argument('--check', action='store_true')
    args = parser.parse_args()
    data = inventory()
    content = json.dumps(data, indent=2, ensure_ascii=False) + '\n'
    if args.write:
        OUTPUT.write_text(content)
    elif args.check:
        if not OUTPUT.exists() or OUTPUT.read_text() != content:
            parser.exit(1, 'Amb3r inventory is stale; run tools/audit_amb3r.py --write\n')
        print(f"Amb3r source inventory: {data['slice_module_count']} slice modules plus ImGTK host OK")
    else:
        print(content, end='')


if __name__ == '__main__':
    main()
