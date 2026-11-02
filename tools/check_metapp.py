#!/usr/bin/env python3
"""Compare MetaPP with a native C/C++ preprocessor and link a real ELF plugin."""
import argparse
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
PUNCT = sorted(['%:%:', '>>=', '<<=', '...', '->*', '<=>', '##', '->', '++', '--',
                '<<', '>>', '<=', '>=', '==', '!=', '&&', '||', '*=', '/=', '%=',
                '+=', '-=', '&=', '^=', '|=', '::', '.*', '<:', ':>', '<%', '%>', '%:'], key=len, reverse=True)
TOKEN_BASE = (
    r'(?:u8|[uUL])?R"(?P<delim>[^ ()\\\t\r\n]{0,16})\(.*?\)(?P=delim)"\w*'
    r'|(?:u8|[uUL])?(?:"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')\w*'
    r'|(?:\d|\.\d)(?:(?<=[eEpP])[+-]|[\w.\'])*'
    r'|[^\W\d]\w*|')
TOKEN = re.compile(TOKEN_BASE + '|'.join(re.escape(p) for p in PUNCT) + r'|[^\s]', re.S)
TOKEN_CPP = re.compile(TOKEN_BASE + r'<(?=::(?![:>]))|' + '|'.join(re.escape(p) for p in PUNCT) + r'|[^\s]', re.S)


def tokens(text, cpp=False):
    canonical = {'%:': '#', '%:%:': '##', '<:': '[', ':>': ']', '<%': '{', '%>': '}'}
    return [canonical.get(m.group(), m.group()) for m in (TOKEN_CPP if cpp else TOKEN).finditer(text)]


CASES = [
    '#define A 3\n#define B(x) x+x\nB(A)\n',
    '#define A B\n#define B A\nA B A\n',
    '#define f(x) x\n#define g f\ng(g)(2) f(f(1))\n',
    '#define F(x) x\n#define CALL F\nCALL\n(4)\n',
    '#define f(a) f(x*(a))\n#define x 2\n#define g f\n#define z z[0]\n'
    '#define h g(~\n#define m(a) a(w)\n#define w 0,1\n#define t(a) a\n'
    'f(y+1) + f(f(z)) % t(t(g)(0)+t)(1);\ng(x+(3,4)-w) | h 5) & m(f)^m(m);\n',
    '#define S(x) #x\nS(a   + /*comment*/ b) S("quoted\\\\value") S()\n',
    '#define CAT(a,b) a##b\n#define X 5\nCAT(X,1) CAT(,X) CAT(X,) CAT(,)\n',
    '#define A(x) x\nA(a ## b)\n',
    '#define hash_hash # ## #\n#define mkstr(a) # a\n#define in_between(a) mkstr(a)\n'
    '#define join(c,d) in_between(c hash_hash d)\njoin(x,y)\n',
    '#define S(x) #x\n#define BAD(a,b) a+b\nS(BAD(1))\n',
    '#define V(a,...) a __VA_ARGS__\nV(1,+2,+3) V(4,)\n',
    '#define N\n#define V(...) f(0 __VA_OPT__(,) __VA_ARGS__)\nV() V(1,2) V(N)\n',
    '#define V(...) a ## __VA_OPT__(b)\nV() V(x)\n',
    '#define X a\\\nb\nX\n',
    '#define X a/*\ncomment*/b\nX\n',
    '#line 50 "renamed.c"\n__LINE__ __FILE__\n',
    '#if 0\n#if 1/0\nwrong\n#endif\n#elif 1\nright\n#elif 1/0\nwrong\n#endif\n',
    '#if defined(UNDEFINED) || defined UNDEFINED\nwrong\n#else\nright\n#endif\n',
    '#if (1 ? -1 : 1U)>0\nright\n#else\nwrong\n#endif\n',
    '#if (1 || 1/0) && (1 ? 1 : 1/0)\nright\n#endif\n',
    '#if -1 < 1U\nwrong\n#else\nright\n#endif\n',
    '#if 0xffffffffffffffffULL == -1 && (-8>>2)==-2\nright\n#endif\n',
    '#define HEADER "config.h"\n#include HEADER\n#include "./config.h"\nVALUE\n',
    '#if __has_include("config.h") && !__has_include("absent.h")\nright\n#endif\n',
    '#define P(x) _Pragma(#x)\nP(pack(push,1))\nint x;\nP(pack(pop))\n',
    '#define S(x) #x\nS(<: %: %:%:)\n',
    '#define S(x) #x\n#define I(x) S(x)\n#define E\nI(a E+b)\n',
    '#define config missing\n#if __has_include("config.h")\nyes\n#endif\n',
    '#line 4 "a\\\\b\\\"c"\n__LINE__ __FILE__\n',
    '#define I(x) x x\nI(_Pragma(L"pack(push,1)"))\n',
]
CPP_CASES = [
    'R"tag(a\\\nb/*c*/"d)tag" u8"string"_suffix L\'x\'\n',
    '#if true and not false and ((7 bitand 3)==3) and (2 not_eq 3)\nright\n#endif\n',
    '#define V(...) __VA_OPT__(1 +) 2\nV() V(3)\n',
    '#define V(...) a ## __VA_OPT__(b)\nV() V(x)\n',
    '#define V(...) __VA_OPT__(a) ## b\nV() V(x)\n',
    '#define X 1\n#define F(a,...) #__VA_OPT__(a + __VA_ARGS__)\nF(X,x) F(X)\n',
    '#define X 1\n#define F(a,...) #__VA_OPT__(a ## a)\nF(X,x) F(X)\n',
    '#define S(x) #x\nS(R"(hello\nworld)")\n',
    'X<::A>\n',
]


def run(command, **kwargs):
    return subprocess.run(command, check=True, capture_output=True, text=True, timeout=60, **kwargs)


def reject_objects(preprocessor, obj, work):
    """Corrupt structural records, never execute modified plugin code."""
    data = obj.read_bytes()
    shoff = struct.unpack_from('<Q', data, 40)[0]
    stride, count = struct.unpack_from('<HH', data, 58)
    sections = [struct.unpack_from('<IIQQQQIIQQ', data, shoff+i*stride) for i in range(count)]
    text = next(i for i, s in enumerate(sections) if s[2] & 4 and s[5])
    rela = next(s for s in sections if s[1] == 4 and s[7] == text and s[5] >= 48)
    symtab = next(s for s in sections if s[1] == 2)
    symbols = [struct.unpack_from('<IBBHQQ', data, symtab[4]+i*symtab[9]) for i in range(symtab[5]//symtab[9])]
    strings = sections[symtab[6]][4]
    entry = next(s for s in symbols if data[strings+s[0]:].split(b'\0', 1)[0] == b'mulib_metapp_plugin_v1')
    descriptor = sections[entry[3]][4]+entry[4]
    changes = [
        (16, '<H', 3), (18, '<H', 3),
        (shoff+text*stride+8, '<Q', 7),
        (shoff+text*stride+4, '<I', 14),
        (rela[4], '<Q', (1 << 64)-1),
        (rela[4]+8, '<Q', (1 << 32) | 42),
        (rela[4]+8, '<Q', (0xffffffff << 32) | 2),
        (descriptor, '<I', 999),
        (strings+entry[0], '<B', ord('X')),
    ]
    corruptions = [data[:63], data[:-64]]
    for offset, fmt, value in changes:
        changed = bytearray(data); struct.pack_into(fmt, changed, offset, value)
        corruptions.append(changed)
    changed = bytearray(data)
    changed[rela[4]+24:rela[4]+48] = data[rela[4]:rela[4]+24]
    corruptions.append(changed)
    for i, content in enumerate(corruptions):
        path = work / f'invalid-{i}.o'; path.write_bytes(content)
        run([preprocessor, '--reject', str(path)])
    return len(corruptions)


def dependency_graphs(preprocessor, work):
    project = work / 'project'
    (project / 'include').mkdir(parents=True)
    files = {
        'a.c': '#define MODE 1\n#include "include/shared.h"\n',
        'b.c': '#define MODE 1\n#include "./include/shared.h"\n#include "include/leaf.h"\n',
        'include/shared.h': '#ifndef MODE\n#error needs_translation_unit\n#endif\n#pragma once\n#include "leaf.h"\n',
        'include/leaf.h': '#ifndef LEAF\n#define LEAF\n#include "cycle.h"\n#endif\n',
        'include/cycle.h': '#include "leaf.h"\n',
        'unrelated.c': 'int unrelated;\n',
        'orphan.h': '#include "only-orphan.h"\n',
        'only-orphan.h': '',
        'ignored.txt': '#error not_a_source\n',
    }
    for name, text in files.items():
        (project / name).write_text(text)
    (project / '.cache').mkdir()
    (project / '.cache' / 'hidden.c').write_text('#error hidden\n')
    (project / 'loop').symlink_to(project, target_is_directory=True)
    (project / 'alias.c').symlink_to(project / 'a.c')
    expected = None
    connected = ['a.c', 'b.c', 'include/shared.h', 'include/leaf.h', 'include/cycle.h']
    for name in connected:
        graph = run([preprocessor, '--graph', 'project', str(project / name)], cwd=work).stdout
        if expected is None:
            expected = graph
        elif graph != expected:
            raise SystemExit(f'Dependency graph changed when selecting {name}')
    nodes = dict(re.findall(r'  (n\d+) \[label="([^"]*)"\];', expected))
    if sorted(nodes.values()) != sorted(str(project / name) for name in connected):
        raise SystemExit(f'Incorrect dependency nodes: {expected}')
    edges = {(nodes[a], nodes[b]) for a, b in re.findall(r'  (n\d+) -> (n\d+);', expected)}
    wanted = [('a.c', 'include/shared.h'), ('b.c', 'include/shared.h'), ('b.c', 'include/leaf.h'),
              ('include/shared.h', 'include/leaf.h'), ('include/leaf.h', 'include/cycle.h'),
              ('include/cycle.h', 'include/leaf.h')]
    if edges != {(str(project / a), str(project / b)) for a, b in wanted}:
        raise SystemExit(f'Incorrect dependency edges: {expected}')
    alias = run([preprocessor, '--graph', './project', 'project/include/../include/leaf.h'], cwd=work).stdout
    if alias != expected:
        raise SystemExit('Dependency graph changed for normalized relative paths')
    orphan = run([preprocessor, '--graph', str(project), str(project / 'orphan.h')]).stdout
    leaf = run([preprocessor, '--graph', str(project), str(project / 'only-orphan.h')]).stdout
    if orphan != leaf or 'a.c' in orphan or 'orphan.h' not in orphan:
        raise SystemExit('Header-only dependency component is incorrect')
    single = run([preprocessor, '--graph', str(project), str(project / 'unrelated.c')]).stdout
    if 'unrelated.c' not in single or '->' in single:
        raise SystemExit('Disconnected source was not isolated')
    absent = subprocess.run([preprocessor, '--graph', str(project), str(project / 'absent.h')], capture_output=True, text=True, timeout=60)
    if not absent.returncode or '(-9)' not in absent.stderr:
        raise SystemExit('Missing dependency node was not diagnosed')
    run([preprocessor, '--graph-check', str(project)])
    print('MetaPP dependency graphs: entry-point independence, reverse edges, cycles, orphan headers, paths, limits and allocation failures passed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preprocessor', required=True)
    parser.add_argument('--compiler', default='cc')
    args = parser.parse_args()
    compiler = shlex.split(args.compiler)
    with tempfile.TemporaryDirectory(prefix='mulib-metapp-',
                                    dir='/tmp/opencode' if Path('/tmp/opencode').is_dir() else None) as work:
        work = Path(work)
        (work / 'config.h').write_text('#pragma once\n#define VALUE 42\nheader VALUE\n')
        count = 0
        for cpp, cases in ((False, CASES), (True, CPP_CASES)):
            for source in cases:
                path = work / ('input.cpp' if cpp else 'input.c')
                path.write_text(source)
                ours = run([args.preprocessor, '--cpp' if cpp else '--file', str(path)]).stdout
                reference = run(compiler + ['-E', '-P', '-undef', '-nostdinc',
                    '-std=c++20' if cpp else '-std=c17', '-x', 'c++' if cpp else 'c', str(path)]).stdout
                if tokens(ours, cpp) != tokens(reference, cpp):
                    raise SystemExit(f'Preprocessor mismatch:\n{source}\nMetaPP: {ours}\nReference: {reference}')
                count += 1
        # Independent signed/unsigned, precedence and short-circuit reference checks.
        for value in range(-16, 17):
            source = f'#if (({value} * 3 + 17) < 8U) || (({value}) < 0 && (7%3)==1)\nyes\n#else\nno\n#endif\n'
            path = work / 'expression.c'; path.write_text(source)
            ours = run([args.preprocessor, '--file', str(path)]).stdout
            reference = run(compiler + ['-E', '-P', '-x', 'c', str(path)]).stdout
            if tokens(ours) != tokens(reference):
                raise SystemExit(f'Conditional mismatch: {source}\n{ours}\n{reference}')
            count += 1
        import platform
        if os.name == 'posix':
            dependency_graphs(args.preprocessor, work)
        if platform.system() == 'Linux' and platform.machine() == 'x86_64':
            obj = work / 'plugin.o'
            run(compiler + ['-std=c11', '-O2', '-fPIC', '-fno-builtin', '-fno-stack-protector',
                '-fno-asynchronous-unwind-tables', '-fno-unwind-tables', '-I'+str(ROOT/'include'), '-c',
                str(ROOT/'examples/plugins/metapp-count.c'), '-o', str(obj)])
            run([args.preprocessor, '--native', str(obj)])
            rejected = reject_objects(args.preprocessor, obj, work)
            print(f'MetaPP native ELF plugin: relocation, execution, allocation failures and {rejected} malformed objects passed')
        print(f'MetaPP compiler reference: {count} cases passed')


if __name__ == '__main__':
    main()
