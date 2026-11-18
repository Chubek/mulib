"""Real native LuaEmbed, bounded Mune and canonical PPX validation."""
import json
import os
import sys

from support import NativeTests, run

t = NativeTests()
plugin = t.compiler.parent / 'stdppx/on1x-ppx-LuaEmbed'
schema = t.root / 'grammar/PPX.schema.json'
env = {**os.environ, 'ON1X_PPX_SCHEMA': str(schema.resolve())}
flags = ['--ppx', 'LuaEmbed', plugin]


def carrier(mode, lua):
    # On1x string literals and JSON share escaping for these ASCII fixtures.
    return '@LuaEmbed(' + json.dumps(mode) + ',' + json.dumps(lua) + ')'


def execute(name, source, expected, extra=()):
    path = t.source(name, source)
    generated, executable = t.work / (name + '.c'), t.work / name
    result = run([t.compiler, '--emit-c', path, *flags, *extra, '-o', generated], env=env)
    assert result.returncode == 0 and not result.stdout, (name, result.stderr)
    first = generated.read_bytes()
    repeated = run([t.compiler, '--emit-c', path, *flags, *extra], env=env)
    assert repeated.returncode == 0 and repeated.stdout.encode() == first, (name, repeated.stderr)
    result = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                  '-I', t.root / 'runtime', generated, t.root / 'runtime/on1x_runtime.c',
                  *t.flags, '-o', executable])
    assert result.returncode == 0, (name, result.stderr)
    result = run([executable])
    assert result.returncode == 0 and result.stdout == expected, (name, result.stdout, result.stderr)
    assert not result.stderr, (name, result.stderr)
    assert 'mulib_mune_' not in generated.read_text()
    return path


def reject(name, source, message=None, extra=()):
    path = t.source('bad_' + name, source)
    output = t.work / 'preserved.c'
    output.write_text('preserved\n')
    result = run([t.compiler, '--emit-c', path, *flags, *extra, '-o', output], env=env)
    assert result.returncode != 0 and not result.stdout, (name, result.stdout, result.stderr)
    assert output.read_text() == 'preserved\n', (name, result.stderr)
    if message:
        assert message in result.stderr, (name, result.stderr)


execute('stdout', 'fn main(){io_stdout("before:");' + carrier('stdout',
        'for i=1,3 do print(i) end; io.write("tail"); io.stdout:write("!")') +
        ';io_stdout(":after");}', 'before:1\n2\n3\ntail!:after')
execute('text', 'fn main(){println(' + carrier('text', 'io.write("answer: ",6*7)') + ');}',
        'answer: 42\n')
execute('expr', 'fn main(){println(' + carrier('expr',
        'local L=require("llembed"); print(L.integer(6*7))') + ');}', '42\n')
execute('statements', 'fn main(){' + carrier('statements',
        'local L=llembed; L.emit(L.do_(L.call("println",{L.integer(40)})));'
        'L.emit(L.do_(L.call("println",{L.integer(2)})))') + ';}', '40\n2\n')
execute('program', carrier('program',
        'local L=llembed; print(L.unit({L.function_("main",L.type_("VoidType"),'
        '{L.do_(L.call("println",{L.integer(42)}))})}))') + ' fn placeholder(){}', '42\n')
execute('program_chain', carrier('program',
        'local L=llembed; L.emit(L.unit({L.function_("main",L.type_("VoidType"),{'
        'L.do_(L.extension("stdout","print(\\\"generated program\\\")")),'
        'L.do_(L.call("println",{L.extension("expr","llembed.emit(llembed.integer(42))")}))})}))')
        + ' fn placeholder(){}', 'generated program\n42\n')
execute('chain', 'fn main(){println(' + carrier('expr',
        'llembed.emit(llembed.extension("expr", "llembed.emit(llembed.integer(42))"))') + ');}', '42\n')
execute('compose', 'fn main(){' + carrier('statements',
        'local L=llembed; L.emit(L.do_(L.node("ExtensionExpr",'
        '{L.quote("assertions"),L.quote("assert_true"),L.list({L.boolean(true)}),L.location})))')
        + ';println(42);}', '42\n',
        ['--ppx', 'assertions', t.compiler.parent / 'stdppx/on1x-ppx-assertions'])
execute('shadow', 'fn main(){let io_stdout=fn(s:string){tool_fail(s);};' +
        carrier('stdout', 'print("not shadowed")') + ';}', 'not shadowed\n')
execute('binary_escapes', 'fn main(){io_stdout(' + carrier('text',
        'io.write("quote\\\" slash\\\\ line\\n tab\\t",string.char(195,169))') +
        ');}', 'quote" slash\\ line\n tab\té')
execute('restricted', 'fn main(){' + carrier('stdout',
        'assert(os==nil and debug==nil and package==nil and load==nil and loadfile==nil);'
        'assert(dofile==nil and coroutine==nil and io.open==nil and io.popen==nil);'
        'assert(math.random==nil and collectgarbage==nil); print("restricted")') +
        ';}', 'restricted\n')
fresh_lua = ('local L=llembed; local a=L.fresh(); local b=L.fresh(); assert(a~=b);'
             'L.emit(L.node("Let",{L.quote(a),L.type_("I64Type"),L.integer(42),L.location}));'
             'L.emit(L.do_(L.call("println",{L.ident(a)})))')
execute('fresh', 'fn main(){let __llembed_1_1=0;' + carrier('statements', fresh_lua) + ';' +
        carrier('statements', fresh_lua) + ';}', '42\n42\n')
execute('runtime_order', 'fn greet(){' + carrier('stdout', 'print("hello")') +
        ';}fn main(){for i in 0..2{greet();}if false{' +
        carrier('stdout', 'print("not executed")') + ';}}', 'hello\nhello\n')
execute('integer_boundary', 'fn main(){println(' + carrier('expr',
        'llembed.emit(llembed.integer(math.mininteger))') + ');}', '-9223372036854775808\n')

for name, mode, script, message in [
    ('syntax', 'stdout', 'local =', 'LuaEmbed'),
    ('semantic', 'stdout', 'break', 'LuaEmbed'),
    ('runtime', 'stdout', 'print("partial"); error("rejected")', 'rejected'),
    ('nul', 'stdout', 'pcall(function() io.write(string.char(0)) end)', 'NUL'),
    ('output', 'stdout', 'local s=string.rep("x",8192);'
     'pcall(function() for i=1,65 do io.write(s) end end)', 'output'),
    ('steps', 'stdout', 'while true do end', 'LuaEmbed'),
    ('memory', 'stdout', 'local x=string.rep("x",16777216)', 'LuaEmbed'),
    ('require', 'stdout', 'require("outside")', 'llembed'),
    ('source', 'text', ' ' * 65537, 'input'),
    ('trailing', 'expr', 'print(llembed.integer(1)); print(llembed.integer(2))', 'trailing'),
    ('category', 'expr', 'print(llembed.do_(llembed.integer(1)))', 'category'),
    ('unknown', 'expr', 'print("(Unknown)")', 'constructor'),
    ('type', 'expr', 'print(llembed.call("println",{}))', 'arity'),
    ('stmt_tail', 'statements', 'print(llembed.do_(llembed.integer(1))); io.write(")")', 'S-expression'),
    ('unsafe', 'expr', 'print("(UnsafeExpr)")', 'constructor'),
    ('bad_op', 'bogus', '', 'expected stdout'),
    ('bad_helper', 'expr', 'print(llembed.integer(1.5))', 'i64'),
]:
    statement = mode in ('stdout', 'statements', 'bogus')
    source = ('fn main(){' + carrier(mode, script) + ';}' if statement else
              'fn main(){println(' + carrier(mode, script) + ');}')
    reject(name, source, message)
reject('literal', 'fn main(){let s="print(42)";@LuaEmbed("stdout",s);}', 'literal')
reject('marker', carrier('program', 'print(llembed.unit({}))') + ' fn marker(){println(1);}', 'marker')
reject('collision', 'fn main(){}' + carrier('program',
       'local L=llembed; print(L.unit({L.function_("main",L.type_("VoidType"),{})}))') +
       ' fn marker(){}', 'collides')
reject('unregistered', 'fn main(){println(' + carrier('expr',
       'llembed.emit(llembed.extension("text","", "Unauthorized"))') + ');}', 'unregistered')
reject('recursive', 'fn main(){println(' + carrier('expr',
       'llembed.emit(llembed.extension("expr", "llembed.emit(llembed.extension(\\\"expr\\\", \\\"\\\"))"))') +
       ');}', 'S-expression')
# Real nonterminating generation uses the current source from a Lua long string.
self_script = ('local s="local s=%q; llembed.emit(llembed.extension(\\\"expr\\\",string.format(s,s)))";'
               'llembed.emit(llembed.extension("expr",string.format(s,s)))')
reject('chain_limit', 'fn main(){println(' + carrier('expr', self_script) + ');}', 'pass limit')
reject('expansion_limit', 'fn main(){' + (carrier('stdout', '') + ';') * 65 + '}', 'expansion limit')

protocol = {**env, 'ON1X_PPX_PROTOCOL': '1', 'ON1X_PPX_LABEL': 'LuaEmbed'}
for wire in ('(Unit 1 ())', '(Unit 1 ()) trailing', '(Unit 1 (', '(Unit 99 ())'):
    result = run([plugin], input=wire, env=protocol)
    if wire == '(Unit 1 ())':
        assert result.returncode == 0 and result.stdout == '(Unit 1 ())\n', result
    else:
        assert result.returncode != 0 and not result.stdout, result
result = run([plugin], input='(Unit 1 ())', env={**protocol, 'ON1X_PPX_PROTOCOL': '2'})
assert result.returncode != 0 and not result.stdout, result

# Name discovery supplies the installed/copied schema automatically.
discovered = t.work / 'discovered'
result = run([sys.executable, t.root / 'tools/on1x-build.py', t.work / 'expr.on1x',
              '--on1xc', t.compiler, '--cc', t.cc, '--stdppx-path', plugin.parent,
              '--use-ppx', 'LuaEmbed', '-o', discovered,
              *['--cflag=' + flag for flag in t.flags]])
assert result.returncode == 0, result.stderr
assert run([discovered]).stdout == '42\n'

example = t.work / 'example'
result = run([sys.executable, t.root / 'tools/on1x-build.py',
              t.root / 'stdppx/examples/luaembed.on1x', '--on1xc', t.compiler, '--cc', t.cc,
              '--stdppx-path', plugin.parent, '--use-ppx', 'LuaEmbed', '-o', example,
              *['--cflag=' + flag for flag in t.flags]])
assert result.returncode == 0, result.stderr
assert run([example]).stdout == 'Lua output at this runtime position\n42\ngenerated statement\n7\n'

plan = json.loads((t.compiler.parent / 'luaembed/luaembed_plan.json').read_text())
assert plan['implementation_translation_units'] == 1
assert plan['libraries'] == ['mune']
assert plan['implementation_macros'].count('MULIB_IMPL_MUNE') == 1
assert {'mune', 'mir', 'vmweave', 'parsec'} <= {item['name'] for item in plan['modules']}
print('LuaEmbed: runtime output, AST streams/programs, chaining, protocol and bounds PASS')
