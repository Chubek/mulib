"""Embed the compiler-checked On1x template and validated declarative catalog."""
import json
from pathlib import Path
import sys


def unique(items):
    result = {}
    for key, value in items:
        if key in result:
            raise ValueError('duplicate GrammarEmbed catalog key')
        result[key] = value
    return result


catalog, template, output = map(Path, sys.argv[1:])
data = json.loads(catalog.read_text(), object_pairs_hook=unique)
if (set(data) != {'schema', 'messages', 'statuses', 'limits'} or
        data['schema'] != 'on1x.grammarembed/v1'):
    raise ValueError('invalid GrammarEmbed catalog')
if (set(data['statuses']) != {'ok', 'lexical', 'syntax', 'limit'} or
        len(set(data['statuses'].values())) != 4):
    raise ValueError('invalid GrammarEmbed status mapping')
if any(type(v) is not int or v <= 0 for v in data['limits'].values()):
    raise ValueError('invalid GrammarEmbed limit')
lines = ['// Generated from catalog.json and machinery.on1x.']
for group in ('messages', 'statuses', 'limits'):
    identifiers = set()
    for name, value in data[group].items():
        if not name or any(c not in 'abcdefghijklmnopqrstuvwxyz_-' for c in name):
            raise ValueError('invalid catalog key')
        normalized = name.replace('-', '_')
        if normalized in identifiers:
            raise ValueError('colliding catalog keys')
        identifiers.add(normalized)
        if group == 'messages':
            if not isinstance(value, str) or not value or any(ord(c) < 32 or ord(c) > 126 for c in value):
                raise ValueError('invalid catalog message')
        elif type(value) is not int or not 0 <= value <= 4194304:
            raise ValueError('invalid catalog integer')
        kind = 'const char*' if group == 'messages' else 'std::size_t'
        lines.append(f'inline constexpr {kind} {group}_{name.replace("-", "_")} = {json.dumps(value)};')
raw = template.read_bytes()
if not raw or b'\0' in raw or len(raw) > 1048576:
    raise ValueError('invalid GrammarEmbed template')
lines.append('static const unsigned char machinery[] = {')
for i in range(0, len(raw), 32):
    lines.append(','.join(map(str, raw[i:i+32])) + ',')
lines.append('0};')
output.write_text('\n'.join(lines) + '\n')
