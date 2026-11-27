#!/usr/bin/env python3
"""Refresh relocated catalog paths, source counts and reproducible file hashes.

Compatibility statuses are retained verbatim. This is inventory maintenance,
not a conformance or foreign-safety certification.
"""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]

def relocate(value):
    if isinstance(value, str):
        return value.replace('on1x/stdlib/basis/', 'on1x/stdlib/')
    if isinstance(value, list):
        return [relocate(x) for x in value]
    if isinstance(value, dict):
        return {key: relocate(x) for key, x in value.items()}
    return value

def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')

for path in [root/'CATALOG.json', *sorted((root/'catalog').rglob('*.json'))]:
    write(path, relocate(json.loads(path.read_text())))
summary = json.loads((root/'CATALOG_SUMMARY.json').read_text())
sources = [path for suffix in ('*.on1x', '*.on1xm') for path in root.rglob(suffix)]
test_sources = sum('tests' in p.relative_to(root).parts or
                   p.relative_to(root).parts[:2] == ('daemon', 'fixtures')
                   for p in sources)
summary.update(actual_on1x_modules=len(sources),
                actual_on1x_library_modules=len(sources) - test_sources,
                on1x_test_sources=test_sources)
write(root/'CATALOG_SUMMARY.json', summary)
files = []
for path in sorted(root.rglob('*')):
    if not path.is_file() or path.name == 'FILE_HASHES.json' or '__pycache__' in path.parts or path.suffix == '.pyc':
        continue
    data = path.read_bytes()
    files.append(dict(path=path.relative_to(root).as_posix(), bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
write(root/'FILE_HASHES.json', dict(schema='on1x-stdlib-files/v1', files=files))
