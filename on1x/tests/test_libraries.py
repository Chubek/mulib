"""Compile every library module, execute Basis adapters and semantic regressions."""
import json
import os
from support import NativeTests, run

t=NativeTests()
stdlib=t.root/'stdlib'
p=run(['python3',stdlib/'tests/check_package.py']);assert p.returncode == 0,p.stderr

# Every implementation is admitted by the actual compiler, including unused definitions.
modules=[p for p in sorted(stdlib.rglob('*.on1x')) if 'tests' not in p.relative_to(stdlib).parts]
for i,module in enumerate(modules):
    path=t.source('module_'+str(i),'import '+json.dumps(str(module.resolve()))+';fn main(){}')
    p=run([t.compiler,'--check',path]);assert p.returncode == 0,(module,p.stderr)

adapters=[stdlib/'runtime/basis_native.c',stdlib/'runtime/basis_real.c','-lm']
for name,expected in [('core','basis core OK\n'),('collections','basis collections OK\n'),
                      ('real','basis real bridge OK\n'),('io','basis I/O OK\n'),('pure','OK\n'),('parsec','OK\n')]:
    exe=t.build('basis_'+name,source=stdlib/'tests'/('test_'+name+'.on1x'),extra=adapters)
    p=run([exe],cwd=t.work);assert p.returncode == 0,(name,p.stderr)
    assert p.stdout == expected,(name,p.stdout)

t.valid('modules','41\n-3\nOn1x, modules\n42\n')
t.valid('basis_edges','0123\n20\n',text='''
import std.Int as I; import std.String as S; import std.List as L;
import std.Option as O; import std.Functional as F;
fn item(i:i64)->sexpr { io_stdout(I.to_string(i));return sx_symbol(I.to_string(i)); }
fn combine(x:sexpr,acc:sexpr)->sexpr {return sx_cons(x,acc);}
fn sum(pair)->_ {let (a,b)=pair;return a+b;}
fn main(){
 assert(!I.same_sign(0,1));assert(I.same_sign(0,0));assert(!I.same_sign(-1,0));
 assert(I.rem(I.min_value(),-1)==0);assert(I.mod(I.min_value(),-1)==0);
 assert(I.to_string(I.min_value())=="-9223372036854775808");
 let(ok,v)=I.from_string("-9223372036854775808");assert(ok&&v==I.min_value());
 let(bad,unused)=I.from_string("9223372036854775808");assert(!bad);
 let xs=L.tabulate(4,item);println("");assert(sx_text(L.nth(xs,3))=="3");
 let folded=L.foldr(xs,L.nil(),combine);assert(sx_text(L.hd(folded))=="0");
 let (evens,odds)=L.partition(xs,fn(x:sexpr)->bool{return sx_text(x)=="0"||sx_text(x)=="2";});
 assert(L.length(evens)==2);assert(sx_text(L.hd(odds))=="1");
 assert(S.find("abcabc","bc",2)==4);assert(S.find("abc","",3)==3);
 assert(S.is_prefix("","abc"));assert(S.is_suffix("bc","abc"));
 let text=O.map(O.some(42),I.to_string,"");assert(O.get_or(text,"")=="42");
 assert(O.get_or(O.none(false),true));
 let ignored=O.map(O.none(0),fn(x:i64)->string{tool_fail("must not run");return "";},"fallback");
 assert(O.get_or(ignored,"absent")=="absent");
 println(F.uncurry(F.curry(sum))((9,11)));
}''')

# Independent Python arithmetic oracle for signed floor division, all sign combinations.
cases=[(a,b) for a in [-9223372036854775808,-101,-7,-1,0,1,7,101,9223372036854775807]
       for b in [-9223372036854775808,-17,-3,-1,1,3,17,9223372036854775807]
       if not (a==-9223372036854775808 and b==-1)]
text='import std.numeric.Int as I;fn main(){'+''.join(
    f'assert(I.basis_int_div({a},{b})=={a//b});assert(I.basis_int_mod({a},{b})=={a%b});' for a,b in cases)+'}'
t.valid('division_oracle','',text=text)

# Searches must not allocate per-position text snapshots. The old implementation
# exhausted the snapshot pool on this small repeated search.
t.valid('search_no_snapshots','',extra=['-DON1X_DATA_MAX_POOL=4096'],text='''
import std.String as S;
fn main(){var i=0;while i<2000{assert(!S.contains("abcdefghijklmnopqrstuvwxyz","xyz!"));i=i+1;}}
''')
for name,body,message in [
    ('word8_sub','W.basis_word8_sub(-1,0);','word8 out of range'),
    ('list_atom','L.map(sx_symbol("atom"),fn(x:sexpr)->sexpr{return x;});','list is not proper'),
    ('substring_overflow','S.substring("x",1,9223372036854775807);','substring bounds'),
]:
    exe=t.build(name,text='import std.numeric.Word8 as W;import std.List as L;import std.String as S;fn main(){'+body+'}')
    p=run([exe],env={**os.environ,'LSAN_OPTIONS':'detect_leaks=0'})
    assert p.returncode == 73 and message in p.stderr,(name,p.returncode,p.stderr)

for name in ['test_native','test_real_native','test_native_failures']:
    exe=t.work/name
    p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',stdlib/'tests'/(name+'.c'),*adapters,*t.flags,'-o',exe])
    assert p.returncode == 0,p.stderr
    p=run([exe],cwd=t.work);assert p.returncode == 0,(name,p.stderr)
for name,text,flags,message in [
 ('array_allocation','import std.collections.Array as A;fn main(){A.basis_array_array(1,0);}',
  ['-DBASIS_MALLOC(n)=((void)(n),(void*)0)'],'array allocation failed'),
 ('snapshot_budget','import std.os.OS_FileSys as FS;fn main(){var i=0;while i<10{FS.basis_fs_cwd();i=i+1;}}',
  ['-DBASIS_MAX_SNAPSHOT_BYTES=256'],'snapshot budget exceeded'),
]:
    exe=t.build(name,text=text,extra=[*adapters,*flags])
    p=run([exe]);assert p.returncode == 73 and message in p.stderr,(name,p.returncode,p.stderr)
exe=t.work/'binary_allocation_failure'
p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',t.root/'tests/test_basis_allocation.c',*t.flags,'-o',exe])
assert p.returncode == 0,p.stderr
p=run([exe,t.work/'binary_failure_input'])
assert p.returncode == 73 and 'snapshot allocation failed' in p.stderr,p.stderr
print(f'{len(modules)} compiled library modules; Basis execution and arithmetic oracle: PASS')
