#!/usr/bin/env python3
"""Independent Nerotypical checking, executable compatibility, closed rejection."""
import sys
from support import NativeTests, run
t=NativeTests()
example=t.root/'examples/type-safety.on1x'
p=run([t.compiler,'--check-types',example])
assert p.returncode==0 and 'well_typed concrete HIR' in p.stdout,p.stderr
assert p.stdout==run([t.compiler,'--check-types',example]).stdout
for name in ['type-safety','closures','tuples','polymorphism','staging','json','memory','mir','slots','f64','unsigned_f32','numeric_literals']:
    path=t.root/'examples'/(name+'.on1x')
    checked=run([t.compiler,'--emit-c','--verify-types',path])
    plain=run([t.compiler,'--emit-c',path])
    assert checked.returncode==plain.returncode==0,(name,checked.stderr,plain.stderr)
    assert checked.stdout==plain.stdout,name
    code=t.work/(name+'.c');code.write_text(checked.stdout)
    exe=t.work/name
    extra=[]
    if 'on1x_json' in checked.stdout:
        extra=[t.root/'runtime/on1x_json.c',t.root/'runtime/on1x_mulib_impl.c','-I',t.root.parent/'include','-lm']
    p=run([t.cc,'-std=c11','-Wall','-Wextra','-Werror',*t.flags,code,'-I',t.root/'runtime',t.root/'runtime/on1x_runtime.c',*extra,'-o',exe])
    assert p.returncode==0,(name,p.stderr)
    p=run([exe]);assert p.returncode==0,(name,p.stderr)
    if name=='type-safety':assert p.stdout=='42\ntrue\n42\n',p.stdout
exe=t.work/'helper'
p=run([sys.executable,t.root/'tools/on1x-build.py',example,'--verify-types','--on1xc',t.compiler,'--cc',t.cc,'-o',exe,*['--cflag='+f for f in t.flags]])
assert p.returncode==0,p.stderr
assert run([exe]).stdout=='42\ntrue\n42\n'
for name,text in [
 ('nested','fn main(){let n=40;let f=fn()->fn(i64)->i64{return fn(x:i64)->i64{return n+x;};};println(f()(2));}'),
 ('shadow','fn main(){let x=1;{let x=true;println(x);}println(x);}'),
 ('macro','macro twice{rule($x:Expression)=>expression{$x+$x};}fn main(){println(twice!(21));}'),
 ('extern','extern fn host(x:i64)->bool;fn main(){println(host(1));}'),
]:
    path=t.source(name,text)
    p=run([t.compiler,'--check-types',path]);assert p.returncode==0,(name,p.stderr)
path=t.source('budget','fn main(){'+'println(1);'*2200+'}')
out=t.work/'budget.c';out.write_text('keep')
p=run([t.compiler,'--emit-c','--verify-types',path,'-o',out])
assert p.returncode==75 and 'ON1X-TYPE-UNKNOWN' in p.stderr and out.read_text()=='keep',p.stderr
# Source errors still reject through ordinary inference; explicit universals
# have a separate admission boundary until quantifier-aware checking is added.
for name,text in [
 ('mismatch','fn main(){let x:i64=true;}'),
 ('forall','fn use(f:forall(a)fn(a)->a){println(f(42));println(f(true));}fn main(){use(fn(x)->_{return x;});}'),
]:
    path=t.source(name,text);out=t.work/'sentinel.c';out.write_text('keep')
    p=run([t.compiler,'--emit-c','--verify-types',path,'-o',out])
    assert p.returncode==1 and out.read_text()=='keep',(name,p.stderr)
for mode in ['--tokens','--emit-ast']:
    p=run([t.compiler,mode,'--verify-types',example]);assert p.returncode==2,p.stderr
owning=t.source(
    'unsupported_unique_type_safety',
    'enum N{Z}fn main(){let n=box(variant<N>(Z,record{}));drop(n);}')
p=run([t.compiler,'--check-types',owning])
assert p.returncode==1 and \
       'unique ownership is outside the independent structural adapter' in p.stderr, \
       p.stderr
print('Nerotypical checked compilation, execution, helper and output preservation PASS')
