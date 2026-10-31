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
          assert(require('test_bundle').answer==42)
          print('Mune AOT passed')''')
        generated.write_text(run([runtime, '--emit', str(source)], work).stdout)
        binary = work / 'aot'
        flags = ['-std=c11', '-O1', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I' + str(ROOT / 'include')]
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
}\n''')
            run(shlex.split(args.compiler) + (['-undefined', 'dynamic_lookup'] if sys.platform == 'darwin' else [])
                + ['-std=c11', '-shared', '-fPIC', '-I' + str(ROOT / 'include'),
                                              str(plugin), '-o', str(work / 'probe.so')], work)
            (work / 'filemodule.lua').write_text('local name,path=...; assert(name=="filemodule"); return {value=42}')
            source.write_text('''local p,path=require('probe'); assert(p.answer==42 and path=='./probe.so')
              assert(type(package.loadlib('./probe.so','muneopen_probe'))=='function')
              local f,message,where=package.loadlib('./probe.so','missing'); assert(f==nil and type(message)=='string' and where=='init')
              assert(require('filemodule').value==42)
              local pipe=assert(io.popen('printf hello','r')); assert(pipe:read('a')=='hello');
              local ok,how,code=pipe:close(); assert(ok and how=='exit' and code==0)''')
            for mode in modes:
                run([runtime, mode, str(source)], work)
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
            source.write_text(text)
            expected = run([args.lua, str(source)], work).stdout.splitlines()
            for mode in modes:
                actual = run([runtime, mode, str(source)], work).stdout.splitlines()
                if actual != expected:
                    mismatch = [(i, expressions[i], a, b) for i, (a, b) in enumerate(zip(expected, actual)) if a != b]
                    raise RuntimeError(f'{mode} differential mismatch ({len(expected)} / {len(actual)} lines):\n' + '\n'.join(map(str, mismatch[:20])))
            checked = len(expressions)
        print(f'Mune: {count} assertion groups in {len(modes)} modes; generated C; {checked} Lua 5.4 differential cases')


if __name__ == '__main__':
    main()
