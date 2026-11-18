"""Build-only resources and the canonical single-TU Mune implementation."""
import json
from pathlib import Path
import sys


def unique(pairs):
    out = {}
    for key, value in pairs:
        if key in out:
            raise ValueError('duplicate LuaEmbed resource key')
        out[key] = value
    return out


def generate(root, output):
    package = root / 'on1x/stdppx/luaembed'
    data = json.loads((package / 'messages.json').read_text(), object_pairs_hook=unique)
    if (set(data) != {'schema', 'messages'} or data['schema'] != 'on1x.luaembed-messages/v1'
            or not isinstance(data['messages'], dict) or 'internal' not in data['messages']):
        raise ValueError('invalid LuaEmbed messages')
    for key, value in data['messages'].items():
        if (not key or not all(c in 'abcdefghijklmnopqrstuvwxyz-' for c in key)
                or not isinstance(value, str) or not value or len(value) > 4096
                or any(ord(c) < 32 or ord(c) > 126 for c in value)):
            raise ValueError('invalid LuaEmbed message')
    lua = (package / 'llembed.lua').read_bytes()
    if not lua or len(lua) > 65536 or b'\0' in lua:
        raise ValueError('invalid llembed library')
    output.mkdir(parents=True, exist_ok=True)
    lines = ['/* Generated from llembed.lua and messages.json. */',
             'static const unsigned char ll_library[] = {']
    for pos in range(0, len(lua), 32):
        lines.append(','.join(map(str, lua[pos:pos+32])) + ',')
    lines += ['0};', 'static const struct { const char *key, *text; } ll_messages[] = {']
    for key, value in sorted(data['messages'].items()):
        lines.append('{' + json.dumps(key) + ',' + json.dumps(value) + '},')
    lines.append('};')
    (output / 'luaembed_resources.h').write_text('\n'.join(lines) + '\n')
    sys.path.insert(0, str(root / 'on1x/stdlib/mulib'))
    import housekeeping
    recipe = housekeeping.plan(root, ['mune'])
    if recipe['native_sources'] or recipe['link_libraries'] != ['dl', 'm', 'pthread']:
        raise ValueError('Mune recipe changed; review LuaEmbed linkage')
    (output / 'luaembed_impl.c').write_text(housekeeping.implementation(root, recipe))
    (output / 'luaembed_plan.json').write_text(json.dumps(recipe, indent=2, sort_keys=True) + '\n')


if __name__ == '__main__':
    generate(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())
