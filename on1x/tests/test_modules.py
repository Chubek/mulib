"""Loader integration: resolution, lexical isolation, macros, PPX, and linking."""
import os
from pathlib import Path
import sys
from support import NativeTests, run

t = NativeTests()

def source(name, text):
    path = t.work / (name + '.on1x')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    return path

def execute(name, text, expected, options=(), env=None):
    path = source(name, text)
    out, exe = t.work / (name + '.c'), t.work / (name + '.exe')
    p = run([t.compiler, '--emit-c', path, '-o', out, *options], env=env)
    assert p.returncode == 0, (name, p.stderr)
    p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-I', t.root/'runtime', out, t.root/'runtime/on1x_runtime.c', *t.flags, '-o', exe])
    assert p.returncode == 0, (name, p.stderr)
    p = run([exe])
    assert p.returncode == 0 and p.stdout == expected, (name, p.stdout, p.stderr)
    return path

def reject(name, text, diagnostic, options=()):
    path = source(name, text)
    output = t.work/'preserved.c'
    output.write_text('preserve this output')
    p = run([t.compiler, '--emit-c', path, '-o', output, *options])
    assert p.returncode != 0 and diagnostic in p.stderr and not p.stdout, (name, p.stderr)
    assert output.read_text() == 'preserve this output'

source('alpha', 'fn helper(x:i64)->i64{return x+1;} fn value(x:i64)->i64{return helper(x);}')
source('beta', 'fn helper(x:i64)->i64{return x*2;} fn value(x:i64)->i64{return helper(x);}')
execute('isolated', '''import alpha, beta as b;
from alpha import (value as increment,);
fn main(){println(alpha.value(20)+b.value(10)+increment(0));}
''', '42\n')
execute('shadowing', '''from alpha import value;
fn main(){println(value(1)); {let value=fn(x:i64)->i64{return x*3;}; println(value(2));}
let (value,other)=(fn(x:i64)->i64{return x+7;},0);println(value(3));}
''', '2\n6\n10\n')
execute('prelude', '''fn main(){println(identity(42));println(identity(true));
println(fst(("first",3)));println(snd((false,9)));ignore(const(1,false));}''', '42\ntrue\nfirst\n9\n')
execute('prelude_shadow', 'fn identity(x:i64)->i64{return x+1;}fn main(){println(identity(41));}', '42\n')
reject('disabled_prelude', 'fn main(){identity(1);}', 'unknown function', ['--no-prelude'])
source('custom_prelude', 'fn answer()->i64{return 42;}')
execute('custom', 'fn main(){println(answer());}', '42\n', ['--no-prelude', '--prelude', t.work/'custom_prelude.on1x'])
source('override_prelude','fn identity(x:i64)->i64{return x+1;}')
execute('override','fn main(){println(identity(41));}','42\n',['--prelude',t.work/'override_prelude.on1x'])
execute('standard', '''import std.Int as I; from std import String; from std.numeric import Word8;
fn main(){println(I.div(-7,3));println(String.contains("On1x", "1x"));
println(Word8.basis_word8_add(255,1));}''', '-3\ntrue\n0\n')
execute('qualified', 'import std.numeric.Int; fn main(){println(std.numeric.Int.basis_int_div(8,2));}', '4\n')
source('facade','import std.numeric.Int;')
execute('dotted_reexport','import facade;fn main(){println(facade.std.numeric.Int.basis_int_div(8,2));}','4\n')
execute('repeated_selective','from std import Int;from std import Int;fn main(){println(Int.abs(-42));}','42\n')

source('pkg/__init__', 'from . import child; from .child import value as exported;')
source('pkg/child', 'from .nested.leaf import number; fn value()->i64{return number();}')
source('pkg/nested/leaf', 'from ...alpha import value; fn number()->i64{return value(41);}')
execute('packages', 'import pkg; from pkg import child as c; fn main(){println(pkg.exported());println(c.value());}', '42\n42\n')
source('namespace/leaf', 'fn number()->i64{return 42;}')
execute('namespace_package', 'from namespace import leaf;fn main(){println(leaf.number());}', '42\n')
execute('freestanding_namespace', 'from namespace import leaf;fn main(){println(leaf.number());}', '42\n',
        ['--no-prelude','--stdlib',t.work/'absent-standard-library'])
source('empty/__init__', '# Empty package initializer')
execute('empty_package', 'import empty; fn main(){}', '')

source('mac', '''import alpha as a;
macro inc {rule ($x:Expression)=>expression {a.value($x)};}
fn value(x:i64)->i64{return inc!(x);}
''')
execute('imported_macro', '''from mac import inc as next; import mac;
fn main(){println(next!(40));println(mac.value(41));
println(let_syntax {macro next {rule ($x:Expression)=>expression {$x*2};}}
 expression {next!(21)});}''', '41\n42\n42\n')
execute('staged_module', 'import alpha;fn main(){let c=quote {let value=alpha.value(40);value+1};println(run(c));}', '42\n')
execute('ppx_module', 'import alpha;fn main(){println(@pipe("apply",41,alpha.value));}', '42\n', ['--stdppx', 'pipe'])
observer=t.work/'require_fold.py'
observer.write_text('#!/usr/bin/env python3\nimport sys\ntext=sys.stdin.read()\n'
                    'if "(Binary " in text:\n print("fold must run first",file=sys.stderr);sys.exit(1)\n'
                    'sys.stdout.write(text)\n')
execute('ppx_order','fn main(){println(20+22);}','42\n',['--stdppx','fold','--ppx','observer',observer])
reject('ppx_reverse','fn main(){println(20+22);}','fold must run first',['--ppx','observer',observer,'--stdppx','fold'])
helper_output=t.work/'helper_order.c'
helper=[sys.executable,t.root/'tools/on1x-build.py',t.work/'ppx_order.on1x',
        '--on1xc',t.compiler,'--emit-only','-o',helper_output]
p=run([*helper,'--stdppx','fold','--ppx','observer',observer])
assert p.returncode == 0 and helper_output.read_text() == (t.work/'ppx_order.c').read_text(),p.stderr
helper_output.write_text('preserve this output')
p=run([*helper,'--ppx','observer',observer,'--stdppx','fold'])
assert p.returncode != 0 and 'fold must run first' in p.stderr and not p.stdout,p.stderr
assert helper_output.read_text() == 'preserve this output'

alias = t.work/'alpha_alias.on1x'
if not alias.exists(): alias.symlink_to(t.work/'alpha.on1x')
entry = execute('diamond', '''import alpha as a, alpha_alias as b; import "alpha.on1x";
fn main(){println(a.value(40)+b.value(0));}''', '42\n')
p = run([t.compiler, '--deps', entry])
assert p.returncode == 0, p.stderr
deps = p.stdout.splitlines()
assert len(deps) == len(set(deps)) and deps.count(str((t.work/'alpha.on1x').resolve())) == 1
assert deps[-1] == str(entry.resolve())
p=run([t.compiler,'--emit-c',entry]);q=run([t.compiler,'--emit-c',entry])
assert p.returncode == q.returncode == 0 and p.stdout == q.stdout

source('external/search', 'fn number()->i64{return 42;}')
execute('search_flag', 'import search;fn main(){println(search.number());}', '42\n', ['-I', t.work/'external'])
execute('search_env', 'import search;fn main(){println(search.number());}', '42\n', env={**os.environ, 'ON1X_PATH': str(t.work/'external')})
source('cycle_a', 'import cycle_b;fn a(){}')
source('cycle_b', 'import cycle_a;fn b(){}')
reject('cycle_entry', 'import cycle_a;fn main(){}', 'import cycle:')
source('link_cycle', 'import "link_cycle_alias.on1x";fn unused(){}')
alias = t.work/'link_cycle_alias.on1x'
if not alias.exists():alias.symlink_to(t.work/'link_cycle.on1x')
reject('symlink_cycle', 'import link_cycle;fn main(){}', 'import cycle:')
reject('missing', 'import not_present;fn main(){}', 'module not found: not_present')
reject('missing_member', 'from alpha import absent;fn main(){}', "has no export 'absent'")
reject('missing_qualified', 'import alpha;fn main(){alpha.absent();}', 'module has no export')
reject('alias_collision', 'import alpha as x,beta as x;fn main(){}', 'conflicting import')
reject('selective_collision', 'from alpha import value;from beta import value;fn main(){}', 'conflicting import')
reject('local_module_shadow', 'import alpha;fn main(){let alpha=1;alpha.value(2);}', 'shadowed by local')
reject('module_value', 'import alpha;fn main(){let x=alpha;}', 'not a first-class value')
reject('namespace_leak', 'import alpha;fn main(){value(2);}', 'unknown function')
reject('wildcard', 'from alpha import *;fn main(){}', 'expected identifier')
reject('empty_path', 'import "";fn main(){}', '1..4096')
source('ambiguous', 'fn unused(){}')
source('ambiguous/__init__', '')
reject('ambiguous_entry', 'import ambiguous;fn main(){}', 'ambiguous module')
source('roots/stdlib/clash','fn value()->i64{return 1;}')
source('roots/std/clash','fn value()->i64{return 2;}')
for i,statement in enumerate(['import std.clash;', 'from std import clash;']):
    reject('ambiguous_standard_'+str(i),statement+'fn main(){}','ambiguous standard module',
           ['--no-prelude','--stdlib',t.work/'roots/stdlib'])
source('raw_a', 'extern fn shared(x:i64)->i64;')
source('raw_b', 'extern fn shared(x:bool)->i64;')
reject('raw_mismatch', 'import raw_a,raw_b;fn main(){}', 'conflicting raw extern')
source('raw_tuple', 'extern fn shared((x,y):(i64,i64))->i64;')
source('raw_flat', 'extern fn shared(x:i64,y:i64)->i64;')
reject('raw_shape', 'import raw_tuple,raw_flat;fn main(){}', 'conflicting raw extern')
reject('raw_leak', 'import raw_a;fn main(){shared(1);}', 'not imported')
source('raw_same','extern fn shared(value:i64)->i64;')
execute('shared_extern','from raw_a import shared;from raw_same import shared;fn main(){}','')
reject('macro_module_shadow','import mac;fn main(){let mac=0;mac.inc!(1);}','shadowed by local')
source('source_ppx', '@ppx identity "missing.py";fn value()->i64{return 42;}')
reject('source_authorization', 'import source_ppx;fn main(){}', 'requires --allow-source-ppx')
p=run([t.compiler, '--deps', t.work/'source_authorization.on1x'])
assert p.returncode == 0, p.stderr

# The build helper links actual Basis and PPX runtime units once, without manual -I/-l flags.
path=source('linked', '''from std.collections import Array; import std.numeric.Real as R;
@timing("elapsed") fn work(){let a=Array.basis_array_array(2,21);
println(Array.basis_array_sub(a,0)*2);Array.basis_array_destroy(a);
println(R.basis_real_add("1.5","2.5"));return;}
fn main(){work();}''')
exe=t.work/'linked.exe'
p=run([sys.executable,t.root/'tools/on1x-build.py',path,'-o',exe,'--on1xc',t.compiler,
       '--cc',t.cc,'--stdppx','timing',*[f'--cflag={flag}' for flag in t.flags]])
assert p.returncode == 0,p.stderr
p=run([exe]);assert p.returncode == 0 and p.stdout == '42\n4\n' and 'on1x timing' in p.stderr,(p.stdout,p.stderr)
print('Module loader, prelude, namespaces, packages, PPX and native linking: PASS')
