#!/usr/bin/env python3
"""Compile genuine LLSynth parse trees into immutable foreign descriptors.
No parser fallback, host commands, native calls or trust grants in this tool.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile
from typing import NamedTuple

MAX_BYTES = 64 * 1024
MAX_FILES = 8
MAX_ENTRIES = 256
MAX_PARAMS = 16
MAX_GUARDS = 16
MAX_TAGS = 128
MAX_TAG_ITEMS = 128
MAX_TYPE_DEPTH = 16
MAX_NATIVE_NODES = 2048
U64_MAX = (1 << 64) - 1
MAX_MODULES = 8192
MAX_BINDINGS = 32768
MAX_MANIFEST = 32 * 1024 * 1024
MAX_CATALOG_INDEX = 64 * 1024 * 1024
IDENT = re.compile(r'[A-Za-z_][A-Za-z_0-9]{0,95}\Z')
MODULE = re.compile(r'[a-z0-9][a-z0-9_+-]{0,95}\Z')
REASON = re.compile(r'[a-z][a-z0-9_]{0,95}\Z')
DIGEST = re.compile(r'[a-f0-9]{64}\Z')
UNIT = re.compile(r'unit_[0-9]{1,5}\Z')
TARGETS = {
    'x86_64-linux-gnu': ('LP64', 'SysVAMD64', 'sysv'),
    'aarch64-linux-gnu': ('LP64', 'AAPCS64', 'aapcs64'),
    'riscv64-linux-gnu': ('LP64', 'LP64', 'lp64'),
}
LEGACY_TARGET = ('x86_64-linux-gnu', *TARGETS['x86_64-linux-gnu'])
LEGACY_TYPE = ['integer', 'long', 4, 64, 'signed', 8, 8]
SURFACE = {
    'void': 'Void', 'i64': 'I64', 'u8': 'U8', 'u16': 'U16', 'u32': 'U32',
    'u64': 'U64', 'f32': 'F32', 'f64': 'F64', 'bool': 'Bool',
}
UNSIGNED = {'u8': 8, 'u16': 16, 'u32': 32, 'u64': 64}
INTEGER_LAYOUTS = {
    'bool': (0, 1, 'unsigned', 1, 1),
    'signed-char': (1, 8, 'signed', 1, 1),
    'unsigned-char': (1, 8, 'unsigned', 1, 1),
    'short': (2, 16, 'signed', 2, 2),
    'unsigned-short': (2, 16, 'unsigned', 2, 2),
    'int': (3, 32, 'signed', 4, 4),
    'unsigned-int': (3, 32, 'unsigned', 4, 4),
    'long': (4, 64, 'signed', 8, 8),
    'unsigned-long': (4, 64, 'unsigned', 8, 8),
    'long-long': (5, 64, 'signed', 8, 8),
    'unsigned-long-long': (5, 64, 'unsigned', 8, 8),
}
# Audited static-inline spelling in include/mulib-container-core.h.
SYMBOL_EXCEPTIONS = {('mulib.container_core', 'mulib_ct_counter_add')}
FRAMED_IPC_SYMBOLS = frozenset('on1x_kernel_ipc_v1_' + suffix for suffix in (
    'open_pair', 'roundtrip', 'open_session', 'register_callback',
    'revoke', 'send', 'poll', 'finish_send',
))

class Invalid(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise Invalid(message)


def read(path, maximum=MAX_BYTES):
    require(path.is_file() and not path.is_symlink(), f'not a regular non-symlink file: {path}')
    with path.open('rb') as f:
        data = f.read(maximum + 1)
    require(len(data) <= maximum, f'file too large: {path}')
    require(b'\0' not in data, f'embedded NUL: {path}')
    return data


def sha(data):
    return hashlib.sha256(data).hexdigest()


def compact(obj):
    return json.dumps(obj, ensure_ascii=True, separators=(',', ':')).encode('ascii')


def load_parser(path):
    spec = importlib.util.spec_from_file_location('on1x_ffi_generated_parser', path)
    require(spec is not None and spec.loader is not None, 'cannot load generated parser')
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def tokens(node):
    pending = [node]
    while pending:
        n = pending.pop()
        if n.children:
            pending.extend(reversed(n.children))
        elif n.token:
            yield n.token.lexeme


def nodes(node, symbol):
    pending = [node]
    while pending:
        n = pending.pop()
        if n.symbol == symbol:
            yield n
        else:
            pending.extend(reversed(n.children))


def ident(value):
    require(IDENT.fullmatch(value), f'invalid identifier: {value[:100]!r}')
    return value


def string(value):
    v = json.loads(value)
    require(isinstance(v, str), 'expected quoted string')
    return v


def lexeme(node):
    # LLSynth may wrap a single terminal in a nonterminal node.
    while node.token is None and len(node.children) == 1:
        node = node.children[0]
    require(node.token is not None, 'expected terminal')
    return node.token.lexeme


def parse_root(parser, raw, path):
    require(len(raw) <= MAX_BYTES and b'\0' not in raw, f'invalid input size or NUL: {path}')
    try:
        root = parser.parse(raw.decode('utf-8'))
    except (parser.LanguageError, UnicodeError) as exc:
        raise Invalid(f'{path}:{getattr(exc, "line", 1)}:{getattr(exc, "column", 1)}: {str(exc)[:300]}') from exc
    header, document, _ = root.children
    version = lexeme(header.children[1])
    require(version in ('1', '2', '3'), 'unsupported FFI schema version')
    if version == '1':
        require(document.children[0].symbol == 'Modules', 'expected ffi 2; before target')
        return 1, LEGACY_TARGET, document.children[0]
    require(document.children[0].symbol == 'Target', f'ffi {version} requires an explicit target')
    t = document.children[0].children
    target = string(lexeme(t[1])), lexeme(t[2]), string(lexeme(t[3])), lexeme(t[4])
    require(TARGETS.get(target[0]) == target[1:], 'unsupported or inconsistent target/data model/ABI')
    return int(version), target, document.children[1].children[0]


def module_name(c):
    require(lexeme(c[1]) == 'mulib', 'only canonical MuLIB namespace')
    alias = ident(lexeme(c[3]))
    require(REASON.fullmatch(alias), 'invalid module alias')
    return 'mulib.' + alias


def module_alias(key):
    # Matches the canonical manifest spelling rules, without importing the
    # retiring on1x-contracts tool into the descriptor compiler.
    alias = key.replace('-', '_').replace('+', '_plus_')
    return 'k_' + alias if alias[0].isdigit() else alias


def symbol_allowed(module, symbol):
    if not module.startswith('mulib.') or not IDENT.fullmatch(symbol):
        return False
    prefix = 'mulib_' + module.removeprefix('mulib.') + '_'
    return (symbol.startswith(prefix) and len(symbol) > len(prefix)) or (module, symbol) in SYMBOL_EXCEPTIONS


def symbol_name(node, module):
    symbol = ident(lexeme(node))
    require(symbol_allowed(module, symbol), 'C symbol outside canonical module prefix')
    return symbol


def number(s):
    require(len(s) <= 4 and (s == '0' or s[0] != '0'), 'noncanonical or out-of-bounds number')
    return int(s)


def reason(node):
    value = lexeme(node)
    require(REASON.fullmatch(value), 'invalid discovery reason code')
    return value


def native_type(node, target):
    atom, layout = node.children
    kind = lexeme(atom)
    fields = layout.children
    if kind == 'void' and not fields:
        return ['void']
    require(fields and kind in ('integer', 'real'), 'unsupported native type')
    identity = lexeme(fields[1])
    entries = [lexeme(n) for n in fields[3].children[::2]]
    if kind == 'integer':
        require(len(entries) == 5, 'invalid integer layout shape')
        rank, bits, sign, size, align = entries
        shape = number(rank), number(bits), sign, number(size), number(align)
        expected = INTEGER_LAYOUTS.get(identity)
        if identity == 'char':
            expected = (1, 8, 'signed' if target[0] == LEGACY_TARGET[0] else 'unsigned', 1, 1)
        require(shape == expected, 'native integer identity/layout mismatch')
    else:
        require(len(entries) == 4, 'invalid real layout shape')
        fmt, bits, size, align = entries
        shape = fmt, number(bits), number(size), number(align)
        expected = {'float': ('binary32', 32, 4, 4), 'double': ('binary64', 64, 8, 8),
                    'long-double': ('x87', 80, 16, 16) if target[0] == LEGACY_TARGET[0]
                    else ('binary128', 128, 16, 16)}
        require(shape == expected.get(identity), 'native real identity/layout mismatch')
    return [kind, identity, *shape]


def surface_type(node, result=False):
    surface = lexeme(node)
    require(surface in SURFACE and (surface != 'void' or result), 'unsupported surface type')
    return surface


def parameters(node):
    result = []
    names = set()
    for p in nodes(node, 'Param'):
        name = ident(lexeme(p.children[0]))
        require(len(result) < MAX_PARAMS and name not in names, 'parameter limit or duplicate')
        names.add(name)
        result.append((name, surface_type(p.children[2])))
    return result


def position_guards(node):
    result = []
    seen = set()
    for g in nodes(node, 'PositionGuard'):
        a, b = (number(lexeme(g.children[i])) for i in (1, 3))
        require(a < MAX_PARAMS and b < MAX_PARAMS, 'guard position out of bounds')
        require(len(result) < MAX_GUARDS, 'guard limit exceeded')
        require((a, b) not in seen, 'duplicate guard')
        seen.add((a, b))
        result.append([a, b])
    return result


def validate_guards(guards, params):
    for lhs, rhs in guards:
        require(lhs < len(params) and rhs < len(params), 'guard position out of bounds')
        a, b = params[lhs][1], params[rhs][1]
        require(a == b and (a == 'i64' or a in UNSIGNED), 'guard requires equal integral surface kinds')


def scalar_mapping(surface, native):
    if surface == 'void':
        return native == ['void']
    if surface == 'bool':
        return native[:2] == ['integer', 'bool']
    if surface == 'f32':
        return native[:3] == ['real', 'float', 'binary32']
    if surface == 'f64':
        return native[:3] == ['real', 'double', 'binary64']
    if native[0] != 'integer' or native[1] == 'bool':
        return False
    if surface == 'i64':
        return native[4] == 'signed' or native[3] < 64
    return native[4] == 'unsigned' and native[3] == UNSIGNED.get(surface)


def native_fingerprint(target, symbol, linkage, definition, parameters, result):
    return sha(compact(['on1x.ffi-native/v2', list(target), symbol, linkage, definition, parameters, result]))


def wire_uint(node, maximum=U64_MAX):
    text = lexeme(node)
    require(re.fullmatch(r'0|[1-9][0-9]{0,19}', text), 'invalid canonical graph integer')
    value = int(text)
    require(value <= maximum, 'graph integer overflow')
    return value


def layout(node):
    leaf = node.children[0]
    return None if lexeme(leaf) == 'unknown' else wire_uint(leaf)


def tag_name(node):
    value = lexeme(node.children[0])
    return None if value == 'none' else ident(string(value))


def rich_type(node, target, depth=1):
    require(depth <= MAX_TYPE_DEPTH, 'native type nesting limit exceeded')
    if node.symbol == 'RichType':
        node = node.children[0]
    p = node.children
    if node.symbol == 'RichPrimitive':
        return native_type(node, target)
    if node.symbol == 'TagRef':
        return ['tag-ref', wire_uint(p[2])]
    if node.symbol in ('Pointer', 'Array'):
        kind = node.symbol.lower()
        a, size, align = (layout(p[i]) for i in (2, 4, 6))
        if kind == 'pointer':
            require((a, size, align) == (64, 8, 8), 'invalid LP64 pointer layout')
        return [kind, a, size, align, rich_type(p[8], target, depth + 1)]
    if node.symbol == 'Qualified':
        quals = list(tokens(p[2]))[::2]
        require(quals == sorted(set(quals)) and set(quals) <= {'const', 'restrict', 'volatile'},
                'invalid or noncanonical qualifiers')
        inner = rich_type(p[4], target, depth + 1)
        require(inner[0] not in ('qualified', 'function-type')
                and ('restrict' not in quals or
                     (inner[0] == 'pointer' and base_type(inner[4])[0] != 'function-type')),
                'invalid qualified type')
        return ['qualified', quals, inner]
    require(node.symbol == 'RichFunction', 'invalid native type')
    mode = lexeme(p[2])
    require(mode in ('fixed', 'variadic', 'unprototyped'), 'unknown prototype kind')
    result = rich_type(p[8], target, depth + 1)
    params = []
    for param in nodes(p[5], 'RichType'):
        require(len(params) < MAX_PARAMS, 'native parameter limit exceeded')
        params.append(rich_type(param, target, depth + 1))
    require((mode != 'unprototyped' or not params) and (mode != 'variadic' or params),
            'invalid prototype arity')
    require(result[0] not in ('array', 'function-type', 'qualified')
            and all(t[0] not in ('void', 'array', 'function-type', 'qualified') for t in params),
            'invalid native function result or unadjusted parameter')
    return ['function-type', mode != 'unprototyped', mode == 'variadic', result, params]


def base_type(t):
    return t[2] if t[0] == 'qualified' else t


def read_tag(node, target):
    p = node.children
    tag_id, kind, name = wire_uint(p[1]), lexeme(p[2]), tag_name(p[3])
    require(kind in ('struct', 'union', 'enum'), 'invalid tag kind')
    body = p[4].children
    complete = lexeme(body[0]) == 'complete'
    data = dict(id=tag_id, kind=kind, name=name, complete=complete, layout=None,
                fields=[], underlying=None, enumerators=[])
    if not complete:
        require(name is not None, 'incomplete tag needs a name')
        return data
    size, align = (layout(body[i]) for i in (2, 4))
    if kind == 'enum':
        # Cephyr records the underlying integer, not a separate enum layout.
        require(size is None and align is None, 'enum layout is supplied by its underlying type')
    else:
        require(size is not None and align and not align & (align - 1)
                and align <= (1 << 32) - 1 and size % align == 0, 'invalid complete tag layout')
        data['layout'] = [size, align]
    names = set()
    for i, item in enumerate(nodes(body[7], 'TagItem')):
        require(i < MAX_TAG_ITEMS, 'tag item limit exceeded')
        item = item.children[0]
        q = item.children
        if item.symbol == 'Field':
            require(kind != 'enum', 'enum contains record field')
            name, offset, t = tag_name(q[1]), layout(q[3]), rich_type(q[5], target)
            flags = q[6].children
            flexible = bool(flags and lexeme(flags[0]) == 'flexible')
            bit = [layout(flags[j]) for j in (2, 4, 6)] if flags and not flexible else None
            require(offset is not None and (bit is None or None not in bit), 'unknown complete field layout')
            data['fields'].append(dict(name=name, offset=offset, type=t,
                                       bit_field=bit, flexible_array=flexible))
        elif item.symbol == 'Underlying':
            require(kind == 'enum' and i == 0, 'enum underlying type must occur once before cases')
            u = rich_type(q[1], target)
            require(u[0] == 'integer', 'invalid enum underlying type')
            data['underlying'] = u
            continue
        else:
            require(kind == 'enum' and data['underlying'] is not None, 'misplaced enum case')
            name = ident(string(lexeme(q[1])))
            sign_value = q[3].children
            n = wire_uint(sign_value[-1])
            if len(sign_value) == 2:
                require(n != 0, 'noncanonical negative zero')
                n = -n
            bits, signed = data['underlying'][3:5]
            require(-(1 << 31) <= n < (1 << 31) and (
                (-(1 << (bits - 1)) <= n < (1 << (bits - 1))) if signed == 'signed'
                else (0 <= n < 1 << bits)), 'enum value outside C int or underlying type')
            data['enumerators'].append([name, n])
        require(name is None or name not in names, 'duplicate tag member')
        names.add(name)
    require((kind != 'enum' and data['fields']) or data['enumerators'], 'empty complete tag')
    return data


def native_children(t):
    kind = t[0]
    if kind in ('pointer', 'array'):
        return [t[4]]
    if kind == 'qualified':
        return [t[2]]
    if kind == 'function-type':
        return [t[3], *t[4]]
    return []


def normalize_native_graph(root, tags):
    """W03 NativeGraph shape; local IDs are renumbered reachable result-first."""
    order, ids, normalized = [], {}, []
    remaining = MAX_NATIVE_NODES

    def visit(t):
        nonlocal remaining
        remaining -= 1
        require(remaining >= 0, 'native node limit exceeded')
        k = t[0]
        if k == 'tag-ref':
            old = t[1]
            require(old in tags, 'dangling tag reference')
            if old not in ids:
                ids[old] = len(order)
                order.append(old)
            return [k, ids[old]]
        if k in ('pointer', 'array'):
            return [*t[:4], visit(t[4])]
        if k == 'qualified':
            return [*t[:2], visit(t[2])]
        if k == 'function-type':
            return [*t[:3], visit(t[3]), [visit(p) for p in t[4]]]
        return t

    result = visit(root)
    for new, old in enumerate(order):
        tag = tags[old]
        fields = [dict(f, type=visit(f['type'])) for f in tag['fields']]
        underlying = visit(tag['underlying']) if tag['underlying'] else None
        normalized.append(dict(tag, id=new, fields=fields, underlying=underlying))
    return result, normalized


def validate_native_graph(tags, roots, target):
    """Validate every wire tag, including those not in a fingerprint."""
    def type_layout(t):
        t = base_type(t)
        if t[0] == 'tag-ref':
            tag = tags[t[1]]
            return tag['underlying'][-2:] if tag['underlying'] else tag['layout'] or (None, None)
        if t[0] in ('integer', 'real'):
            return t[-2:]
        if t[0] in ('pointer', 'array'):
            return t[2:4]
        return None, None

    pending = [*(t for t, _ in roots), *(f['type'] for tag in tags.values() for f in tag['fields'])]
    arrays = []
    while pending:
        t = pending.pop()
        if t[0] == 'tag-ref':
            require(t[1] in tags, 'dangling tag reference')
        if t[0] == 'array':
            arrays.append(t)
        pending.extend(native_children(t))

    active, checked, flexible = set(), set(), set()

    def record(tag_id):
        require(tag_id not in active, 'cyclic by-value tag layout')
        if tag_id in checked:
            return
        active.add(tag_id)
        tag = tags[tag_id]
        if tag['fields']:
            size, align = tag['layout']
            union = tag['kind'] == 'union'
            end, visible = 0, False
            for i, f in enumerate(tag['fields']):
                name, offset, t, bit, flex = (f[k] for k in
                    ('name', 'offset', 'type', 'bit_field', 'flexible_array'))
                t = base_type(t)
                dep = t
                while dep[0] == 'array':
                    dep = base_type(dep[4])
                if dep[0] == 'tag-ref':
                    record(dep[1])
                require(offset <= size and (not union or offset == 0), 'invalid record field offset')
                start = offset * 8
                if bit is not None:
                    width, shift, container = bit
                    require(t[0] == 'integer' and t[1] in ('bool', 'int', 'unsigned-int'),
                            'invalid bit-field base type')
                    require(width <= t[3] and (width > 0 or (name is None and shift == 0 and offset == container))
                            and container <= offset and container % t[-1] == 0
                            and (offset - container) * 8 + shift + width <= t[-2] * 8
                            and (not union or shift == 0)
                            and ((name is None and target[0] != 'aarch64-linux-gnu') or align >= t[-1]),
                            'invalid bit-field layout')
                    start += shift
                    stop = start + width
                else:
                    fs, fa = type_layout(t)
                    require(fa and align >= fa and offset % fa == 0, 'invalid record field alignment')
                    if flex:
                        require(not union and i == len(tag['fields']) - 1 and visible and name
                                and t[0] == 'array' and t[1:3] == [None, None], 'invalid flexible array member')
                        flexible.add(tag_id)
                        stop = start
                    else:
                        require(fs is not None, 'incomplete record field')
                        if t[0] == 'tag-ref' and t[1] in flexible:
                            require(union, 'structure embeds flexible array member')
                            flexible.add(tag_id)
                        stop = start + fs * 8
                    if name is None:
                        require(t[0] == 'tag-ref' and tags[t[1]]['kind'] in ('struct', 'union')
                                and tags[t[1]]['name'] is None, 'invalid anonymous record field')
                        visible = True
                visible |= name is not None
                require(stop <= size * 8 and (union or start >= end), 'record field overlaps or exceeds layout')
                end = stop
            require(visible, 'record lacks a named member')
        active.remove(tag_id)
        checked.add(tag_id)

    for tid in tags:
        record(tid)
    for _, count, size, align, element in arrays:
        es, ea = type_layout(element)
        elem = base_type(element)
        require(es is not None and ea == align and (elem[0] != 'tag-ref' or elem[1] not in flexible)
                and ((count is None and size is None)
                     or (count is not None and count * es == size)), 'invalid array layout')
    for root, definition in roots:
        if definition != 'declaration':
            require((root[3][0] == 'void' or type_layout(root[3])[0] is not None)
                    and all(type_layout(t)[0] is not None for t in root[4]),
                    'incomplete function definition type')


def native_graph_fingerprint(target, source, symbol, linkage, definition, specifiers, root, tags):
    root, tags = normalize_native_graph(root, tags)
    value = ['on1x.ffi-native-graph/v3', list(target), source, symbol, linkage,
             definition, sorted(specifiers), root, tags]
    return sha(json.dumps(value, ensure_ascii=True, sort_keys=True, separators=(',', ':')).encode('ascii'))


def parse_graph_module(body, mod):
    origin, graph_nodes, discoveries = body.children
    recipe = lexeme(origin.children[1]) if origin.children else None
    module, key, header, target = (mod[k] for k in ('module', 'provider', 'header', 'target'))

    def source_for(recipe):
        require(recipe is None or (recipe == 'framed_ipc_v1' and key == 'ipc-framed'
                                   and target == LEGACY_TARGET), 'unknown or inapplicable closed companion recipe')
        return ['companion', key, recipe] if recipe else ['public', key, header]

    default_source = source_for(recipe)
    graphs, roots, sources = {}, {}, {}
    for graph in nodes(graph_nodes, 'Graph'):
        gid = wire_uint(graph.children[1])
        require(len(graphs) < MAX_ENTRIES and gid not in graphs, 'duplicate graph ID or graph limit')
        provenance = graph.children[2].children
        require(not (provenance and recipe), 'graph repeats module companion recipe')
        sources[gid] = source_for(lexeme(provenance[1])) if provenance else default_source
        tables = graphs[gid] = {}
        roots[gid] = []
        for tag in nodes(graph.children[4], 'Tag'):
            require(len(tables) < MAX_TAGS, 'graph tag limit exceeded')
            data = read_tag(tag, target)
            require(data['id'] not in tables, 'duplicate tag ID')
            tables[data['id']] = data
    for d in nodes(discoveries, 'Discovery'):
        require(len(mod['entries']) < MAX_ENTRIES, 'entry limit exceeded')
        p = d.children
        member, symbol = ident(lexeme(p[1])), ident(lexeme(p[5]))
        linkage, definition = lexeme(p[7]), lexeme(p[8])
        require(linkage in ('internal', 'external') and definition in ('declaration', 'defined', 'inline-only'),
                'invalid discovery linkage or definition')
        gid = wire_uint(p[10])
        require(gid in graphs, 'unknown wire graph ID')
        source = sources[gid]
        require((symbol in FRAMED_IPC_SYMBOLS) if source[0] == 'companion' else symbol_allowed(module, symbol),
                'C symbol outside declaration source')
        root = rich_type(p[11], target)
        specifiers = list(tokens(p[12]))[2:-1]
        require(len(specifiers) == len(set(specifiers)) and set(specifiers) <= {'inline', 'noreturn'},
                'invalid function specifiers')
        require(definition != 'inline-only' or (linkage == 'external' and 'inline' in specifiers),
                'invalid inline-only declaration')
        roots[gid].append((root, definition))
        fingerprint = native_graph_fingerprint(target, source, symbol, linkage, definition, specifiers,
                                               root, graphs[gid])
        mod['entries'].append(dict(member=member, symbol=symbol, profile='discovery', reason=reason(p[14]),
                                   linkage=linkage, definition=definition, graph=gid, native_type=root,
                                   declaration_source=source,
                                   specifiers=sorted(specifiers), native_fingerprint=fingerprint))
    for gid, tags in graphs.items():
        require(roots[gid], 'unused wire graph')
        validate_native_graph(tags, roots[gid], target)
    mod.update(declaration_source=default_source, native_graphs=[
        dict(id=gid, tags=[tags[i] for i in sorted(tags)], declaration_source=sources[gid])
        for gid, tags in sorted(graphs.items())])
    return mod


def descriptor_id(row):
    return sha(compact(['on1x.ffi-id/v2', row['version'], row['module'], row['member'],
                        row['provider'], row['header'], row['native_fingerprint'],
                        [p['surface'] for p in row['parameters']], row['result'],
                        row['profile'], row['effects'], row['guards']]))


class OverrideDocument(NamedTuple):
    target: tuple[str, str, str, str]
    module: str
    overrides: dict
    selections: tuple[str, ...]
    obligations: tuple[str, ...]


def parse_overrides(parser, raw, path):
    """Read exact ownership selections, guard deltas and discovery obligations."""
    version, target, body = parse_root(parser, raw, path)
    require(version == 2 and body.symbol == 'Extension', 'expected a version 2 extend document')
    module = module_name(body.children)
    overrides, selections, obligations = {}, set(), set()
    for i, item in enumerate(nodes(body.children[5], 'OverrideItem')):
        require(i < MAX_ENTRIES, 'override entry limit exceeded')
        item = item.children[0]
        c = item.children
        if item.symbol == 'Selection':
            symbol = symbol_name(c[3], module)
            require(symbol not in selections, 'duplicate ownership selection')
            selections.add(symbol)
            continue
        if item.symbol == 'Obligation':
            code = reason(c[1])
            require(code not in obligations, 'duplicate discovery obligation')
            obligations.add(code)
            continue
        symbol = symbol_name(c[3], module)
        require(symbol not in overrides, 'duplicate override symbol')
        fingerprint = string(lexeme(c[5]))
        require(DIGEST.fullmatch(fingerprint), 'invalid override fingerprint')
        guards = position_guards(c[6])
        require(guards, 'override must add at least one guard')
        overrides[symbol] = dict(fingerprint=fingerprint, guards=guards)
    return OverrideDocument(target, module, overrides, tuple(sorted(selections)), tuple(sorted(obligations)))


def parse_module(m, version, target):
    c = m.children
    module = module_name(c)
    alias = module.removeprefix('mulib.')
    key = string(lexeme(c[7]))
    header = string(lexeme(c[10]))
    require(lexeme(c[6]) == 'mulib', 'only canonical MuLIB provider')
    require(MODULE.fullmatch(key) and alias == module_alias(key), 'module/provider mismatch')
    require(header == 'mulib-' + key + '.h', 'noncanonical header')
    mod = dict(module=module, provider=key, header=header, version=version, target=target,
               unavailable=None, entries=[])
    if version >= 2:
        body = c[12].children
        if body[0].symbol == 'GraphBody':
            require(version == 3, 'native graph requires ffi 3')
            return parse_graph_module(body[0], mod)
        if body[0].symbol != 'Entries':
            mod['unavailable'] = reason(body[2])
            if body[3].children:
                require(version == 3, 'audit text requires ffi 3')
                audit = string(lexeme(body[3].children[1]))
                require(0 < len(audit) <= 512 and audit.isprintable(), 'invalid discovery audit text')
                mod['audit'] = audit
            return mod
        require(version == 2, 'ffi 3 modules require a native graph or explicit unavailability')
    functions = (nodes(c[12], 'Function') if version == 1 else
                 (entry.children[0] for entry in nodes(c[12], 'Entry')))
    for f in functions:
        require(len(mod['entries']) < MAX_ENTRIES, 'entry limit exceeded')
        p = f.children
        if f.symbol == 'Unsupported':
            mod['entries'].append(dict(member=ident(lexeme(p[1])), symbol=symbol_name(p[5], module),
                                       profile='unsupported', reason=reason(p[7])))
            continue
        if version == 1:
            member = ident(lexeme(p[2]))
            params = parameters(p[4])
            result = surface_type(p[7], result=True)
            require(result == 'i64' and all(s == 'i64' for _, s in params), 'only i64 slots in ffi 1')
            symbol = symbol_name(p[11], module)
            linkage, definition = ('internal', 'defined') if p[0].children else ('external', 'declaration')
            native_params, native_result = [LEGACY_TYPE] * len(params), LEGACY_TYPE
            guards = []
            if p[12].children:
                guard = list(tokens(p[12]))
                names = [n for n, _ in params]
                require(guard[1] in names and guard[3] in names, 'guard refers to unknown parameter')
                guards.append([names.index(guard[1]), names.index(guard[3])])
            profile, effects = 'scalar', 'unknown'
        else:
            member = ident(lexeme(p[1]))
            params = parameters(p[3])
            result = surface_type(p[6], result=True)
            symbol = symbol_name(p[10], module)
            linkage, definition = lexeme(p[12]), lexeme(p[13])
            native_params = []
            for n in nodes(p[15], 'NativeType'):
                require(len(native_params) < MAX_PARAMS, 'native parameter limit exceeded')
                native_params.append(native_type(n, target))
            require(len(native_params) == len(params) and all(t != ['void'] for t in native_params),
                    'native/surface arity mismatch or void parameter')
            native_result = native_type(p[18], target)
            profile = lexeme(p[20])
            effects = lexeme(p[21].children[1]) if p[21].children else 'unknown'
            guards = position_guards(p[22])
        require((linkage, definition) in {('internal', 'defined'), ('external', 'declaration'),
                                         ('external', 'defined')}, 'unsupported native linkage/definition')
        if (module, symbol) in SYMBOL_EXCEPTIONS:
            require(version == 2 and linkage == 'internal', 'noncanonical symbol requires typed static-inline placement')
        require(profile in ('scalar', 'discovery') and effects == 'unknown', 'unsupported profile or effect')
        validate_guards(guards, params)
        if profile == 'scalar':
            require(scalar_mapping(result, native_result) and all(
                scalar_mapping(s, t) for (_, s), t in zip(params, native_params)),
                'unsupported native/surface scalar mapping')
        else:
            require(not guards, 'discovery entries cannot have a marshalling plan')
        row = dict(module=module, provider=key, header=header, version=version, target=target,
                   member=member, symbol=symbol, linkage=linkage, definition=definition,
                   parameters=[dict(name=n, surface=s, native=t) for (n, s), t in zip(params, native_params)],
                   result=result, native_result=native_result, profile=profile, effects=effects, guards=guards,
                   native_fingerprint=native_fingerprint(target, symbol, linkage, definition, native_params, native_result))
        if profile == 'scalar':
            row['id'] = descriptor_id(row)
        mod['entries'].append(row)
    return mod


def parse_document(parser, raw, path):
    version, target, body = parse_root(parser, raw, path)
    require(body.symbol != 'Extension', 'unmerged override is not a generated module')
    return (parse_module(m, version, target) for m in (nodes(body, 'Module') if version == 1 else [body]))


def compile_files(parser, paths, target=None, canonical_files=False):
    require(0 < len(paths) <= MAX_FILES, 'invalid number of source files')
    modules, seen_symbols, rows, inputs, discoveries = [], set(), [], [], []
    seen_modules = set()
    for path in paths:
        raw = read(path)
        inputs.append({'file': path.name, 'sha256': sha(raw)})
        for mod in parse_document(parser, raw, path):
            module = mod['module']
            require(len(modules) < MAX_FILES and module not in seen_modules, 'module limit or duplicate module: '+module)
            if canonical_files:
                require(path.name == mod['provider'] + '.ffi', 'noncanonical module filename: ' + path.name)
            if target is None:
                target = mod['target'][0]
            require(mod['target'][0] == target, 'descriptor target mismatch')
            seen_modules.add(module)
            entries = mod.pop('entries')
            modules.append(mod)
            members = set()
            for row in entries:
                member, symbol = row['member'], row['symbol']
                require(member not in members, 'duplicate member: ' + member)
                require(symbol not in seen_symbols, 'duplicate native symbol: ' + symbol)
                members.add(member)
                seen_symbols.add(symbol)
                if row['profile'] == 'scalar':
                    rows.append(row)
                else:
                    discoveries.append(dict(row, module=module))
    for values in (rows, discoveries):
        values.sort(key=lambda r: (r['module'], r['member']))
    return dict(target=target, bindings=rows, discovery=discoveries,
                modules=sorted(modules, key=lambda m: m['module']),
                inputs=sorted(inputs, key=lambda v: (v['file'], v['sha256'])))


def canonical(obj):
    return json.dumps(obj, sort_keys=True, indent=2, ensure_ascii=True) + '\n'


def braces(fields):
    return '{' + ', '.join(fields) + '}'


def cpp_type(t):
    if t[0] == 'integer':
        fields = ['NativeKind::Integer', json.dumps(t[1]), '""', str(t[2]), str(t[3]),
                  str(t[5]), str(t[6]), str(t[4] == 'signed').lower()]
    elif t[0] == 'real':
        fields = ['NativeKind::Real', json.dumps(t[1]), json.dumps(t[2]), '0',
                  str(t[3]), str(t[4]), str(t[5]), 'false']
    else:
        fields = ['NativeKind::Void', '"void"', '""', '0', '0', '0', '0', 'false']
    return braces(fields)


def array(typ, name, items):
    return [f'inline constexpr std::array<{typ}, {len(items)}> {name} = {{' +
            braces(items) + '};']


def prelude():
    return ['// Generated by FFIHandler; do not edit.', '#pragma once', '#include <array>',
            '#include <cstdint>', '#include <on1x/ffi.hpp>']


def emit_indexes(rows, module_order=None):
    if module_order is None:
        module_order = sorted(range(len(rows)), key=lambda i: (rows[i]['module'], rows[i]['member']))
    lines = []
    for key, order in [('member', module_order)] + [
        (k, sorted(range(len(rows)), key=lambda i: rows[i][k])) for k in ('symbol', 'id')
    ]:
        lines.append(f'inline constexpr std::array<std::uint32_t, {len(order)}> by_{key} = {{{{')
        for offset in range(0, len(order), 32):
            lines.append('    ' + ', '.join(map(str, order[offset:offset + 32])) + ',')
        lines.append('}};')
    return lines


def emit_header(rows, digest, unit=None):
    lines = prelude() + ['namespace on1x::ffi::generated' + (f'::{unit}' if unit else '') + ' {',
                         f'inline constexpr char descriptor_sha256[] = "{digest}";']
    bindings = []
    for i, r in enumerate(rows):
        params, guards = f'p{i}', f'g{i}'
        lines += array('Parameter', params, [braces([json.dumps(p['name']), 'SurfaceKind::' + SURFACE[p['surface']],
                                                     cpp_type(p['native'])]) for p in r['parameters']])
        lines += array('Guard', guards, [braces(map(str, pair)) for pair in r['guards']])
        fields = [json.dumps(r[key]) for key in ('module', 'provider', 'header', 'member', 'symbol')]
        fields += [braces(map(json.dumps, r['target'])),
                   'Placement::StaticInline' if r['linkage'] == 'internal' else 'Placement::External',
                   json.dumps(r['definition']), params, 'SurfaceKind::' + SURFACE[r['result']],
                   cpp_type(r['native_result']), braces([guards]), 'Effect::Unknown',
                   json.dumps(r['native_fingerprint']), braces([json.dumps(r['id'])]), str(r['version'])]
        bindings.append('    ' + braces(fields) + ',')
    lines += [f'inline constexpr std::array<Descriptor, {len(rows)}> bindings = {{{{', *bindings, '}};']
    if unit is None:
        lines += emit_indexes(rows)
    return '\n'.join([*lines, '}', ''])


def unique_pairs(pairs):
    fields = {}
    for key, value in pairs:
        require(key not in fields, 'duplicate index field')
        fields[key] = value
    return fields


def verify_catalogue(path, inputs, target):
    raw = read(path, MAX_CATALOG_INDEX)
    data = json.loads(raw, object_pairs_hook=unique_pairs)
    require(data.get('schema') == 'on1x.stdffi.mulib/v1' and data.get('target') == target,
            'catalogue index schema or target mismatch')
    modules = data['modules']
    require(isinstance(modules, list) and 0 < len(modules) <= MAX_MODULES, 'invalid catalogue index module count')
    files = {}
    for mod in modules:
        key, name, digest = (mod[field] for field in ('key', 'file', 'ffi_sha256'))
        require(MODULE.fullmatch(key) and name == key + '.ffi' and name not in files
                and DIGEST.fullmatch(digest), 'invalid catalogue index file identity')
        files[name] = digest
    require(inputs == files, 'stale catalogue: index and descriptor input files/digests differ')
    return sha(raw)


def registry_index(paths, target=None, catalogue=None):
    require(0 < len(paths) <= MAX_MODULES, 'invalid registry shard count')
    rows, units, fingerprints = [], [], []
    inputs = {}
    seen_modules, seen_symbols, seen_ids, seen_units = set(), set(), set(), set()
    for path in sorted(paths, key=lambda p: p.name):
        raw = read(path, MAX_MANIFEST)
        payload = json.loads(raw, object_pairs_hook=unique_pairs)
        unit = payload.get('unit', '')
        require(payload.get('schema') == 'on1x.ffi-descriptors/v2' and UNIT.fullmatch(unit)
                and unit not in seen_units and path.name == unit + '.json', 'invalid or duplicate registry shard')
        seen_units.add(unit)
        shard_digest = payload.pop('descriptor_sha256', None)
        require(shard_digest == sha(canonical(payload).encode()), 'descriptor shard digest mismatch')
        if target is None:
            target = payload['target']
        require(target in TARGETS and payload['target'] == target, 'registry shard target mismatch')
        for inp in payload['inputs']:
            name, digest = inp['file'], inp['sha256']
            require(name not in inputs, 'duplicate registry input filename: ' + name)
            inputs[name] = digest
        for mod in payload['modules']:
            module = mod['module']
            require(module not in seen_modules and len(seen_modules) < MAX_MODULES, 'registry module collision or limit')
            seen_modules.add(module)
        for row in (*payload['bindings'], *payload['discovery']):
            symbol = row['symbol']
            require(symbol not in seen_symbols, 'registry native symbol collision: ' + symbol)
            seen_symbols.add(symbol)
        if payload['bindings']:
            units.append(unit)
        for row in payload['bindings']:
            id_ = row['id']
            require(id_ not in seen_ids and len(rows) < MAX_BINDINGS, 'registry foreign ID collision or limit')
            seen_ids.add(id_)
            rows.append({key: row[key] for key in ('module', 'member', 'symbol', 'id')})
        fingerprints.append(dict(unit=unit, descriptor_sha256=shard_digest))
    data = dict(schema='on1x.ffi-registry/v2', target=target, module_count=len(seen_modules),
                binding_count=len(rows), shards=fingerprints, grants_safe_admission=False)
    if catalogue:
        data['catalogue_index_sha256'] = verify_catalogue(catalogue, inputs, target)
    digest = sha(canonical(data).encode())
    data['descriptor_sha256'] = digest
    lines = prelude() + [f'#include "{unit}.hpp"' for unit in units]
    lines += ['namespace on1x::ffi::generated {',
              f'inline constexpr char descriptor_sha256[] = "{digest}";',
              f'inline constexpr auto bindings = [] {{ std::array<Descriptor, {len(rows)}> all{{}};']
    if units:
        lines += ['    std::size_t pos = 0;',
                  *(f'    for (const auto& entry : {u}::bindings) all[pos++] = entry;' for u in units)]
    lines += ['    return all;', '}();', *emit_indexes(rows), '}', '']
    return '\n'.join(lines), data


def write_atomic(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(prefix=path.name+'.', dir=path.parent)
    try:
        with os.fdopen(fd, 'w', encoding='utf-8', newline='\n') as f:
            f.write(data)
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--parser', type=Path, help='trusted LLSynth-generated Python parser')
    ap.add_argument('--index', action='store_true', help='index built descriptor shards (no DSL input)')
    ap.add_argument('--unit', help='build unit name for registry indexing')
    ap.add_argument('--target', choices=TARGETS, help='require this target in the table')
    ap.add_argument('--canonical-files', action='store_true', help='require canonical module filenames')
    ap.add_argument('--catalogue-index', type=Path, help='check source catalogue inventory for --index')
    ap.add_argument('--header', required=True, type=Path)
    ap.add_argument('--manifest', required=True, type=Path)
    ap.add_argument('sources', nargs='+', type=Path)
    a = ap.parse_args(argv)
    try:
        require(a.header.resolve() != a.manifest.resolve(), 'output paths must differ')
        require(not any(x and x.resolve() in {a.header.resolve(), a.manifest.resolve()}
                        for x in [*a.sources, a.parser, a.catalogue_index]), 'output overlaps input')
        if a.index:
            require(not a.parser and not a.unit and not a.canonical_files, 'index takes built shard manifests only')
            header, payload = registry_index(a.sources, a.target, a.catalogue_index)
        else:
            require(a.parser is not None and not a.catalogue_index, 'a genuine LLSynth parser is required for DSL input')
            if a.unit:
                require(UNIT.fullmatch(a.unit) and a.header.name == a.unit + '.hpp'
                        and a.manifest.name == a.unit + '.json', 'invalid unit/output name')
            payload = compile_files(load_parser(a.parser), a.sources, a.target, a.canonical_files)
            payload.update(schema='on1x.ffi-descriptors/v2', unit=a.unit,
                           parser_sha256=sha(a.parser.read_bytes()), generator_sha256=sha(Path(__file__).read_bytes()),
                           grants_safe_admission=False)
            digest = sha(canonical(payload).encode())
            payload['descriptor_sha256'] = digest
            header = emit_header(payload['bindings'], digest, a.unit)
        # Validate the entire package before replacing either output. Pair is not a filesystem transaction.
        write_atomic(a.header, header)
        write_atomic(a.manifest, canonical(payload))
    except (OSError, ValueError, KeyError, TypeError, AttributeError, RecursionError) as exc:
        print(f'FFIHandler: {exc}', file=sys.stderr)
        return 1
    return 0

if __name__=='__main__':
    raise SystemExit(main())
