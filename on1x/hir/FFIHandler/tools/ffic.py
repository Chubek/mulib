#!/usr/bin/env python3
"""Compile genuine LLSynth parse trees into immutable scalar descriptors.
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

MAX_BYTES = 64 * 1024
MAX_FILES = 64
MAX_EXPORTS = 256
MAX_PARAMS = 16
IDENT = re.compile(r'[A-Za-z_][A-Za-z_0-9]{0,95}\Z')
MODULE = re.compile(r'[a-z][a-z0-9-]{0,95}\Z')
HEADER = re.compile(r'mulib-[a-z0-9][a-z0-9-]{0,95}\.h\Z')

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
    require(IDENT.fullmatch(value), f'invalid identifier: {value!r}')
    return value


def string(value):
    v = json.loads(value)
    require(isinstance(v, str), 'expected quoted string')
    return v


def parse_document(parser, raw, path):
    try:
        root = parser.parse(raw.decode('utf-8'))
    except (parser.LanguageError, UnicodeError) as exc:
        raise Invalid(f'{path}:{getattr(exc, "line", 1)}:{getattr(exc, "column", 1)}: {exc}') from exc
    require(list(tokens(root.children[0])) == ['ffi', '1', ';'], 'expected ffi 1;')
    modules = []
    for m in nodes(root.children[1], 'Module'):
        c = m.children
        # The grammar controls the tree layout. Never re-tokenize source text.
        require(len(c) == 14, 'unexpected Module parse tree schema')
        namespace, name = ident(next(tokens(c[1]))), ident(next(tokens(c[3])))
        provider = next(tokens(c[6]))
        key = string(next(tokens(c[7])))
        header = string(next(tokens(c[10])))
        require(namespace == 'mulib' and provider == 'mulib', 'only canonical MuLIB provider in v1')
        require(MODULE.fullmatch(key) and name == key.replace('-', '_'), 'module/provider mismatch')
        require(HEADER.fullmatch(header) and header == 'mulib-' + key + '.h', 'noncanonical header')
        exports = []
        for f in nodes(c[12], 'Function'):
            p = f.children
            require(len(p) == 14, 'unexpected Function parse tree schema')
            member = ident(next(tokens(p[2])))
            params = []
            for param in nodes(p[4], 'Param'):
                t = list(tokens(param))
                require(len(t) == 3 and t[2] == 'i64', 'only i64 parameters in v1')
                params.append(ident(t[0]))
            require(len(params) <= MAX_PARAMS and len(set(params)) == len(params), 'parameter limit or duplicate')
            require(next(tokens(p[7])) == 'i64', 'only i64 results in v1')
            symbol = ident(next(tokens(p[11])))
            require(symbol.startswith('mulib_' + name + '_'), 'C symbol outside canonical module prefix')
            guard = list(tokens(p[12]))
            indices = []
            if guard:
                require(len(guard) == 4 and guard[0] == 'requires' and guard[2] == '<=', 'unsupported guard')
                require(guard[1] in params and guard[3] in params, 'guard refers to unknown parameter')
                indices = [params.index(guard[1]), params.index(guard[3])]
            exports.append((member, params, symbol, bool(list(tokens(p[0]))), indices))
        modules.append((namespace+'.'+name,key,header,exports))
    return modules


def compile_files(parser, paths):
    require(0 < len(paths) <= MAX_FILES, 'invalid number of source files')
    seen_modules, seen_symbols, rows, inputs = set(), set(), [], []
    for path in paths:
        raw = read(path)
        inputs.append({'file': path.name, 'sha256': sha(raw)})
        for module, provider, header, exports in parse_document(parser, raw, path):
            require(module not in seen_modules, 'duplicate module: '+module)
            seen_modules.add(module)
            members = set()
            for member, params, symbol, inline, guard in exports:
                require(member not in members, 'duplicate member: '+member)
                require(symbol not in seen_symbols, 'duplicate native symbol: '+symbol)
                members.add(member)
                seen_symbols.add(symbol)
                rows.append(dict(module=module, provider=provider, header=header, member=member,
                                 symbol=symbol, parameters=params, result='i64', inline=inline,
                                 guard=guard, safety='representation_only'))
                require(len(rows) <= MAX_EXPORTS, 'export limit exceeded')
    require(rows, 'empty descriptor package')
    return sorted(rows, key=lambda r:(r['module'],r['member'])), sorted(inputs, key=lambda x:(x['file'],x['sha256']))


def canonical(obj):
    return json.dumps(obj, sort_keys=True, indent=2, ensure_ascii=True) + '\n'


def emit_header(rows, digest):
    lines = ['// Generated by FFIHandler; do not edit.', '#pragma once', '#include <array>',
             '#include <on1x/ffi_scalar.hpp>', 'namespace on1x::ffi::generated {',
             f'inline constexpr char descriptor_sha256[] = "{digest}";',
             f'inline constexpr std::array<ScalarBinding, {len(rows)}> bindings = {{{{']
    for r in rows:
        g = r['guard'] or [-1,-1]
        fields = [json.dumps(r[k]) for k in ('module','provider','header','member','symbol')]
        fields += [str(len(r['parameters'])), str(r['inline']).lower(), str(g[0]), str(g[1])]
        lines.append('    {' + ', '.join(fields) + '},')
    lines += ['}};', '} // namespace on1x::ffi::generated', '']
    return '\n'.join(lines)


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
    ap.add_argument('--parser', required=True, type=Path, help='trusted LLSynth-generated Python parser')
    ap.add_argument('--header', required=True, type=Path)
    ap.add_argument('--manifest', required=True, type=Path)
    ap.add_argument('sources', nargs='+', type=Path)
    a = ap.parse_args(argv)
    try:
        require(a.header.resolve() != a.manifest.resolve(), 'output paths must differ')
        require(not any(x.resolve() in {a.header.resolve(),a.manifest.resolve()} for x in [a.parser,*a.sources]), 'output overlaps input')
        rows, inputs = compile_files(load_parser(a.parser), a.sources)
        payload = dict(schema='on1x.ffi-descriptors/v1', inputs=inputs,
                       parser_sha256=sha(a.parser.read_bytes()), generator_sha256=sha(Path(__file__).read_bytes()),
                       bindings=rows, grants_safe_admission=False)
        digest=sha(canonical(payload).encode())
        payload['descriptor_sha256']=digest
        # Validate the entire package before replacing either output. Pair is not a filesystem transaction.
        write_atomic(a.header, emit_header(rows,digest))
        write_atomic(a.manifest, canonical(payload))
    except (Invalid, OSError, ValueError) as exc:
        print(f'FFIHandler: {exc}', file=sys.stderr)
        return 1
    return 0

if __name__=='__main__':
    raise SystemExit(main())
