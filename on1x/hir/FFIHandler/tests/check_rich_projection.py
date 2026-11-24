#!/usr/bin/env python3
"""Qualification bridge: check actual W01/W03 graph identity against W05.

Consumes existing Cephyr captures; this is not an inventory generator.
"""
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import ffic


def literal(n):
    return 'unknown' if n is None else str(n)


def name(s):
    return 'none' if s is None else json.dumps(s)


def typ(t):
    kind = t[0]
    if kind == 'void':
        return kind
    if kind in ('integer', 'real', 'tag-ref'):
        items = list(map(str, t[1:]))
    elif kind in ('array', 'pointer'):
        items = [*map(literal, t[1:4]), typ(t[4])]
    elif kind == 'qualified':
        items = ['+'.join(t[1]), typ(t[2])]
    elif kind == 'function-type':
        mode = 'variadic' if t[2] else ('fixed' if t[1] else 'unprototyped')
        items = [mode, '(' + ','.join(map(typ, t[4])) + ')', typ(t[3])]
    else:
        raise AssertionError(t)
    return f'{kind}(' + ','.join(items) + ')'


def tag_text(tag):
    prefix = f'tag {tag["id"]} {tag["kind"]} {name(tag["name"])}'
    if not tag['complete']:
        return prefix + ' incomplete;'
    size, align = tag['layout'] or (None, None)
    items = []
    if tag['underlying']:
        items.append('underlying ' + typ(tag['underlying']) + ';')
    for field in tag['fields']:
        suffix = ''
        if field['bit_field']:
            suffix = ' bit-field(' + ','.join(map(literal, field['bit_field'])) + ')'
        elif field['flexible_array']:
            suffix = ' flexible'
        items.append(f'field {name(field["name"])} at {literal(field["offset"])} : {typ(field["type"])}{suffix};')
    items.extend(f'case {json.dumps(k)} = {v};' for k, v in tag['enumerators'])
    return prefix + f' complete ({literal(size)},{literal(align)}) {{ ' + ' '.join(items) + ' }'


def verify(parser, capture, key, companion):
    from cephyr_interface import decode_interface
    from ffi_types import NativeGraph, digest

    raw = capture.read_bytes()
    interface = decode_interface(raw)
    target = interface.target, interface.data_model, interface.abi, ffic.TARGETS[interface.target][2]
    alias, header = ffic.module_alias(key), f'mulib-{key}.h'
    source = ['companion', key, companion] if companion else ['public', key, header]
    selected = [f for f in interface.functions if (
        f.name in ffic.FRAMED_IPC_SYMBOLS if companion else f.name.startswith(f'mulib_{alias}_'))]
    assert selected
    if companion:
        assert {f.name for f in selected} == ffic.FRAMED_IPC_SYMBOLS
    expected = {}
    graph = NativeGraph(interface.tags)
    roots = {f.name: graph.type(f.type) for f in sorted(selected, key=lambda f: f.name)}
    tags = graph.tags()
    document = [f'ffi 3; target "{target[0]}" {target[1]} "{target[2]}" {target[3]};',
                f'foreign mulib.{alias} {{ provider mulib "{key}"; header "{header}";']
    if companion:
        document.append(f'companion {companion};')
    document += ['graph 0 {', *map(tag_text, tags), '}']
    for f in selected:
        single = NativeGraph(interface.tags)
        single_root, single_tags = single.type(f.type), single.tags()
        expected[f.name] = digest(['on1x.ffi-native-graph/v3', list(target), source, f.name,
                                  f.linkage, f.definition, sorted(f.specifiers), single_root, single_tags])
        specs = ' specifiers(' + ' '.join(f.specifiers) + ')' if f.specifiers else ''
        document.append(f'discover {f.name} = c::{f.name} native {f.linkage} {f.definition} graph 0 '
                        f'{typ(roots[f.name])}{specs} reason rich_lowering_unavailable;')
    document.append('}')
    mod, = ffic.parse_document(parser, '\n'.join(document).encode(), capture)
    assert mod['native_graphs'] == [{'id': 0, 'tags': tags, 'declaration_source': source}], capture
    assert {e['symbol']: e['native_fingerprint'] for e in mod['entries']} == expected, capture
    assert all(e['profile'] == 'discovery' and 'id' not in e for e in mod['entries'])
    print(f'{key} {target[0]} projection={ffic.sha(raw)} tags={len(tags)} discoveries={len(expected)}')
    for symbol, fingerprint in sorted(expected.items()):
        print(f'  {symbol}: {fingerprint}')


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--parser', type=Path, required=True)
    ap.add_argument('--decoder-tools', type=Path, required=True)
    ap.add_argument('--generator-tools', type=Path, required=True)
    ap.add_argument('--capture', action='append', nargs=2, metavar=('KERNEL', 'INTERFACE'), default=[])
    ap.add_argument('--ipc-companion', type=Path)
    args = ap.parse_args()
    sys.path[:0] = [str(args.decoder_tools.resolve()), str(args.generator_tools.resolve())]
    parser = ffic.load_parser(args.parser)
    assert args.capture or args.ipc_companion, 'qualification requires real projection inputs'
    for key, path in args.capture:
        verify(parser, Path(path), key, None)
    if args.ipc_companion:
        verify(parser, args.ipc_companion, 'ipc-framed', 'framed_ipc_v1')
