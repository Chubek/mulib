#!/usr/bin/env python3
"""Check independent Lua execution, generated C, and optional Lua 5.4 equivalence."""
import argparse
import ast
import os
from pathlib import Path
import random
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def run(command, cwd):
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=180)
    if result.returncode not in (0, 77):
        raise RuntimeError(f'{shlex.join(map(str, command))}\n{result.stderr or result.stdout}')
    return result


def corpus():
    test = (ROOT / 'tests/test_mune_runtime.c').read_text()
    cases = [ast.literal_eval(s) for s in re.findall(r'execute\(s,\s*("(?:\\.|[^"\\])*")\)', test)
             if 'test_bundle' not in s and 'host_data' not in s and 'native_roundtrip' not in s]
    return '\n'.join('do ' + case + ' end' for case in cases), len(cases)


def differential():
    rng = random.Random(54)
    values = ['0', '1', '-1', '2', '-3', '7', '-9223372036854775808', '9223372036854775807',
              '9007199254740993', '0.0', '-0.0', '0.5', '1e300', '0/0', '1/0', '"3"', '"3.0"']
    ops = ['+', '-', '*', '/', '//', '%', '^', '&', '|', '~', '<<', '>>', '==', '<', '<=']
    expressions = [f'({rng.choice(values)}) {rng.choice(ops)} ({rng.choice(values)})' for _ in range(400)]
    expressions += [
        "string.gsub('ababa','a.-a','[%0]')", "string.match('xx 123 yy','()(%d+)()')",
        "string.find('abc','^b',2,true)", "string.match('a(b(c)d)e','%b()')",
        "string.gsub('abc','.*','x')", "string.gsub('abc','a*','x')",
        "string.gsub('aaa','a?', 'x')", "string.gsub('ab','(.)',{a='x'})",
        "math.modf(-2.5)", "math.sin(0,{})", "math.log(1,nil)", "math.atan(1,nil)",
        "string.format('%q',1.5)", "string.format('%d',3.0)", "string.format('%s','a\\0b')",
        "string.unpack('>i2I2','\\255\\254\\0\\42')",
        "utf8.len('a\\128')", "utf8.offset('abc',0,4)", "pcall(error,nil)",
        "(pcall(function() return '3' & 2 end))", "math.tointeger('12')", "tonumber('0x1.fp2')",
    ]
    for byteorder in ('<', '>'):
        for kind in ('i', 'I'):
            for size in (1, 2, 4, 8, 9, 16):
                for value in ('-1', '0', '255', 'math.mininteger', 'math.maxinteger'):
                    expressions.append(f"string.pack('{byteorder}{kind}{size}',{value})")
    for fmt in ('Xx', 'Xi4', 'X i4', 'X<i4', '!3b', '!3i4', '!4bi4', 'i16', 'z', 'c0', 's0'):
        expressions.append(f"string.packsize('{fmt}')")
    expressions += [
        "string.unpack('i9',string.pack('I8',-1)..'\\0')",
        "string.unpack('>i16',string.rep('\\255',16))",
        "string.unpack('I1','a',nil)", "string.format('%q',math.mininteger)",
        "string.format('%+s','x')", "string.format('%03s','x')", "string.format('%.1c',65)",
        "string.format('%#d',1)", "string.format('%+u',1)", "string.format('%100d',1)",
        "math.fmod(1,nil)", "tostring()", "tonumber()", "rawequal()",
        "debug.getmetatable()", "debug.getmetatable(nil)",
        "debug.setmetatable(1)", "debug.getuservalue()", "debug.getuservalue(nil)",
        "rawset({},'key')", "rawget({},nil)", "getmetatable(nil)",
        "string.find('abc','',4)", "string.find('abc','',5)",
        "string.match('abc','',4)", "string.match('abc','',5)",
        "string.rep('abc',0,{})", "string.rep('abc',-1,{})",
        "utf8.codepoint('abc',4)", "utf8.codepoint('abc',1,0)",
        "utf8.offset('abc',0,0)", "utf8.offset('abc',-1,1)",
        "utf8.len('abc',4)", "utf8.len('abc',5)",
        "string.packsize('!0i4')", "string.packsize('!16i8')",
        "string.pack('c0','')", "string.pack('s1',42)",
        "string.unpack('c0','',1)", "string.unpack('c0','',2)",
        "table.remove({},0)", "table.remove({},1)", "table.remove({},2)",
        "(pcall(load,42))",
    ]
    # Library semantics, including coercions, lazy captures, invalid UTF-8,
    # null pointer formatting, and deterministic Lua 5.4 random generation.
    expressions += [
        "math.abs('3')", "math.fmod('3','2')", "math.modf(-math.huge)",
        "math.random(1,2,3)", "math.randomseed(nil)", "table.move({})",
        "io.type()", "io.close(nil)", "coroutine.isyieldable(nil)",
        "debug.setuservalue(nil,nil)", "debug.setupvalue(function() end,1)",
        "debug.setlocal(1,1)", "select('#rest',1,nil,3)",
        "string.find(')',')')", "string.find('abc',')')",
        "string.match('abc','%0')", "string.gsub('abc','x',false,0)",
        "string.gsub('abc','x',nil)", "string.gsub('abc','(','x')",
        "string.gsub('abc','(a)(','%1')", "string.gsub('abc','()','%1')",
        "string.match('xx','%bxx')", "utf8.codepoint('abc',5,1)",
        "string.format('%p',nil)", "string.format('%10p',false)",
        "string.format('%+p',nil)", "string.format('%.2p',nil)",
        "string.format('%q',math.huge)", "string.format('%q',-math.huge)",
        "os.date('%Q',0)", "os.date('%',0)", "os.date('%E',0)",
        "os.date('a\\0b',0)", "os.date('!%Y-%m-%d',0)",
        "package.searchpath('x','first/?.lua;second/?.lua')",
        "package.searchpath('x',42)", "package.searchpath('x','')",
        "package.searchpath('x',';;')", "package.searchpath('x','?.lua',false)",
        "package.searchpath('x','?.lua',nil,{})", "select('#',debug.gethook())",
        "string.match('a','()%1')",
    ]
    for seed in ((0, 0), (1, 2), (-1, -2), (9223372036854775807, -9223372036854775808)):
        for call in ('math.random()', 'math.random(0)', 'math.random(3)',
                     'math.random(-10,10)', 'math.random(math.mininteger,math.maxinteger)'):
            expressions.append(f'(function() math.randomseed({seed[0]},{seed[1]}); '
                               f'local a,b,c={call},{call},{call}; return a,b,c end)()')
    for point in (0, 0x7f, 0x80, 0x3ff, 0x400, 0x7ff, 0x800, 0x3fff, 0x4000, 0x7fff,
                  0x8000, 0xd800, 0xffff, 0x10000, 0x10ffff, 0x110000, 0x1fffff,
                  0x200000, 0x3ffffff, 0x4000000, 0x7fffffff):
        expressions.append(f'utf8.char({point})')
        expressions.append(f"'\\u{{{point:x}}}'")
        expressions.append(f'utf8.codepoint(utf8.char({point}),1,-1,true)')
    for text in ("'a\\128b'", "'\\194\\162\\128'", "'\\192\\128'", "'\\255'", "'\\237\\160\\128'"):
        for lax in ('false', 'true'):
            expressions.append(f'(function() local t={{}}; for p,c in utf8.codes({text},{lax}) '
                               "do t[#t+1]=p..':'..c end; return table.concat(t,',') end)()")
    for pattern in ("')'", "'%0'", "'%bxx'", "'%f[%z]'", "'a?'", "'a*'", "'.*'", "''", "'^'", "'$'", "'()'", "'x'"):
        for text in ("''", "'abc'", "'xxx'", "'a\\0b'"):
            for replacement in ('false', 'nil', '42', "'%1'", "'%0'", "'%%'", '{}'):
                expressions.append(f'string.gsub({text},{pattern},{replacement})')
    pattern_rng = random.Random(5412)
    tokens = ['a', 'b', '.', '[a-c]', '[^a]', '[z-a]', '%a', '%d', '%z', '%b()', '%bxx',
              '%f[%w]', '()', '(a)', '(', '(%a*)', '(%a+)', '(%a?)(%a*)', '%1', '%0',
              '%', '[', ')', '^', '$', '*', '?', '-', '+', '\0']
    def quote(text):
        return '"' + ''.join(f'\\x{ord(c):02x}' for c in text) + '"'
    for i in range(180):
        text = quote(''.join(pattern_rng.choices('abc()012\0\n', k=pattern_rng.randrange(12))))
        pattern = quote(''.join(pattern_rng.choices(tokens, k=pattern_rng.randrange(1, 6))))
        if i % 3 == 2:
            replacement = quote(pattern_rng.choice(['x', '%0', '%1', '%2', '%%']))
            expressions.append(f'string.gsub({text},{pattern},{replacement})')
        else:
            name = 'find' if i % 3 == 0 else 'match'
            expressions.append(f'string.{name}({text},{pattern})')
    preamble = '''local function value(v)
      if type(v)=='number' then
        if v~=v then return 'number:nan' end
        return math.type(v)..':'..tostring(v)
      elseif type(v)=='string' then
        return 'string:'..(v:gsub('.',function(c) return string.format('%02x',string.byte(c)) end))
      else return type(v)..':'..tostring(v) end
    end
    local function check(i,f)
      local r=table.pack(pcall(f)); io.write(i)
      if not r[1] then io.write(':error\\n'); return end
      for j=2,r.n do io.write('|',value(r[j])) end
      io.write('\\n')
    end
    '''
    return preamble + '\n'.join(f'check({i},function() return {expr} end)' for i, expr in enumerate(expressions)), expressions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', required=True)
    parser.add_argument('--compiler', default=os.environ.get('CC', 'cc'))
    parser.add_argument('--lua', default=shutil.which('lua5.4') or shutil.which('lua54'))
    parser.add_argument('--sanitizers', action='store_true')
    args = parser.parse_args()
    runtime = str(Path(args.runtime).resolve())
    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mune-runtime-', dir=parent) as temp:
        work = Path(temp)
        source = work / 'corpus.lua'
        text, count = corpus()
        source.write_text(text)
        if args.lua:
            run([args.lua, str(source)], work)
        modes = []
        for mode in ('--run', '--vm', '--jit'):
            if run([runtime, mode, str(source)], work).returncode != 77:
                modes.append(mode)
        generated = work / 'generated.c'
        module = work / 'module.lua'
        module.write_text('return {answer=42}')
        with (work / 'test_bundle.luac').open('wb') as bundle:
            subprocess.run([runtime, '--bundle', str(module)], cwd=work, stdout=bundle, check=True, timeout=60)
        # Exercise generated branches, closures, calls, construction, arithmetic,
        # short circuiting, multiple results, goto and coroutine suspension.
        source.write_text(text + '\n' + '''local n=0; ::again:: n=n+1; if n<4 then goto again end
          local function f(x,...) if x<=0 then return ... end return f(x-1,...) end
          local t={f(4,10,20,30)}; assert(n==4 and #t==3 and t[2]==20)
          local co=coroutine.create(function() for i=1,3 do coroutine.yield(i*2) end return 42 end)
          for i=1,4 do local ok,v=coroutine.resume(co); assert(ok and v==(i==4 and 42 or i*2)) end
          local a=false; assert((a and error('bad') or 42)==42)
          local x={}; function x:m(a) return a+2 end; assert(x:m(40)==42)
          local g=assert(load('return 42')); assert(g()==42)
          package.path='./?.luac;'..package.path
          assert(require('test_bundle').answer==42)
          print('Mune AOT passed')''')
        generated.write_text(run([runtime, '--emit', str(source)], work).stdout)
        binary = work / 'aot'
        flags = ['-std=c11', '-O1', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I' + str(ROOT / 'kernels')]
        if os.name == 'posix':
            flags += ['-pthread']
        if sys.platform.startswith(('linux', 'freebsd')):
            flags += ['-rdynamic']
        if args.sanitizers:
            flags += ['-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
        links = [] if os.name == 'nt' else ['-lm', '-ldl']
        run(shlex.split(args.compiler) + flags + [str(generated)] + links + ['-o', str(binary)], work)
        assert run([str(binary)], work).stdout == 'Mune AOT passed\n'
        if os.name == 'posix':
            plugin = work / 'probe.c'
            plugin.write_text('''#include "mulib-mune.h"
mulib_mune_status_t muneopen_probe(mulib_mune_state_t *s,const mulib_mune_value_t *a,
 size_t n,mulib_mune_result_t *r,void *u) {
 mulib_mune_value_t table,key,value={MULIB_MUNE_INTEGER,{.integer=42}};
 (void)u;
 if(n!=2 || a[0].type!=MULIB_MUNE_STRING || a[1].type!=MULIB_MUNE_STRING) return MULIB_MUNE_INVALID;
 if(mulib_mune_make_table(s,&table)!=MULIB_MUNE_OK || !mulib_mune_result_push(r,table)
    || mulib_mune_make_string(s,"answer",6,&key)!=MULIB_MUNE_OK
    || mulib_mune_raw_set(s,table,key,value)!=MULIB_MUNE_OK) return MULIB_MUNE_MEMORY;
 mulib_mune_collect(s); return MULIB_MUNE_OK;
}
mulib_mune_status_t muneopen_probe_sub(mulib_mune_state_t *s,const mulib_mune_value_t *a,
 size_t n,mulib_mune_result_t *r,void *u) { return muneopen_probe(s,a,n,r,u); }
mulib_mune_status_t muneopen_fallback(mulib_mune_state_t *s,const mulib_mune_value_t *a,
 size_t n,mulib_mune_result_t *r,void *u) { return muneopen_probe(s,a,n,r,u); }
\n''')
            run(shlex.split(args.compiler) + (['-undefined', 'dynamic_lookup'] if sys.platform == 'darwin' else [])
                + ['-std=c11', '-shared', '-fPIC', '-I' + str(ROOT / 'kernels'),
                                               str(plugin), '-o', str(work / 'probe.so')], work)
            shutil.copyfile(work / 'probe.so', work / 'probe-v2.so')
            shutil.copyfile(work / 'probe.so', work / 'ignored-fallback.so')
            (work / 'filemodule.lua').write_text('local name,path=...; assert(name=="filemodule"); return {value=42}')
            source.write_text('''local p,path=require('probe'); assert(p.answer==42 and path=='./probe.so')
              local sub,subpath=require('probe.sub'); assert(sub.answer==42 and subpath=='./probe.so')
              assert(require('probe-v2').answer==42 and require('ignored-fallback').answer==42)
              assert(not pcall(require,'probe.absent'))
              assert(type(package.loadlib('./probe.so','muneopen_probe'))=='function')
              assert(package.loadlib('./probe.so','*')==true)
              local f,message,where=package.loadlib('./probe.so','missing'); assert(f==nil and type(message)=='string' and where=='init')
              assert(require('filemodule').value==42)
              local pipe=assert(io.popen('printf hello','r')); assert(pipe:read('a')=='hello');
              local ok,how,code=pipe:close(); assert(ok and how=='exit' and code==0)''')
            for mode in modes:
                run([runtime, mode, str(source)], work)
        # Version-specific environment variables take precedence; a double
        # separator splices in the implementation's documented default path.
        source.write_text('''assert(package.path=='first;./?.lua;./?/init.lua;./?.luac;last')
          assert(package.cpath=='native/?.so')''')
        env = dict(os.environ, LUA_PATH='ignored', LUA_PATH_5_4='first;;last',
                   LUA_CPATH='ignored', LUA_CPATH_5_4='native/?.so')
        for mode in modes:
            result = subprocess.run([runtime, mode, str(source)], cwd=work, env=env,
                                    capture_output=True, text=True, timeout=60)
            assert result.returncode == 0, result.stderr
        source.write_text('''local t=setmetatable({}, {__gc=function() print('finalized') end})
          local x<close> =setmetatable({}, {__close=function() print('closed') end})
          local co=coroutine.create(function()
            local y<close> =setmetatable({}, {__close=function() error('unexpected coroutine cleanup') end})
            os.exit(0,true)
          end)
          coroutine.resume(co); error('exit returned')''')
        if args.lua:
            assert run([args.lua, str(source)], work).stdout == 'closed\nfinalized\n'
        for mode in modes:
            assert run([runtime, mode, str(source)], work).stdout == 'closed\nfinalized\n'
        checked = 0
        if args.lua:
            text, expressions = differential()
            preamble = text[:text.index('check(0,')]
            # Bound parser/AST usage independently of the corpus size. Each
            # expression is isolated; explicitly seeded RNG cases are repeatable.
            for start in range(0, len(expressions), 128):
                batch = expressions[start:start + 128]
                source.write_text(preamble + '\n'.join(
                    f'check({i},function() return {expr} end)'
                    for i, expr in enumerate(batch, start)))
                expected = run([args.lua, str(source)], work).stdout.splitlines()
                for mode in modes:
                    actual = run([runtime, mode, str(source)], work).stdout.splitlines()
                    if actual != expected:
                        mismatch = [(i, expressions[i], a, b)
                                    for i, (a, b) in enumerate(zip(expected, actual), start) if a != b]
                        raise RuntimeError(f'{mode} differential mismatch ({len(expected)} / {len(actual)} lines):\n'
                                           + '\n'.join(map(str, mismatch[:20])))
            checked = len(expressions)
        print(f'Mune: {count} assertion groups in {len(modes)} modes; generated C; {checked} Lua 5.4 differential cases')


if __name__ == '__main__':
    main()
