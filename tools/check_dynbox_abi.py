#!/usr/bin/env python3
"""Compare dynbox calls/closures with compiler-generated native ABI signatures.

No libffi or Python packages required. Also builds/loads a hidden-visibility
shared library and passes a native closure across its ABI. All output is temporary.
"""
import argparse
import os
from pathlib import Path
import platform
import random
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
SCALARS = {
    'i8': ('int8_t', 'sint8'), 'u8': ('uint8_t', 'uint8'),
    'i16': ('int16_t', 'sint16'), 'u16': ('uint16_t', 'uint16'),
    'i32': ('int32_t', 'sint32'), 'u32': ('uint32_t', 'uint32'),
    'i64': ('int64_t', 'sint64'), 'u64': ('uint64_t', 'uint64'),
    'f': ('float', 'float'), 'd': ('double', 'double'),
    'ld': ('long double', 'longdouble'),
}
STRUCTS = {
    'dd': ['d', 'd'], 'di': ['d', 'i64'], 'id': ['i64', 'd'],
    'ff': ['f', 'f'], 'fff': ['f', 'f', 'f'], 'if': ['i32', 'f'],
    'fi': ['f', 'i32'], 'bytes': ['i8', 'u16', 'i8'],
    'nested': ['ff', 'di'], 'small_nested': ['ff', 'ff'],
    'ld_box': ['ld'], 'ld_nested': ['ld_box'],
    'ld_pair': ['i64', 'ld'], 'big': ['d', 'i64', 'd', 'f'],
}


def ctype(t):
    return SCALARS[t][0] if t in SCALARS else 'T_' + t


def descriptor(t):
    return '&mulib_dynbox_type_' + SCALARS[t][1] if t in SCALARS else '&type_' + t


def leaves(t, path=''):
    if t in SCALARS:
        return [(path, ctype(t))]
    return [(p, c) for i, field in enumerate(STRUCTS[t])
            for p, c in leaves(field, path + '.f' + str(i))]


def source():
    rng = random.Random(137501)
    all_types = list(SCALARS) + list(STRUCTS)
    text = ['#include "mulib-dynbox.h"', '#include "mulib-dynload.h"',
            '#include <assert.h>', '#include <stdio.h>', '#include <string.h>']
    setup = []
    for name, fields in STRUCTS.items():
        text += ['typedef struct {' + ''.join(ctype(t) + ' f' + str(i) + ';'
                                             for i, t in enumerate(fields)) + '} T_' + name + ';',
                 'static mulib_dynbox_type_t type_' + name + ';',
                 'static const mulib_dynbox_type_t *const fields_' + name + '[] = {' +
                 ','.join(descriptor(t) for t in fields) + '};']
        setup += [f'assert(!mulib_dynbox_struct_init(&type_{name},fields_{name},{len(fields)},NULL));',
                  f'assert(type_{name}.size == sizeof(T_{name}) && type_{name}.alignment == _Alignof(T_{name}));']
    checks = []
    count = len(all_types) * 4
    for case in range(count):
        ret = all_types[case % len(all_types)]
        args = [rng.choice(all_types) for _ in range(rng.randrange(19))]
        params = ','.join(ctype(t) + ' a' + str(i) for i, t in enumerate(args)) or 'void'
        native_args = ','.join('a' + str(i) for i in range(len(args)))
        text += [f'static {ctype(ret)} fn_{case}({params}) {{', 'long double total = 0;']
        for i, t in enumerate(args):
            for j, (path, _) in enumerate(leaves(t)):
                text.append(f'total += (long double)a{i}{path} * {1 + (i+j)%7};')
        text.append(ctype(ret) + ' result;')
        for j, (path, base) in enumerate(leaves(ret)):
            text.append(f'result{path} = ({base})((long long)total % 37 + {j+1});')
        text += ['return result; }',
                 f'static void cb_{case}(const mulib_dynbox_cif_t *c,void *r,void *const *a,void *u) {{',
                 '(void)c; (void)u; (void)a;',
                 f'*({ctype(ret)} *)r = fn_{case}(' + ','.join(f'*({ctype(t)} *)a[{i}]' for i, t in enumerate(args)) + '); }']
        checks += ['{', 'mulib_dynbox_cif_t *c = NULL;', 'mulib_dynbox_closure_t *closure = NULL;',
                   'mulib_dynbox_fn entry;', f'{ctype(ret)} got, expected;',
                   f'{ctype(ret)} (*closure_fn)(' + (','.join(ctype(t) for t in args) or 'void') + ');']
        for i, t in enumerate(args):
            checks.append(f'{ctype(t)} a{i} = {{0}};')
            for j, (path, base) in enumerate(leaves(t)):
                value = str(1+(i+j)%7) + ('.25' if base in ('float', 'double', 'long double') else '')
                checks.append(f'a{i}{path} = ({base}){value};')
        checks += ['const mulib_dynbox_type_t *types[] = {' + (','.join(descriptor(t) for t in args) or 'NULL') + '};',
                   'const void *arguments[] = {' + (','.join('&a'+str(i) for i in range(len(args))) or 'NULL') + '};',
                   f'assert(!mulib_dynbox_prepare(&c,{descriptor(ret)},{len(args)},types));',
                   f'expected = fn_{case}({native_args});',
                   f'assert(!mulib_dynbox_call(c,(mulib_dynbox_fn)fn_{case},&got,sizeof got,arguments));']
        for path, _ in leaves(ret):
            checks.append(f'assert(got{path} == expected{path});')
        checks += [f'assert(!mulib_dynbox_closure_create(&closure,c,cb_{case},NULL));',
                   'entry = mulib_dynbox_closure_function(closure); memcpy(&closure_fn,&entry,sizeof closure_fn);',
                   f'got = closure_fn({native_args});']
        for path, _ in leaves(ret):
            checks.append(f'assert(got{path} == expected{path});')
        checks += ['assert(!mulib_dynbox_closure_destroy(closure)); assert(!mulib_dynbox_destroy(c));', '}']
    text += ['static void plugin_callback(const mulib_dynbox_cif_t *c,void *r,void *const *a,void *u) {',
             '(void)c; *(int64_t *)r = *(int64_t *)a[0] + *(int64_t *)u; }',
             'int main(int argc,char **argv) {',
             'if (!mulib_dynbox_supported()) { puts("SKIP: native dynbox ABI unavailable"); return 0; }',
             'assert(argc == 2);'] + setup + checks
    text += ['''{
mulib_dynload_t library = MULIB_DYNLOAD_INIT;
mulib_dynbox_cif_t *c = NULL, *callback_cif = NULL;
mulib_dynbox_closure_t *closure = NULL;
mulib_dynbox_fn function = NULL, entry;
char error[256];
double real = 1.25, result;
int64_t integer = 7, extra = 3, callback_result;
const mulib_dynbox_type_t *types[] = {&mulib_dynbox_type_double,&mulib_dynbox_type_sint64};
const void *arguments[] = {&real,&integer};
assert(!mulib_dynload_open(&library,argv[1],error,sizeof error));
assert(!mulib_dynload_function(&library,"dynbox_plugin",&function,sizeof function,error,sizeof error));
assert(!mulib_dynbox_prepare(&c,&mulib_dynbox_type_double,2,types));
assert(!mulib_dynbox_call(c,function,&result,sizeof result,arguments) && result == 8.25);
assert(!mulib_dynbox_destroy(c)); c = NULL;
assert(!mulib_dynbox_prepare(&callback_cif,&mulib_dynbox_type_sint64,1,types+1));
assert(!mulib_dynbox_closure_create(&closure,callback_cif,plugin_callback,&extra));
entry = mulib_dynbox_closure_function(closure);
types[0] = &mulib_dynbox_type_pointer; arguments[0] = &entry;
assert(!mulib_dynbox_prepare(&c,&mulib_dynbox_type_sint64,2,types));
assert(!mulib_dynload_function(&library,"dynbox_callback",&function,sizeof function,error,sizeof error));
assert(!mulib_dynbox_call(c,function,&callback_result,sizeof callback_result,arguments) && callback_result == 10);
assert(!mulib_dynbox_destroy(c) && !mulib_dynbox_closure_destroy(closure) && !mulib_dynbox_destroy(callback_cif));
assert(!mulib_dynload_close(&library,error,sizeof error));
}''', f'puts("dynbox compiler ABI: {count} signatures, calls/closures and shared-library integration passed");',
             'return 0; }']
    return '\n'.join(text) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default=os.environ.get('CC', 'cc'))
    parser.add_argument('--sanitizers', action='store_true')
    args = parser.parse_args()
    if not sys.platform.startswith('linux') or platform.machine().lower() not in ('x86_64', 'amd64'):
        print('SKIP: dynbox compiler ABI requires Linux x86-64')
        return
    flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG',
             '-I' + str(ROOT / 'include')]
    if args.sanitizers:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    compiler = shlex.split(args.compiler)
    temp_parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-dynbox-abi-', dir=temp_parent) as tmp:
        tmp = Path(tmp)
        client, impl, plugin = tmp/'client.c', tmp/'implementation.c', tmp/'plugin.c'
        client.write_text(source())
        impl.write_text('#define MULIB_IMPL_DYNBOX\n#define MULIB_IMPL_DYNLOAD\n'
                        '#include "mulib-dynbox.h"\n#include "mulib-dynload.h"\n')
        plugin.write_text('''#define MULIB_DYNLOAD_EXPORT_SYMBOLS
#include "mulib-dynload.h"
#include <stdint.h>
MULIB_DYNLOAD_EXPORT double MULIB_DYNLOAD_CALL dynbox_plugin(double a,int64_t b) { return a+b; }
MULIB_DYNLOAD_EXPORT int64_t MULIB_DYNLOAD_CALL dynbox_callback(int64_t (*fn)(int64_t),int64_t value) { return fn(value); }
''')
        library = tmp/'libdynbox_probe.so'
        subprocess.run(compiler + flags + ['-fPIC', '-fvisibility=hidden', '-shared', str(plugin), '-o', str(library)], check=True, timeout=120)
        binary = tmp/'client'
        subprocess.run(compiler + flags + [str(impl), str(client), '-ldl', '-o', str(binary)], check=True, timeout=180)
        subprocess.run([str(binary), str(library)], check=True, timeout=60)


if __name__ == '__main__':
    main()
