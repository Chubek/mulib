#!/usr/bin/env python3
"""Intrinsic statement loops: actual C11, MIR differential and closed boundaries."""
import random
import os
from pathlib import Path
from support import NativeTests, run

t=NativeTests()

def execute(name,source,expected,options=(),mode='--emit-c',extra=(),env=None):
    path=t.source(name,source);out=t.work/(name+'.c');exe=t.work/name
    p=run([t.compiler,mode,path,*options,'-o',out],env=env);assert p.returncode==0,(name,p.stderr)
    p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I',t.root/'runtime',out,
           t.root/'runtime/on1x_runtime.c',*extra,*t.flags,'-o',exe]);assert p.returncode==0,(name,p.stderr)
    p=run([exe]);assert p.returncode==0 and p.stdout==expected,(name,p.stdout,p.stderr)
    return path

def differential(name,source,expected):
    path=execute(name,source,expected)
    execute(name+'_mir',source,expected,['--optimize-ir'],mode='--emit-ir-c')
    p=run([t.compiler,'--run-ir',path]);assert p.returncode==0 and p.stdout==expected,(name,p.stdout,p.stderr)
    ir=t.work/(name+'.mir');p=run([t.compiler,'--emit-ir',path,'-o',ir]);assert p.returncode==0,p.stderr
    p=run([t.compiler,'--emit-ir','--ir-input',ir]);assert p.returncode==0 and p.stdout==ir.read_text(),p.stderr
    return path

def rejected(name,text,diagnostic,options=()):
    path=t.source(name,text);out=t.work/'preserved.c';out.write_text('preserved')
    p=run([t.compiler,'--emit-c',path,*options,'-o',out])
    assert p.returncode and diagnostic in p.stderr and not p.stdout and out.read_text()=='preserved',(name,p.stderr)

def finite(name,source,expected):
    path=execute(name,source,expected)
    p=run([t.compiler,'--run-vm',path]);assert p.returncode==0 and p.stdout==expected,(name,p)
    for tag,opts in [('plain',[]),('opt',['--optimize-ir'])]:
        artifact=t.work/(name+'_'+tag+'.o1vm')
        p=run([t.compiler,'--emit-vm',path,*opts,'-o',artifact]);assert p.returncode==0,(name,p)
        runner=t.compiler.with_name('on1x-vm')
        p=run([runner,'--verify',artifact]);assert p.returncode==0,(name,p)
        p=run([runner,artifact]);assert p.returncode==0 and p.stdout==expected and not p.stderr,(name,p)
    return path

example=(t.root/'examples/statement_loops.on1x').read_text()
path=differential('example',example,'8\n3\n9223372036854775807\n')
execute('optimized_c',example,'8\n3\n9223372036854775807\n',extra=['-O2'])
for mode in ['--check-types','--check-memory']:
    p=run([t.compiler,mode,path]);assert p.returncode==0 or (p.returncode==2 and 'unavailable' in p.stderr),p.stderr
    if p.returncode:print(mode+': UNSUPPORTED (optional SDK not configured)')

differential('ranges','''
fn bound(mark:i64,x:i64)->i64{println(mark);return x;}
fn main(){var count=0;
 for i in bound(1,-2)..bound(2,2){count=count+i;}println(count);
 for i in 5..5{assert(false);}for i in 5..=4{assert(false);}
 for i in 9223372036854775806..9223372036854775807{println(i);}
 for i in -9223372036854775808..=-9223372036854775808{println(i);continue;}
 var high=3;for i in 0..high{high=0;count=count+1;}println(count);
 let i=99;for i in i..=99{println(i);}println(i);
}''','1\n2\n-2\n9223372036854775806\n-9223372036854775808\n1\n99\n99\n')

differential('latches','''
fn step(i:i64)->i64{println(10+i);return i+1;}
fn main(){var hits=0;
 outer:for(var i=0;i<3;i=step(i)){
  for(var j=0;j<2;j=j+1){if j==0{hits=hits+1;continue outer;}assert(false);}
  assert(false);
 }
 println(hits);
 stop:for(var i=0;i<3;i=step(i)){loop{break stop;}}
 for(println(40);false;println(41)){assert(false);}
 var k=0;for(;k<2;){k=k+1;}println(k);
 for(;;){break;}
}''','10\n11\n12\n3\n40\n2\n')

differential('lexical','''
fn main(){var n=0;let label=100;
 label:while n<4{n=n+1;if n<3{continue label;}if n==3{break label;}}
 println(n);println(label);
 for x in 0..3{let x=42;println(x);}
 with:loop{break with;}
 var n2=0;with:while n2<2{n2=n2+1;continue with;}assert(n2==2);
}''','3\n100\n42\n42\n42\n')

differential('returns','''
fn early(x:i64)->i64{for(var i=0;i<5;i=i+1){if i==x{return i;}}return -1;}
fn main(){println(early(2));println(early(8));}''','2\n-1\n')

finite_example=(t.root/'examples/finite_iteration.on1x').read_text()
finite_path=finite('finite_example',finite_example,'100\n30\n9\n')
execute('finite_ppx',finite_example,'100\n30\n9\n',['--stdppx','identity'],extra=['-O2'])
for mode in ('--emit-ir','--emit-ir-c','--run-ir'):
    sentinel=t.work/'preserved.mir'
    sentinel.write_text('unchanged')
    p=run([t.compiler,mode,finite_path,'-o',sentinel])
    assert p.returncode==1 and 'ON1X-MIR-UNSUPPORTED: scalar MIR cannot lower array' in p.stderr,(mode,p)
    assert sentinel.read_text()=='unchanged' and not p.stdout,(mode,p)

finite('finite_scope','''
fn empty(mark:i64)->array<i64,0>{println(mark);let result:array<i64,0>=array[];return result;}
fn rows(mark:i64)->array<i64,3>{println(mark);return array[2,4,8];}
fn pick(a:array<i64,3>)->i64{
  for n in a { if n>2 {return n;} } return 0;
}
fn main(){
  var x=rows(10);
  let _=12;
  var n=0;
  for x in x {
    if x==2 {continue;}
    n=n+x;
    let x=100;n=n+x;
  }
  for _ in empty(20){assert(false);}
  for _ in rows(30)[1:1]{assert(false);}
  for _ in rows(40)[1:3]{n=n+_;}
  for (_,(rest,_)) in array[(17,(19,21))]{assert(_==12&&rest==19);}
  for _ in 0..1{println(_);}
  println(n);println(pick(x));
}''','10\n20\n30\n40\n0\n236\n4\n')

all_indices=','.join(map(str,range(256)))
finite('finite_extent_limit',f'''
fn main(){{
 let a=array[{all_indices}];
 var visited=0;var last=-1;
 for x in a{{visited=visited+1;last=x;}}
 println(visited);println(last);
}}''','256\n255\n')

finite('finite_hygiene','''
macro wrap{rule($values:Expression,$b:Block)=>block{
  inner:for (x,_) in $values {$b;break inner;}
};}
fn main(){
  let x=42;
  outer:for n in array[1,2,3]{
    wrap!(array[(n,8)],{println(x+n);if n<3{continue outer;}});
    println(n);
  }
  wrap!(array[(5,6)],{println(x);});
}''','43\n44\n45\n3\n42\n')
execute('finite_staged','''
fn main(){
 let code=quote{var total=0;for (x,y) in array[(1,2),(3,4)]{total=total+x*y;}total};
 println(run(code));
}''','14\n')
execute('finite_generic','''
fn visit<T>(items:array<T,2>, f:fn(T)->i64)->i64 {
 var result=0;for n in items {result=result+f(n);}return result;
}
fn main(){println(visit(array[1,2],fn(x:i64)->i64{return x;}));
          println(visit(array[true,false],fn(x:bool)->i64{if x{return 8;}return 9;}));}
''','3\n17\n')
execute('finite_borrow_copy','''
fn main(){
 var a=array[1,2];let r=borrow mut a;
 for x in r {r:=array[9,9];println(x);}
 println(r[0]);
}''','1\n2\n9\n')
execute('finite_unsafe_block', '''
fn main(){
 let a=array[2,3];
 let result=unsafe {var total=0;for x in a{total=total+x;}total};
 println(result);
}''','5\n',['--kernel-c11'])

trap_source=t.source('finite_subject_trap','''
fn bound(x:i64)->i64{println(x);return x;}
fn main(){
 let a=array[1,2,3];
 for _ in a[bound(1):bound(4)]{println(99);}
}
''')
trap_native=run([t.build('finite_subject_trap',source=trap_source)])
assert (trap_native.returncode,trap_native.stdout)==(70,'1\n4\n'),trap_native
assert f'{trap_source}:5:' in trap_native.stderr and 'slice bounds out of range' in trap_native.stderr,trap_native
trap_guest=run([t.compiler,'--run-vm',trap_source])
assert (trap_guest.returncode,trap_guest.stdout)==(70,'1\n4\n'),trap_guest
assert f'{trap_source}:5:' in trap_guest.stderr and 'assertion failed' in trap_guest.stderr,trap_guest

# Historical ordinary identifier programs remain valid, including discard
# expression statements. A labeled exit is unambiguous even when shadowed.
execute('identifier_compat','''
fn loop()->i64{return 1;}fn for(x:i64)->i64{return x;}
fn break()->i64{return 4;}
fn main(){println(loop());println(for(2));break;
 let continue=3;continue;let loop=5;println(loop);
 named:loop{let break=42;break;continue;break named;}
}''','1\n2\n5\n')
t.source('loop_module','''fn break()->i64{return 4;}
fn count()->i64{break;var n=0;for i in 0..3{n=n+i;}return n;}''')
execute('imported_loops','import loop_module as M;fn main(){println(M.count());}','3\n')

# Syntax contexts are a separate namespace for labels. A captured block's bare
# exit must not be captured by a template-introduced loop of the same spelling.
hygiene='''
macro wrap{rule($b:Block)=>block{outer:loop{$b;break outer;}};}
macro local{rule()=>block{inner:loop{break inner;}};}
fn main(){var x=0;outer:loop{x=x+1;wrap!({if x==1{break outer;}});x=100;}println(x);
 var y=0;loop{y=y+1;wrap!({if y==1{break;}});y=100;}println(y);local!();local!();
}'''
differential('hygiene',hygiene,'1\n1\n')
execute('hygiene_ppx',hygiene,'1\n1\n',['--stdppx','identity'])

execute('staged','''
fn main(){let c=quote{(fn()->i64{var total=0;for i in 0..3{total=total+i;}return total;})()};println(run(c));}
''','3\n')
execute('generic','''
fn action(f){for i in 0..2{f(i);}}
fn main(){action(fn(x:i64){println(x);});action(fn(x:i64){println(x+10);});}
''','0\n1\n10\n11\n')
execute('generic_specializations','''
fn apply(f)->_{for i in 0..2{f(i);}return f(2);}
fn main(){println(apply(fn(x:i64)->i64{return x+1;}));println(apply(fn(x:i64)->bool{return x==2;}));}
''','3\ntrue\n')

# Existing closure allocator catches skipped scope releases even without LSan.
extra=['-include',t.root/'tests/fixtures/closure_allocator.h',t.root/'tests/fixtures/closure_allocator.c']
execute('finite_cleanup','''
extern fn live_environments()->i64;
fn make(x:i64)->fn()->i64{return fn()->i64{return x;};}
fn early(data:array<i64,2>)->i64{
 for x in data {let c=make(x);if x>0{return c();}}
 return 0;
}
fn main(){
 outer:for x in array[1,2,3]{
  assert(live_environments()==0);
  let f=make(x);
  for y in array[4,5]{
   let g=make(y);
   assert(live_environments()==2);
   println(f()+g());
   if x==1 {continue outer;}
   if x==2 {break outer;}
  }
  assert(false);
 }
 assert(live_environments()==0);
 println(early(array[0,8]));assert(live_environments()==0);
}''','5\n6\n8\n',extra=extra)
execute('cleanup','''
extern fn live_environments()->i64;
fn make(x:i64)->fn()->i64{return fn()->i64{return x;};}
fn early()->i64{for(let owned=make(42);true;assert(false)){let inner=make(9);return owned()+inner();}return 0;}
fn main(){
 for i in 0..100{let f=make(i);let pair=(f,make(i+1));if i<99{continue;}break;}
 assert(live_environments()==0);
 outer:for(let init=make(1);true;assert(false)){
  let body=make(2);loop{let nested=make(3);assert(live_environments()==3);break outer;}
 }
 assert(live_environments()==0);println(early());assert(live_environments()==0);
}
''','51\n',extra=extra)

if (t.root.parent/'kernels/mulib-json.h').exists():
    json_source='''fn main(){outer:for i in 0..=2{
      let root=json_parse_or_die("{\\"items\\":[1,2]}");let view=json_get_or_die(root,"items");
      if i==0{continue;}loop{let nested=json_parse_or_die("{}");println(json_size(view));break outer;}
    }}'''
    json_path=execute('json_exits',json_source,'2\n',extra=['-I',t.root.parent/'kernels',
                      t.root/'runtime/on1x_json.c',t.root/'runtime/on1x_mulib_impl.c','-lm'])
    p=run([t.compiler,'--check-memory',json_path])
    assert p.returncode==0 or (p.returncode==2 and 'unavailable' in p.stderr),p.stderr

overflow='fn main(){for(var i=9223372036854775807;true;i=i+1){println(1);continue;}}'
exe=t.build('step_overflow',text=overflow);p=run([exe]);assert p.returncode==70 and p.stdout=='1\n' and 'overflow' in p.stderr,p.stderr
p=run([t.compiler,'--run-ir',t.work/'step_overflow.on1x']);assert p.returncode==70 and p.stdout=='1\n' and 'overflow' in p.stderr,p.stderr

# Identity transforms reconstruct the new source forms without carrying trusted
# target IDs; targets are recomputed only after all source transforms/staging.
for name,options in [('python',['--stdppx','identity']),('native',['--ppx','identity',t.compiler.parent/'stdppx/on1x-ppx-identity'])]:
    if name=='native' and not Path(options[-1]).exists():continue
    execute('ppx_'+name,example,'8\n3\n9223372036854775807\n',options,env={**os.environ,'ON1X_PPX_SCHEMA':str(t.root/'grammar/PPX.schema.json')})
p=run([t.compiler,'--emit-ast',path]);assert p.returncode==0 and 'RangeLoop' in p.stdout and 'ControlWhile' in p.stdout and 'LoopExit' in p.stdout,p.stderr

negative=[
 ('break_outside','fn main(){break;}','no lexical target'),
 ('continue_outside','fn main(){continue;}','no lexical target'),
 ('missing_label','fn main(){loop{break missing;}}','no lexical target'),
 ('duplicate_label','fn main(){a:loop{a:loop{break;}}}','duplicate active loop label'),
 ('boundary','fn main(){a:loop{let f=fn(){break a;};break;}}','no lexical target'),
 ('template_escape','macro b{rule()=>block{break;};}fn main(){loop{b!();break;}}','no lexical target'),
 ('after_break','fn main(){loop{break;println(1);}}','unreachable'),
 ('after_continue','fn main(){loop{continue;println(1);}}','unreachable'),
 ('binder_escape','fn main(){for i in 0..1{}println(i);}','undefined'),
 ('init_escape','fn main(){for(var i=0;i<1;i=i+1){}println(i);}','undefined'),
 ('body_in_step','fn main(){for(var i=0;i<1;i=k){let k=1;}}','undefined'),
 ('immutable_binder','fn main(){for i in 0..3{i=i+1;}}','immutable'),
 ('float_range','fn main(){for i in 0.0..1.0{}}','expected i64'),
 ('unsigned_range','fn main(){for i in 0u8..1u8{}}','expected i64'),
 ('bool_condition','fn main(){for(;1;){} }','expected bool'),
 ('finite_scalar','fn main(){for i in 3{}}','for-in requires a known fixed array or snapshot slice'),
 ('finite_tuple_source','fn main(){for i in (1,2){}}','for-in requires a known fixed array or snapshot slice'),
 ('finite_sexpr','fn walk(xs:sexpr){for i in xs{}} fn main(){}','for-in requires a known fixed array or snapshot slice'),
 ('finite_nominal_list','enum List{Nil,Cons(i64,own<List>)} fn walk(xs:own<List>){for i in xs{}} fn main(){}','for-in requires a known fixed array or snapshot slice'),
 ('finite_borrow','fn main(){var a=array[1];for i in borrow mut a{}}','iteration snapshot must be a value'),
 ('finite_missing_shape','fn f(xs){for i in xs{}} fn main(){f(array[1]);}','for-in requires a known fixed array or snapshot slice'),
 ('finite_immutable','fn main(){for i in array[1]{i=2;}}','immutable'),
 ('finite_scope_escape','fn main(){for i in array[1]{}println(i);}','undefined'),
 ('finite_tuple_type','fn main(){for (x,y) in array[1]{}}','tuple binding'),
 ('finite_duplicate','fn main(){for (x,x) in array[(1,2)]{}}','duplicate local'),
 ('finite_refutable','fn main(){for (x,(1,y)) in array[(2,(1,2))]{}}','expected an irrefutable name'),
 ('finite_refutable_array','fn main(){for [x,y] in array[array[1,2]]{}}','expected an irrefutable name'),
 ('finite_cons','fn main(){for x::xs in array[1]{}}','list/cons patterns are unsupported'),
 ('finite_bad_delimiter','fn main(){for (x,y) in array[(1,2)] println(x);}','expected \'{\''),
 ('finite_range_pattern','fn main(){for (x,y) in 0..3{}}','i64 ranges require a single named binder'),
 ('value_break','fn main(){loop{break with 1;}}','expression loop target'),
 ('label_value_break','fn main(){a:loop{break a with 1;}}','expression loop target'),
 ('cleanup_resource_unsupported','fn main(){loop{let r=json_parse_or_die("{}");defer json_print(r);break;}}',"defer cannot retain JSON"),
 ('async_unsupported','async fn main(){}',"expected 'fn'"),
]
for name,source,error in negative:rejected(name,source,error)
probe=run([t.compiler,'--check',t.source('finite_regex_probe','fn main(){let r=/(a)/;}')])
if probe.returncode:
    assert 'ON1X-REGEX-SDK' in probe.stderr,probe
    print('finite_regex: UNSUPPORTED (optional Firestone regex compiler not configured)')
else:
    rejected('finite_regex','fn main(){for x in (capture /(a)/ ~ "a"){}}',
             'for-in requires a known fixed array or snapshot slice')
rejected('depth','fn main(){'+'loop{'*300+'break;'+'}'*301,'limit')

# Deterministic numeric oracle exercises all exits and empty/reversed intervals.
rng=random.Random(1729)
for case in range(8):
    low=rng.randrange(-8,4);high=rng.randrange(-3,9);inclusive=bool(case%2)
    stop=rng.randrange(1,7);values=[]
    for i in range(low,high+int(inclusive)):
        if i==stop:break
        if i%2==0:continue
        values.append(i)
    text=f'fn main(){{var sum=0;for i in {low}{"..=" if inclusive else ".."}{high}{{if i=={stop}{{break;}}if i%2==0{{continue;}}sum=sum+i;}}println(sum);}}'
    differential('oracle_'+str(case),text,str(sum(values))+'\n')

print('Intrinsic statement loops: source/HIR/MIR/C11, hygiene, cleanup and negative profiles PASS')
