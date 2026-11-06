#!/usr/bin/env python3
"""Generate compiler-typed MuLL bindings for every public MuLIB header.

Requires the Python clang bindings and libclang only when regenerating.
Normal builds compile the checked-in snapshot with the host C compiler.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'mull/stdlib/Mulib'


def quote(text):
    return json.dumps(text, ensure_ascii=True)


def module_name(header):
    return re.sub(r'^(include-)?mulib-', '', header.removesuffix('.h'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--include', type=Path, default=ROOT.parent / 'include')
    parser.add_argument('--clang', default='clang')
    parser.add_argument('--libclang', default=os.environ.get('LIBCLANG_LIBRARY_FILE'))
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    from clang import cindex as c
    if args.libclang:
        c.Config.set_library_file(args.libclang)
    elif Path('/usr/lib/libclang.so').exists():
        c.Config.set_library_file('/usr/lib/libclang.so')
    include = args.include.resolve()
    headers = sorted(include.glob('*.h'))
    resource = subprocess.check_output([args.clang, '-print-resource-dir'], text=True).strip()
    source = '\n'.join(f'#include "{h.name}"' for h in headers)
    tu = c.Index.create().parse('mull_mulib_inventory.c',
        args=['-x', 'c', '-std=gnu11', '-resource-dir=' + resource, '-I' + str(include)],
        unsaved_files=[('mull_mulib_inventory.c', source)],
        options=c.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
    errors = [str(d) for d in tu.diagnostics if d.severity >= c.Diagnostic.Error]
    if errors:
        raise RuntimeError('\n'.join(errors))
    nodes = [n for n in tu.cursor.get_children()
             if n.location.file and Path(str(n.location.file)).parent == include]
    modules = {module_name(h.name): {'header': h.name, 'symbols': []} for h in headers}
    kinds = c.TypeKind
    types, type_ids, preferred = [], {}, {}
    aliases, funcs, constants = [], [], []

    def key(t):
        canonical = t.get_canonical()
        if canonical.kind in (kinds.POINTER, kinds.FUNCTIONPROTO, kinds.FUNCTIONNOPROTO):
            return canonical.spelling
        return re.sub(r'\b(const|volatile|restrict)\b\s*', '', canonical.spelling).strip()

    # Anonymous records often have a public typedef; use it rather than the
    # source-location-dependent spelling libclang gives their canonical type.
    for n in nodes:
        if n.kind == c.CursorKind.TYPEDEF_DECL:
            preferred.setdefault(key(n.underlying_typedef_type), n.spelling)

    def add(t, expression=None, parameter=False):
        canon = t.get_canonical()
        if parameter and canon.kind in (kinds.CONSTANTARRAY, kinds.INCOMPLETEARRAY):
            element = add(canon.element_type)
            qualifier = 'const ' if canon.element_type.is_const_qualified() else ''
            k = qualifier + types[element]['key'] + ' *'
            if k not in type_ids:
                i = len(types); type_ids[k] = i
                types.append({'key': k, 'name': k, 'kind': 'POINTER', 'element': element,
                              'pointee_const': bool(qualifier),
                              'decl': f'typedef {qualifier}mb_t{element} *mb_t{i};', 'fields': [], 'count': 0})
            return type_ids[k]
        k = key(t)
        if k in type_ids:
            return type_ids[k]
        i = len(types); type_ids[k] = i
        entry = {'key': k, 'name': preferred.get(k, k), 'fields': [], 'count': 0,
                 'element': None, 'kind': 'OPAQUE', 'complete': canon.get_size() >= 0}
        if expression and ('unnamed' in entry['name'] or 'anonymous' in entry['name']):
            entry['name'] = f'__typeof__({expression})'
        types.append(entry)
        kind = canon.kind
        if kind == kinds.POINTER:
            element = add(canon.get_pointee(), '*(' + expression + ')' if expression else None); entry.update(kind='POINTER', element=element)
            entry['pointee_const'] = canon.get_pointee().is_const_qualified()
            qualifier = 'const ' if canon.get_pointee().is_const_qualified() else ''
            entry['decl'] = f'typedef {qualifier}mb_t{element} *mb_t{i};'
        elif kind in (kinds.CONSTANTARRAY, kinds.INCOMPLETEARRAY):
            element = add(canon.element_type, expression + '[0]' if expression else None); count = max(0, canon.element_count)
            entry.update(kind='ARRAY', element=element, count=count)
            entry['decl'] = f'typedef mb_t{element} mb_t{i}[{count}];'
        elif kind in (kinds.FUNCTIONPROTO, kinds.FUNCTIONNOPROTO):
            if kind == kinds.FUNCTIONPROTO and canon.is_function_variadic():
                raise RuntimeError('Variadic public signature requires an explicit adapter: ' + k)
            result = add(canon.get_result())
            arguments = [add(a, parameter=True) for a in canon.argument_types()] if kind == kinds.FUNCTIONPROTO else []
            entry.update(kind='FUNCTION', result=result, args=arguments)
            params = ', '.join(f'mb_t{a}' for a in arguments) or 'void'
            entry['decl'] = f'typedef mb_t{result} mb_t{i}({params});'
        else:
            spelling = preferred.get(k, k)
            if expression and ('unnamed' in spelling or 'anonymous' in spelling):
                spelling = f'__typeof__({expression})'
                entry['name'] = spelling
            if 'unnamed' in spelling or 'anonymous' in spelling:
                raise RuntimeError('Cannot name C type: ' + k)
            entry['decl'] = f'typedef {spelling} mb_t{i};'
            if kind == kinds.VOID:
                entry['kind'] = 'VOID'
            elif kind in (kinds.FLOAT, kinds.DOUBLE, kinds.LONGDOUBLE):
                entry['kind'] = 'FLOAT'
            elif kind == kinds.RECORD:
                entry['kind'] = 'RECORD' if entry['complete'] else 'OPAQUE'
                declaration = canon.get_declaration()
                public = declaration.location.file and Path(str(declaration.location.file)).parent == include
                for field in (canon.get_fields() if public else []):
                    if not field.spelling:
                        continue
                    ft = add(field.type, f'((mb_t{i}*)0)->{field.spelling}')
                    entry['fields'].append({'name': field.spelling, 'type': ft,
                        'bitfield': field.is_bitfield(), 'readonly': field.type.is_const_qualified()})
            else:
                numeric_kind = canon.get_declaration().enum_type.kind if kind == kinds.ENUM else kind
                if numeric_kind in (kinds.BOOL, kinds.CHAR_U, kinds.UCHAR, kinds.USHORT, kinds.UINT, kinds.ULONG, kinds.ULONGLONG):
                    entry['kind'] = 'UNSIGNED'
                elif numeric_kind in (kinds.CHAR_S, kinds.SCHAR, kinds.SHORT, kinds.INT, kinds.LONG, kinds.LONGLONG):
                    entry['kind'] = 'SIGNED'
        return i

    def symbol(header, name, kind, index):
        modules[module_name(header)]['symbols'].append((name, kind, index))

    # Primitive aliases make native scalar/output storage usable from scripts.
    primitive_source = '\n'.join(f'typedef {t} mull_primitive_{i};' for i, t in enumerate(
        ['void', 'char', 'unsigned char', 'short', 'unsigned short', 'int', 'unsigned int',
         'long', 'unsigned long', 'long long', 'unsigned long long', 'float', 'double', 'long double',
         'size_t', 'int8_t', 'uint8_t', 'int16_t', 'uint16_t', 'int32_t', 'uint32_t', 'int64_t', 'uint64_t']))
    pt = c.Index.create().parse('primitives.c', args=['-x','c','-std=gnu11','-resource-dir='+resource],
        unsaved_files=[('primitives.c', '#include <stddef.h>\n#include <stdint.h>\n'+primitive_source)])
    for n in pt.cursor.get_children():
        if n.spelling.startswith('mull_primitive_'):
            aliases.append((n.underlying_typedef_type.spelling, add(n.underlying_typedef_type)))
    seen_functions = set()
    for n in nodes:
        header = Path(str(n.location.file)).name
        if n.kind == c.CursorKind.TYPEDEF_DECL:
            tid = add(n.underlying_typedef_type)
            aliases.append((n.spelling, tid)); symbol(header, n.spelling, 1, tid)
        elif n.kind == c.CursorKind.FUNCTION_DECL and n.spelling.startswith('mulib_') and not n.spelling.endswith('_'):
            if n.spelling in seen_functions:
                continue
            seen_functions.add(n.spelling)
            signature = add(n.type)
            symbol(header, n.spelling, 0, len(funcs)); funcs.append((n.spelling, signature))
        elif n.kind == c.CursorKind.ENUM_DECL:
            for value in n.get_children():
                if value.kind == c.CursorKind.ENUM_CONSTANT_DECL:
                    tid = add(value.type)
                    symbol(header, value.spelling, 2, len(constants))
                    constants.append((value.spelling, tid, False))
        elif n.kind == c.CursorKind.VAR_DECL and n.spelling.startswith('mulib_'):
            tid = add(n.type); symbol(header, n.spelling, 2, len(constants))
            constants.append((n.spelling, tid, True))

    # Object-like value macros are runtime constants. Type aliases, declaration
    # generators and function-like preprocessor macros have no callable C ABI.
    candidates, preprocessor_only = [], []
    for n in nodes:
        if n.kind != c.CursorKind.MACRO_DEFINITION or not n.spelling.startswith('MULIB_'):
            continue
        tokens = list(n.get_tokens())
        if len(tokens) < 2 or n.spelling.startswith('MULIB_IMPL_') or n.spelling.endswith(('_H', '_H_', '_IMPLEMENTED')):
            continue
        if tokens[1].spelling == '(' and tokens[1].extent.start.offset == tokens[0].extent.end.offset:
            preprocessor_only.append(n.spelling)
        else:
            candidates.append((n.spelling, Path(str(n.location.file)).name))
    probe = source + '\n' + '\n'.join(f'__auto_type mb_probe_{i} = ({name});' for i,(name,_) in enumerate(candidates))
    macro_tu = c.Index.create().parse('macros.c',
        args=['-x','c','-std=gnu11','-ferror-limit=0','-resource-dir='+resource,'-I'+str(include)],
        unsaved_files=[('macros.c', probe)])
    bad_lines = {d.location.line for d in macro_tu.diagnostics if d.severity >= c.Diagnostic.Error}
    bound_macros = set()
    for n in macro_tu.cursor.get_children():
        if not n.spelling.startswith('mb_probe_') or n.location.line in bad_lines:
            continue
        name, header = candidates[int(n.spelling.removeprefix('mb_probe_'))]
        if n.type.get_canonical().kind not in (kinds.BOOL,kinds.CHAR_S,kinds.CHAR_U,kinds.SCHAR,kinds.UCHAR,
                kinds.SHORT,kinds.USHORT,kinds.INT,kinds.UINT,kinds.LONG,kinds.ULONG,kinds.LONGLONG,kinds.ULONGLONG,
                kinds.FLOAT,kinds.DOUBLE,kinds.LONGDOUBLE,kinds.POINTER,kinds.ENUM):
            preprocessor_only.append(name)
            continue
        tid = add(n.type)
        symbol(header,name,2,len(constants)); constants.append((name,tid,False))
        bound_macros.add(name)
    preprocessor_only.extend(name for name,_ in candidates if name not in bound_macros)

    out = ['/* Generated by scripts/generate_mulib.py. Do not edit. */', source]
    emitted = set()
    def emit_type(i):
        if i in emitted:
            return
        entry = types[i]
        # Named records may point back to themselves. Their typedef does not
        # require field aliases, so emit it before following field types.
        if entry['kind'] not in ('POINTER', 'ARRAY', 'FUNCTION'):
            out.append(entry['decl']); emitted.add(i)
        if entry['element'] is not None:
            emit_type(entry['element'])
        if entry['kind'] == 'FUNCTION':
            emit_type(entry['result'])
            for arg in entry['args']: emit_type(arg)
        if i not in emitted:
            out.append(entry['decl']); emitted.add(i)
        for field in entry['fields']: emit_type(field['type'])
    for i in range(len(types)): emit_type(i)

    for i, entry in enumerate(types):
        for j, field in enumerate(entry['fields']):
            ft = field['type']; access = f'((mb_t{i}*)object)->{field["name"]}'
            address = 'NULL' if field['bitfield'] else f'(void*)&{access}'
            out.append(f'static void *mb_addr_{i}_{j}(void *object) {{ return {address}; }}')
            getter = f'*(mb_t{ft}*)out = {access};' if field['bitfield'] else f'memcpy(out, &{access}, sizeof(mb_t{ft}));'
            out.append(f'static void mb_get_{i}_{j}(const void *object, void *out) {{ {getter} }}')
            if not field['readonly']:
                setter = f'{access} = *(const mb_t{ft}*)value;' if field['bitfield'] else f'memcpy(&{access}, value, sizeof(mb_t{ft}));'
                out.append(f'static void mb_set_{i}_{j}(void *object, const void *value) {{ {setter} }}')
        if entry['fields']:
            out.append(f'static const mb_field mb_fields_{i}[] = {{')
            for j, f in enumerate(entry['fields']):
                setter = 'NULL' if f['readonly'] else f'mb_set_{i}_{j}'
                out.append(f'{{{quote(f["name"])},{f["type"]},mb_addr_{i}_{j},mb_get_{i}_{j},{setter}}},')
            out.append('};')
        if entry['kind'] == 'FUNCTION':
            arguments = entry['args']; result = entry['result']; returns = types[result]['kind'] != 'VOID'
            actuals = ','.join(f'*(mb_t{arg}*)args[{j}]' for j, arg in enumerate(arguments))
            prefix = f'*(mb_t{result}*)out = ' if returns else '(void)out; '
            out.append(f'static void mb_indirect_{i}(const void *pointer, void *out, void **args) {{ mb_t{i} *fn; memcpy(&fn,pointer,sizeof(fn)); {prefix}fn({actuals}); }}')
            out.append(f'static mb_callback_slot mb_slots_{i}[MULL_MULIB_CALLBACK_SLOTS];')
            params = ','.join(f'mb_t{a} a{j}' for j, a in enumerate(arguments)) or 'void'
            ptrs = ','.join(f'&a{j}' for j in range(len(arguments))) or 'NULL'
            for slot in range(8):
                body = f'mb_t{result} result = {{0}};' if returns else ''
                body += f'void *args[] = {{{ptrs}}}; mb_dispatch(&mb_slots_{i}[{slot}],{i},' + ('&result' if returns else 'NULL') + ',args);'
                if returns: body += 'return result;'
                out.append(f'static mb_t{result} mb_cb_{i}_{slot}({params}) {{ {body} }}')
            out.append(f'static void *mb_create_{i}(mull_mb_callback_fn fn, void *user, void *out) {{')
            out.append(f'mb_t{i} *functions[] = {{' + ','.join(f'mb_cb_{i}_{slot}' for slot in range(8)) + '};')
            out.append(f'int slot = mb_reserve(mb_slots_{i},fn,user); if(slot<0) return NULL; memcpy(out,&functions[slot],sizeof(functions[slot])); return &mb_slots_{i}[slot]; }}')
            out.append(f'static const size_t mb_args_{i}[] = {{' + (','.join(map(str, arguments)) or '0') + '};')

    out.append('static const mb_type mb_types[] = {')
    for i, t in enumerate(types):
        complete = t.get('complete', True) and t['kind'] not in ('VOID', 'FUNCTION')
        size = f'sizeof(mb_t{i})' if complete else '0'
        align = f'_Alignof(mb_t{i})' if complete else '1'
        fields = f'mb_fields_{i}' if t['fields'] else 'NULL'
        signature = t['kind'] == 'FUNCTION'
        out.append('{' + ','.join([quote(t['name']), 'MULL_MB_'+t['kind'], size, align,
            str(t['element']) if t['element'] is not None else 'MULL_MB_NONE', str(t['count']), fields, str(len(t['fields'])),
            str(t['result']) if signature else 'MULL_MB_NONE', f'mb_args_{i}' if signature else 'NULL',
            str(len(t['args'])) if signature else '0', f'mb_indirect_{i}' if signature else 'NULL',
            f'mb_create_{i}' if signature else 'NULL']) + '},')
    out.append('};')
    out.append('static int mb_pointee_const(size_t type) { switch(type) {')
    out.extend(f'case {i}: return 1;' for i,t in enumerate(types) if t.get('pointee_const'))
    out.append('default: return 0; }}')
    for i, (name, sig) in enumerate(funcs):
        t = types[sig]; result = t['result']; returns = types[result]['kind'] != 'VOID'
        prefix = f'*(mb_t{result}*)out = ' if returns else '(void)out; '
        actuals = ','.join(f'*(mb_t{a}*)args[{j}]' for j, a in enumerate(t['args']))
        out.append(f'static void mb_fn_{i}(void *out, void **args) {{ {prefix}{name}({actuals}); }}')
    out.append('static const mb_function mb_functions[] = {')
    out.extend(f'{{{quote(name)},{sig},mb_fn_{i}}},' for i,(name,sig) in enumerate(funcs)); out.append('};')
    for i, (name, tid, variable) in enumerate(constants):
        if not variable: out.append(f'static const mb_t{tid} mb_const_{i} = {name};')
    out.append('static const mb_constant mb_constants[] = {')
    out.extend(f'{{{tid},&{name if variable else "mb_const_"+str(i)}}},' for i,(name,tid,variable) in enumerate(constants)); out.append('};')
    for i,(name, module) in enumerate(sorted(modules.items())):
        out.append(f'static const mb_symbol mb_symbols_{i}[] = {{')
        out.extend(f'{{{quote(n)},{k},{v}}},' for n,k,v in module['symbols'])
        out.append('{NULL,0,0}};')
    out.append('static const mb_library mb_libraries[] = {')
    out.extend(f'{{{quote(name)},{quote(m["header"])},mb_symbols_{i},{len(m["symbols"])}}},' for i,(name,m) in enumerate(sorted(modules.items()))); out.append('};')
    out.append('static const mb_alias mb_aliases[] = {')
    out.extend(f'{{{quote(name)},{i}}},' for name,i in sorted(set(aliases))); out.append('};')
    # Numeric conversion is compiler-typed too: no guessed enum sizes or long ABI.
    for operation in ('read', 'write', 'integer_read', 'integer_write'):
        signatures = {'read': 'const void *value, double *out', 'write': 'void *value, double number',
                      'integer_read': 'const void *value, char *out, size_t capacity',
                      'integer_write': 'void *value, const char *text'}
        out.append(f'static int mb_numeric_{operation}(size_t type, {signatures[operation]}) {{ switch(type) {{')
        for i,t in enumerate(types):
            kind = t['kind']
            if kind not in ('SIGNED','UNSIGNED','FLOAT'): continue
            out.append(f'case {i}: {{')
            if operation == 'read':
                out.append(f'long double n = *(const mb_t{i}*)value;')
                if kind != 'FLOAT': out.append('if(n < -9007199254740991.0L || n > 9007199254740991.0L) return 0;')
                out.append('*out=(double)n; return isfinite(*out);')
            elif operation == 'write':
                out.append('if(!isfinite(number)) return 0;')
                if kind in ('SIGNED','UNSIGNED'):
                    out.append(f'long double limit=ldexpl(1.0L,sizeof(mb_t{i})*CHAR_BIT-{1 if kind=="SIGNED" else 0});')
                    out.append('if(trunc(number)!=number || fabs(number)>9007199254740991.0) return 0;')
                    out.append(f'if((long double)number < {"-limit" if kind=="SIGNED" else "0"} || (long double)number >= limit) return 0;')
                out.append(f'mb_t{i} n=(mb_t{i})number;')
                if kind == 'FLOAT': out.append('if(!isfinite(n)) return 0;')
                out.append(f'*(mb_t{i}*)value=n; return 1;')
            elif operation == 'integer_read':
                if kind == 'FLOAT': out.append('return 0;')
                else:
                    fmt,cast = ('%lld','long long') if kind=='SIGNED' else ('%llu','unsigned long long')
                    out.append(f'int n=snprintf(out,capacity,"{fmt}",({cast})*(const mb_t{i}*)value); return n>=0 && (size_t)n<capacity;')
            elif operation == 'integer_write':
                if kind == 'FLOAT': out.append('return 0;')
                else:
                    unsigned = kind == 'UNSIGNED'
                    out.append('char *end; errno=0; if(!text || !*text || isspace((unsigned char)*text)) return 0;')
                    if unsigned: out.append('if(*text==\'-\') return 0;')
                    out.append(f'{"unsigned long long" if unsigned else "long long"} n={"strtoull" if unsigned else "strtoll"}(text,&end,10); if(errno || *end) return 0;')
                    # Round trip catches narrower integer/enum types.
                    out.append(f'mb_t{i} converted=(mb_t{i})n; if(({"unsigned long long" if unsigned else "long long"})converted!=n) return 0; *(mb_t{i}*)value=converted; return 1;')
            out.append('}')
        out.append('default: return 0; }}')

    macros = sorted(set(re.findall(r'(?:defined\s*\(?\s*|#\s*ifdef\s+)(MULIB_IMPL_\w+)', '\n'.join(h.read_text() for h in headers))))
    implementation = '/* Generated implementation switches for every public header. */\n' + '\n'.join('#define '+m for m in macros) + '\n' + source + '\n'
    inventory = {'headers': [h.name for h in headers], 'libraries': len(modules), 'functions': len(funcs),
                 'types': len(types), 'constants': len(constants),
                 'preprocessor_only': sorted(set(preprocessor_only)),
                 'fingerprint': hashlib.sha256(''.join(h.read_text() for h in headers).encode()).hexdigest(),
                 'modules': {name: {'header': m['header'], 'functions': [n for n,k,_ in m['symbols'] if k==0]} for name,m in sorted(modules.items())}}
    outputs = {'Generated.inc': '\n'.join(out)+'\n', 'Implementation.c': implementation,
               'Inventory.json': json.dumps(inventory, indent=2)+'\n'}
    for filename, content in outputs.items():
        path = DEST / filename
        if args.check:
            if not path.exists() or path.read_text() != content: raise RuntimeError(f'{filename} is stale; regenerate bindings')
        elif not path.exists() or path.read_text() != content:
            path.write_text(content)
    print(f'{len(headers)} headers, {len(modules)} libraries, {len(funcs)} functions, {len(types)} native types, {len(constants)} constants')


if __name__ == '__main__':
    main()
