import json
import os
from pathlib import Path
import sys
import random
from support import NativeTests, run

t = NativeTests()
sys.path.insert(0, str(t.root/'stdlib'))
from sexpr import Atom, loads, dumps
from ppx import Schema

schema_path = (t.root/'grammar/PPX.schema.json').resolve()
environment = {**os.environ, 'ON1X_PPX_SCHEMA': str(schema_path), 'ON1X_PPX_LABEL': 'native', 'ON1X_PPX_PROTOCOL': '1'}
processors = {name: t.build('native_'+name, source=t.root/'stdppx/native'/(name+'.on1x'))
              for name in ('identity', 'control', 'unroll', 'checks', 'derive', 'fold')}
codec = t.build('sexpr')
for wire in ['()', '(before ("\\x01\\x80\\xff\\n\\r\\t\\\"\\\\" before) ())', 'symbol', '""', '('+'a '*10000+')', '('*100+'x'+')'*100]:
    tree = loads(wire)
    # This example deliberately rewrites one symbol.
    def rename(n):
        return [rename(c) for c in n] if isinstance(n, list) else Atom('after') if isinstance(n, Atom) and n == 'before' else n
    p = run([codec], input=wire)
    assert p.returncode == 0 and p.stdout == dumps(rename(tree)), (wire[:100], p.returncode, p.stderr)
for wire in ['', '() ()', '(a', ')', '"bad\\q"', '"\\x00"', '"\\xgg"', '"bad\n"', 'a"b', '('*257+')'*257]:
    p = run([codec], input=wire)
    assert p.returncode == 73 and not p.stdout and 'error' in p.stderr, (wire[:100], p.returncode, p.stderr)

for name, text, extra, diagnostic in [
    ('text_bounds', 'fn main(){text_byte("x",1);}', [], 'index out of range'),
    ('slice_bounds', 'fn main(){text_slice("x",0,2);}', [], 'slice out of range'),
    ('wrong_head', 'fn main(){sx_head(sx_symbol("x"));}', [], 'nonempty list'),
    ('improper', 'fn main(){sx_cons(sx_nil(),sx_string("tail"));}', [], 'proper list'),
    ('bad_symbol', 'fn main(){sx_symbol("bad symbol");}', [], 'invalid S-expression symbol'),
    ('nul_byte', 'fn main(){buffer_byte(buffer_new(),0);}', [], 'NUL-free'),
    ('buffer_limit', 'fn main(){let b=buffer_new();var i=0;while i<17{buffer_byte(b,65);i=i+1;}}', ['-DON1X_DATA_MAX_TEXT=16'], 'buffer byte limit'),
    ('node_limit', 'fn main(){var list=sx_nil();var i=0;while i<5{list=sx_cons(sx_nil(),list);i=i+1;}}', ['-DON1X_DATA_MAX_NODES=4'], 'live data node limit'),
    ('pool_limit', 'fn main(){sx_string("x");}', ['-DON1X_DATA_MAX_POOL=16'], 'snapshot pool limit'),
    ('data_allocation', 'fn main(){sx_string("x");}', ['-DON1X_DATA_MALLOC(n)=((void)(n),(void*)0)'], 'text allocation failure'),
]:
    target = t.build('data_'+name, text=text, extra=extra)
    # Fatal tool failures, like the existing arithmetic/closure failures, do not
    # unwind language owners. Keep leak detection on all normal-exit tests.
    p = run([target], env={**os.environ, 'LSAN_OPTIONS': 'detect_leaks=0'})
    assert p.returncode == 73 and not p.stdout and diagnostic in p.stderr, (name, p.returncode, p.stderr)

for text in ['extern fn bad()->sexpr;fn main(){}', 'extern fn bad(x:buffer);fn main(){}', 'fn main(){println(sx_nil());}', 'fn main(){let a=sx_nil();assert(a==a);}']:
    p = run([t.compiler, '--emit-c', t.source('reject_managed_abi', text)])
    assert p.returncode == 1 and not p.stdout, p.stderr

hooks = ['-include', t.root/'tests/fixtures/closure_allocator.h', t.root/'tests/fixtures/closure_allocator.c']
t.valid('managed_data', 'original\nsnapshot\n', extra=hooks, text='''
extern fn live_environments()->i64;
fn main(){
 {let b=buffer_new();buffer_append(b,"original");let copy=buffer_text(b);buffer_clear(b);buffer_append(b,"snapshot");println(copy);println(buffer_text(b));
  let atom=sx_string(copy);let pair=(atom,fn()->sexpr{return atom;});let(a,f)=pair;assert(sx_text(f())=="original");}
 assert(data_live_nodes()==0);assert(live_environments()==0);
 {var list=sx_nil();var i=0;while i<100000{list=sx_cons(sx_nil(),list);i=i+1;}}
 assert(data_live_nodes()==0);assert(live_environments()==0);
}
''')
visitor_import = json.dumps(str((t.root/'stdlib/visitor/visitor.on1x').resolve()))
t.valid('native_paths', '(0)\n(1)\n()\n', extra=hooks, text=f'''
import {visitor_import};
fn visit(node:sexpr,path:sexpr)->sexpr{{io_stdout(sexpr_print(path));return node;}}
fn main(){{{{let t=visitor_rewrite(sexpr_parse("(a b)"),visit);}}assert(data_live_nodes()==0);}}
''')
ppx_import = json.dumps(str((t.root/'stdlib/ppx/ppx.on1x').resolve()))
t.valid('native_fresh', 'on1x_ppx_2\non1x_ppx_3\n', extra=hooks, text=f'''
import {ppx_import};
fn main(){{{{let tree=sexpr_parse("(on1x_ppx_1)");let(a,n)=ppx_fresh(tree,0);let(b,m)=ppx_fresh(tree,n);println(a);println(b);}}assert(data_live_nodes()==0);}}
''')

sample = t.source('native_sample', '''
@d("derive","arity","signature") fn add(a:i64,b:i64)->i64{return a+b;}
fn main(){let assert=fn(x:bool){}; @c("assert_eq",add(40,2),42); @c("assert_ne",1,2);
 @u("repeat",2,{@k("unless",false,{println((10+11)*2);});}); println(add_arity());println(add_signature());}
''')
out = t.work/'native_pipeline.c'
p = run([t.compiler, '--emit-c', sample, '-o', out, '--ppx', 'c', processors['checks'], '--ppx', 'u', processors['unroll'],
         '--ppx', 'k', processors['control'], '--ppx', 'd', processors['derive'], '--ppx', 'f', processors['fold']], env=environment)
assert p.returncode == 0, p.stderr
exe = t.work/'native_pipeline'
p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', t.root/'runtime', out, t.root/'runtime/on1x_runtime.c', *t.flags, '-o', exe])
assert p.returncode == 0, p.stderr
p = run([exe]); assert p.returncode == 0 and p.stdout == '42\n42\n2\nfn(i64, i64) -> i64\n', (p.stdout, p.stderr)

# Identity validates independently and preserves canonical bytes, including MSP.
for example in ('staging', 'structural_macros', 'sexpr'):
    source = t.root/'examples'/(example+'.on1x')
    wire = run([t.compiler, '--emit-ast', source]); assert wire.returncode == 0, wire.stderr
    p = run([processors['identity']], input=wire.stdout, env=environment)
    assert p.returncode == 0 and p.stdout == wire.stdout, (example, p.stderr)
    p = run([t.compiler, '--emit-c', source, '--ppx', 'native', processors['identity']], env=environment)
    assert p.returncode == 0, (example, p.stderr)
    Schema().validate(loads(wire.stdout))

# Bootstrap the authoring library through its own native identity processor.
identity_source = t.root/'stdppx/native/identity.on1x'
p = run([t.compiler, '--emit-c', identity_source, '--ppx', 'native', processors['identity']], env=environment)
assert p.returncode == 0 and p.stdout == (t.work/'native_identity.c').read_text(), p.stderr

minimal = '(Unit 1 ((Function "main" () (VoidType) (Block () (Loc "test" 0 1 1)) () (Loc "test" 0 1 1))))'
for bad in [minimal.replace('Unit 1', 'Unit 2'), minimal.replace('"main"', '"if"'), minimal.replace('(VoidType)', '(PrivateHIR)'),
            minimal.replace('"test" 0', '"test" 18446744073709551616'), minimal+' ()']:
    p = run([processors['identity']], input=bad, env=environment)
    assert p.returncode == 73 and not p.stdout, p.stderr
for schema in ['{"version":1,"version":1}', '{"version":1,}', '"\\ud800"', '"\\udc00"', '['*65+']'*65,
               '{"schema_format":"on1x.sexpr-ast/v1","version":1,"categories":{"Syntax":""},"nodes":{}}']:
    path = t.work/'invalid-schema.json'; path.write_text(schema)
    p = run([processors['identity']], input=minimal, env={**environment, 'ON1X_PPX_SCHEMA': str(path.resolve())})
    assert p.returncode == 73 and not p.stdout, (schema[:100], p.stderr)

# Native extension-schema loading is real, not a hard-coded core-tag table.
extension = t.work/'extension.json'
extension.write_text('{"version":1,"nodes":{"native:Box":{"category":"Expression","fields":[["value","Expression"],["location","Location"]]}}}')
ext_source = t.source('native_extension', f'''
import {ppx_import};
fn lower(node:sexpr,path:sexpr)->sexpr{{if visitor_tag(node)=="native:Box"{{return sexpr_nth(node,1);}}return node;}}
fn transform(tree:sexpr,label:string)->sexpr{{return visitor_rewrite(tree,lower);}}
fn main(){{ppx_serve(ppx_schema_extend(ppx_schema_env(),"native",{json.dumps(str(extension.resolve()))}),transform);}}
''')
ext_exe = t.build('native_extension', source=ext_source)
wire = minimal.replace('(Block ()', '(Block ((Do (native:Box (DecimalLiteral "42" (Loc "test" 0 1 1)) (Loc "test" 0 1 1)) (Loc "test" 0 1 1)))')
p = run([ext_exe], input=wire, env=environment)
assert p.returncode == 0 and 'native:Box' not in p.stdout, p.stderr
Schema().validate(loads(p.stdout))
extension.write_text('{"version":1,"nodes":{"foreign:Box":{"category":"Expression","fields":[]}}}')
p = run([ext_exe], input=minimal, env=environment)
assert p.returncode == 73 and not p.stdout and 'prefix' in p.stderr, p.stderr

# Folding preserves bad-literal diagnostics and arithmetic traps, not just values.
for text, expected in [('fn main(){println(9223372036854775807+1);}', 70), ('fn main(){println(1/0);}', 70),
                       ('fn main(){println(-9223372036854775808 / -1);}', 70)]:
    source = t.source('native_trap', text)
    p = run([t.compiler, '--emit-c', source, '--ppx', 'fold', processors['fold'], '-o', out], env=environment)
    assert p.returncode == 0, p.stderr
    p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', t.root/'runtime', out, t.root/'runtime/on1x_runtime.c', *t.flags, '-o', exe]); assert p.returncode == 0, p.stderr
    p = run([exe]); assert p.returncode == expected and 'arithmetic error' in p.stderr, p.stderr
source = t.source('native_bad_literal', 'fn main(){println(9223372036854775808 * 0);}')
p = run([t.compiler, '--emit-c', source, '--ppx', 'fold', processors['fold']], env=environment)
assert p.returncode == 1 and not p.stdout and 'out of i64 range' in p.stderr, p.stderr

# Differential folding against the Python bootstrap, including i64 boundaries.
sys.path.insert(0, str(t.root/'stdppx'))
from passes import fold
rng = random.Random(20261114)
values = [-(1 << 63), -(1 << 63)+1, -7, -1, 0, 1, 7, (1 << 63)-2, (1 << 63)-1]
location = [Atom('Loc'), 'fold-test', Atom('0'), Atom('1'), Atom('1')]
expressions = []
for _ in range(160):
    a, b = rng.choice(values), rng.choice(values)
    expressions.append([Atom('Do'), [Atom('Binary'), Atom(rng.choice(['+', '-', '*', '/', '%'])),
                                    [Atom('DecimalLiteral'), str(a), location], [Atom('DecimalLiteral'), str(b), location], location], location])
tree = [Atom('Unit'), Atom('1'), [[Atom('Function'), 'main', [], [Atom('VoidType')], [Atom('Block'), expressions, location], [], location]]]
p = run([processors['fold']], input=dumps(tree), env=environment)
assert p.returncode == 0 and p.stdout == dumps(fold(tree, 'native')), p.stderr
print('Native On1x sexpr/visitor/PPX libraries and six processors: PASS')
