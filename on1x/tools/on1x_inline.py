"""Raw C11 forwarding wrappers from in-process ExoLib CDecl evidence.

This is not On1x call lowering or an ExoML semantic contract. No review or
import path accepts these artifacts.
"""

import re


IDENT = r'[A-Za-z_][A-Za-z_0-9]*'
SELECTION = re.compile(r'([a-z][a-z0-9-]*):(mulib_[a-z0-9_]+)')
TARGET = re.compile(r'x86_64(?:-[a-z0-9_]+)?-linux-gnu')
PREFIX = 'on1x_inline_v1_'
INTEGERS = {
    '_Bool': False, 'signed char': True, 'unsigned char': False,
    'short': True, 'unsigned short': False, 'int': True, 'unsigned int': False,
    'long': True, 'unsigned long': False, 'long long': True, 'unsigned long long': False,
}


def selections(requests):
    if not 1 <= len(requests) <= 32:
        raise ValueError('wrap-inline requires 1..32 explicit MODULE:SYMBOL selections')
    matches = [SELECTION.fullmatch(s) if len(s) <= 256 else None for s in requests]
    if not all(matches):
        raise ValueError('invalid wrap-inline selection (expected MODULE:mulib_SYMBOL)')
    pairs = sorted(m.groups() for m in matches)
    if len({s for _, s in pairs}) != len(pairs):
        raise ValueError('duplicate wrap-inline symbol selection')
    return pairs


class Surface:
    """Project a bounded subset of CDecl v2. Never use free-form C tokens."""

    def __init__(self, graph):
        self.graph = graph
        self.types = {t['id']: t for t in graph['types']}
        if (not TARGET.fullmatch(graph['target']['triple']) or graph['target']['pointer_bits'] != 64 or
                not re.fullmatch('[0-9a-f]{64}', graph['fingerprint']) or len(self.types) != len(graph['types'])):
            raise ValueError('unsupported CDecl target or type graph')
        self.checks = set()

    @staticmethod
    def plain(t, allow_const=False):
        if (t['volatile'] or t['restrict'] or (t['const'] and not allow_const) or
                t['attributes'] or t['unsupported_reason']):
            raise ValueError('unsupported qualifiers, attributes or type: '+t['spelling'])

    def layout(self, t, spelling):
        size, align = t['size'], t['alignment']
        if (type(size) is not int or not 1 <= size <= 128 or
                type(align) is not int or align not in (1, 2, 4, 8, 16) or size % align):
            raise ValueError('unsupported CDecl layout: '+spelling)
        self.checks.add(f'_Static_assert(sizeof({spelling}) == {size} && _Alignof({spelling}) == {align},'
                        f' "inline ABI layout: {spelling}");')

    def type_pair(self, t, allow_const=False):
        c = self.types[t['canonical_id']]
        self.plain(t, allow_const)
        self.plain(c, allow_const)
        if t['const'] != c['const'] or (t['size'], t['alignment']) != (c['size'], c['alignment']):
            raise ValueError('inconsistent CDecl canonical type: '+t['spelling'])
        return c

    def value(self, type_id, *, allow_const=False, allow_record=True, allow_void=False):
        t = self.types[type_id]
        c = self.type_pair(t, allow_const)
        canonical, spelling = c['spelling'], t['spelling']
        prefix = 'const ' if c['const'] else ''
        if not canonical.startswith(prefix):
            raise ValueError('inconsistent CDecl type qualification')
        base = canonical[len(prefix):]
        if spelling != canonical and not (t['kind'] == 'typedef' and spelling.startswith(prefix) and
                                          re.fullmatch(IDENT, spelling[len(prefix):])):
            raise ValueError('unsupported CDecl type spelling: '+spelling)
        if c['kind'] == 'void' and canonical == spelling == 'void' and allow_void:
            return spelling, canonical
        if c['kind'] == 'integer' and base in INTEGERS and c['signed'] is INTEGERS[base]:
            pass
        elif c['kind'] == 'floating' and base in ('float', 'double'):
            pass
        elif (allow_record and c['kind'] == 'record' and not c['union'] and
              re.fullmatch('struct '+IDENT, base) and 1 <= len(c['fields']) <= 16):
            self.record(c, base)
        else:
            raise ValueError('unsupported CDecl value type: '+spelling)
        self.layout(c, canonical)
        return spelling, canonical

    def record(self, record, name):
        # Flat, padding-free records only. This avoids admitting an ABI change
        # that adds another field inside unchecked padding without changing
        # size, alignment, or the offsets of fields we know about.
        offset = 0
        seen = set()
        for field in record['fields']:
            member = field['name']
            if (not re.fullmatch(IDENT, member) or member in seen or
                    field['bit_width'] is not None or field['offset_bits'] != offset * 8):
                raise ValueError('unsupported CDecl record member layout: '+name)
            seen.add(member)
            _, field_type = self.value(field['type'], allow_record=False)
            size = self.types[field['type']]['size']
            self.checks.add(f'_Static_assert(offsetof({name}, {member}) == {offset},'
                            f' "inline ABI offset: {name}.{member}");')
            self.checks.add(f'_Static_assert(_Generic(&(({name} *)0)->{member}, {field_type} *: 1,'
                            f' default: 0), "inline ABI member type: {name}.{member}");')
            offset += size
        if offset != record['size']:
            raise ValueError('unsupported CDecl record padding: '+name)

    def parameter(self, type_id):
        t = self.types[type_id]
        if t['kind'] != 'pointer':
            return self.value(type_id)
        c = self.type_pair(t)
        if c['kind'] != 'pointer':
            raise ValueError('inconsistent CDecl pointer kind')
        spelling, canonical = self.value(t['element'], allow_const=True)
        if self.types[t['element']]['canonical_id'] != c['element'] or c['spelling'] != canonical+' *':
            raise ValueError('inconsistent CDecl pointer element')
        self.layout(c, c['spelling'])
        return spelling+' *', canonical+' *'

    def wrapper(self, symbol):
        candidates = [d for d in self.graph['declarations'] if d['name'] == symbol]
        if len(candidates) != 1:
            raise ValueError('expected one canonical CDecl definition')
        d = candidates[0]
        if (d['kind'] != 'function' or d['definition'] is not True or d['inline'] is not True or
                d['linkage'] != '2' or d['source']['file'] != self.graph['header'] or d['unsupported_reason']):
            raise ValueError('expected a canonical static-inline definition')
        t = self.types[d['type']]
        canonical = self.type_pair(t)
        for fn in (t, canonical):
            if (fn['kind'] != 'function' or fn['prototype'] is not True or fn['variadic'] or
                    fn['calling_convention'] != 1 or len(fn['arguments']) > 12):
                raise ValueError('unsupported CDecl prototype or calling convention')
        if (t['arguments'] != [p['type'] for p in d['parameters']] or
                [self.types[k]['canonical_id'] for k in t['arguments']] != canonical['arguments'] or
                self.types[t['result']]['canonical_id'] != canonical['result']):
            raise ValueError('inconsistent CDecl prototype type IDs')
        ret, raw_ret = self.value(t['result'], allow_void=True)
        args = [self.parameter(p) for p in t['arguments']]
        named = ', '.join(f'{name} a{i}' for i, (name, _) in enumerate(args)) or 'void'
        raw_args = ', '.join(c for _, c in args) or 'void'
        export = PREFIX+symbol
        signature = f'{ret} {export}({named})'
        for name in (symbol, export):
            self.checks.add(f'_Static_assert(_Generic(&{name}, {raw_ret} (*)({raw_args}): 1,'
                            f' default: 0), "inline ABI prototype: {name}");')
        forwarded = ', '.join(f'a{i}' for i in range(len(args)))
        statement = ('return ' if ret != 'void' else '')+f'{symbol}({forwarded});'
        return {
            'c_name': symbol, 'wrapper': export, 'declaration_id': d['id'], 'c_type_id': d['type'],
            'declaration': signature+';', 'definition': f'{signature} {{ {statement} }}',
            'link_ref': f'{raw_ret} (*on1x_inline_link_ref_{symbol})({raw_args}) = &{export};',
        }


def render(evidence, pairs):
    surfaces = {name: Surface(e['metadata']['cdecl']) for name, e in evidence.items()}
    exports = []
    for name, symbol in pairs:
        try:
            exports.append(dict(module=name, **surfaces[name].wrapper(symbol)))
        except ValueError as error:
            raise ValueError(f'wrap-inline {name}:{symbol}: {error}') from error
    includes = [f'#include <mulib-{name}.h>' for name in surfaces]
    checks = sorted(set().union(*(s.checks for s in surfaces.values())))
    header = '\n'.join([
        '/* Raw C11 declarations; underlying C preconditions still apply. */',
        '#ifndef ON1X_RAW_INLINE_V1_H', '#define ON1X_RAW_INLINE_V1_H',
        '#if !defined(__linux__) || !defined(__x86_64__) || !defined(__ELF__)',
        '#error "unsupported inline ABI target"', '#endif',
        '#include <limits.h>', '#include <stddef.h>', *includes,
        '_Static_assert(CHAR_BIT == 8 && sizeof(void *) == 8, "inline ABI data model");',
        *(e['declaration'] for e in exports), *checks, '#endif', '',
    ])
    definitions = '\n'.join(['#include "on1x-inline.h"', *(e['definition'] for e in exports), ''])
    # External data initializers preserve the symbol references at -O2 while
    # allowing a link check with no synthetic calls or guessed preconditions.
    link_source = '\n'.join(['#include "on1x-inline.h"', *(e['link_ref'] for e in exports),
                             'int main(void) { return 0; }', ''])
    return header, definitions, link_source, [
        {k: v for k, v in e.items() if k not in ('declaration', 'definition', 'link_ref')} for e in exports
    ]
