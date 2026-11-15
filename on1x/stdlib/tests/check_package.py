#!/usr/bin/env python3
"""Static sanity checks only. NOT a substitute for compiling with on1xc."""
from pathlib import Path
import json
import re
import sys
root=Path(__file__).resolve().parents[1]
catalog=json.loads((root/'CATALOG.json').read_text())
modules=sorted(root.rglob('*.on1x'))
assert len(modules)>=60, len(modules)
assert len(catalog['required_signatures'])==40
assert len(catalog['required_structures'])==43
assert len(catalog['optional_signatures'])==24
assert len(catalog['optional_structures'])==64
assert len(catalog['optional_functors'])==3
entries=catalog['components']
assert len(entries)==186,len(entries)
assert len({(e['kind'],e['name']) for e in entries})==len(entries)
exported={}
for module in modules:
    source=module.read_text()
    for relative in re.findall(r'(?m)^\s*import\s+"([^"]+)";',source):
        path=(module.parent/relative).resolve()
        assert path.is_file(),f'{module}: unresolved import {relative}'
        assert root in path.parents, f'{module}: import escapes basis root'
    for symbol in re.findall(r'(?m)^\s*(?:extern\s+)?fn\s+([A-Za-z_]\w*)\(',source):
        if symbol=='main':continue
        if symbol in exported:raise AssertionError(f'duplicate {symbol}: {module}, {exported[symbol]}')
        exported[symbol]=module
    # lexical balance, ignoring On1x comments and strings
    stack=[];quote=False;escaped=False
    for line in source.splitlines():
        for c in line:
            if quote:
                if escaped:escaped=False
                elif c=='\\':escaped=True
                elif c=='"':quote=False
                continue
            if c=='#':break
            if c=='"':quote=True;continue
            if c in '({':stack.append(c)
            if c in ')}':
                assert stack and stack.pop()==('(' if c==')' else '{'),module
        assert not quote,module
    assert not stack,module
for e in entries:
    if e['on1x_module']:
        path=root.parents[1]/e['on1x_module']
        assert path.is_file(),(e['name'],path)
print(f'catalog: {len(entries)} entries, {len(modules)} On1x files, {len(exported)} API declarations')
print('static checks passed (not a compiler test)')
