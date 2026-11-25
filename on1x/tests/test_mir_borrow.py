#!/usr/bin/env python3
"""Copy-value lexical borrows: C11, MIR evaluator/C11, VM wire and Archon parity."""
from support import NativeTests, run

t = NativeTests()
vm = t.compiler.parent / 'on1x-vm'
archon = t.compiler.parent / 'backends/mir-archon/on1x-archon-mir'
abi = archon.with_name('libon1x_archon_scalar_abi.a')
native = archon.is_file() and abi.is_file()


def command(args):
    p = run(args)
    assert p.returncode == 0, (list(map(str, args)), p.stdout, p.stderr)
    return p


def exercise(name, text, trace, *, status=0, trap=None):
    source = t.source(name, text)
    outputs = {'source-c11': run([t.build(name + '-reference', source=source)])}
    for option in ([], ['--optimize-ir']):
        variant = 'opt' if option else 'plain'
        mir = t.work / f'{name}-{variant}.mir'
        command([t.compiler, '--emit-ir', *option, source, '-o', mir])
        roundtrip = command([t.compiler, '--ir-input', '--emit-ir', mir])
        assert roundtrip.stdout == mir.read_text(encoding='utf-8'), (name, variant, roundtrip)
        outputs['eval-' + variant] = run([t.compiler, '--ir-input', '--run-ir', mir])
        c = t.work / f'{name}-{variant}.c'
        exe = c.with_suffix('')
        command([t.compiler, '--ir-input', '--emit-ir-c', mir, '-o', c])
        command([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                 '-I', t.root / 'runtime', c, t.root / 'runtime/on1x_runtime.c',
                 *t.flags, '-lm', '-o', exe])
        outputs['mir-c11-' + variant] = run([exe])

        bytecode = mir.with_suffix('.o1vm')
        command([t.compiler, '--ir-input', '--emit-vm', mir, '-o', bytecode])
        outputs['vm-wire-' + variant] = run([vm, bytecode])
        if native:
            asm = mir.with_suffix('.s')
            executable = asm.with_suffix('.native')
            command([archon, '--ir-input', mir, '-o', asm])
            command([t.cc, '-I', t.root / 'runtime', asm, abi,
                     t.root / 'runtime/on1x_runtime.c', *t.flags, '-lm', '-o', executable])
            outputs['archon-' + variant] = run([executable])
    outputs['source-vm'] = run([t.compiler, '--run-vm', source])
    for profile, p in outputs.items():
        assert (p.returncode, p.stdout) == (status, trace), (name, profile, p)
        assert (trap in p.stderr) if trap else not p.stderr, (name, profile, p.stderr)


exercise('alias_identity', '''
fn mark(n:i64)->i64 { println(n);return n; }
fn main() {
 let x=mark(4);
 { let a=borrow x; let b=borrow a; println(b); println(x); }
 {
  let x=borrow mut x;
  { let s=borrow x; let child=borrow s; println(child); }
  x:=x+1;
  { let b=borrow mut x; let c=borrow mut b; c:=mark(c+2); }
  { let x=50; let s=borrow x; println(s); }
  println(x);
 }
 println(x);
}
''', '4\n4\n4\n4\n7\n50\n7\n7\n')

exercise('branches_and_latches', '''
fn change(x:i64, choose:bool)->i64 {
 let n=1;
 let keep=100;
 for(var i=0;i<3;i=i+1) {
  let r=borrow mut x;
  if i==1 { let child=borrow mut r; child:=child+n; continue; }
  r:=r+2;
 }
 if choose { let edit=borrow mut n; edit:=4; }
 else { let read=borrow n; println(read); }
 return x+n+keep;
}
fn main() { println(change(10,true)); println(change(10,false)); }
''', '119\n1\n116\n')

exercise('specialized_parameter', '''
fn swap_copy<T>(x:T, next:T)->T {
 let r=borrow mut x;
 r:=next;
 return r;
}
fn main(){println(swap_copy(1,2));println(swap_copy(false,true));}
''', '2\ntrue\n')

exercise('numeric_slots', '''
fn main() {
 let flag=false;
 { let edit=borrow mut flag; edit:=!edit; }
 println(flag);
 let f=2.0f32;
 { let a=borrow mut f; let b=borrow mut a; b:=3.25f32; }
 assert(f==3.25f32);
 let g=1.0;
 { let a=borrow mut g; a:=a+1.0; }
 assert(g==2.0);
 let u=1u64;
 { let a=borrow mut u; a:=a+2u64; }
 assert(u==3u64);
 let small=12u16;
 { let a=borrow mut small; a:=a+2u16; }
 assert(small==14u16);
}
''', 'true\n')

exercise('order_and_nonlocal_return', '''
fn mark(n:i64)->i64 { println(n);return n; }
fn pair(a:i64,b:i64) { println(a);println(b); }
fn change(early:bool)->i64 {
 let x=3;
 let r=borrow mut x;
 pair(r,loop{let child=borrow mut r; child:=mark(5); break with child;});
 r:=loop{if early{return r;} let r=7;break with mark(r);};
 return r;
}
fn main()->i64 { println(change(true)); println(change(false)); return 70; }
''', '5\n3\n5\n5\n5\n3\n5\n7\n7\n', status=70)

exercise('rhs_trap', '''
fn mark(n:i64)->i64 { println(n);return n; }
fn main() {
 let x=4;
 let r=borrow mut x;
 r:=mark(r)/mark(0)+mark(99);
 println(r);
}
''', '4\n0\n', status=70, trap='invalid division')

exercise('skipped_store', '''
fn mark(n:i64)->i64 { println(n);return n; }
fn main() {
 var x=7;
 outer:for(var i=0;i<2;i=i+1) {
  let r=borrow mut x;
  let s=borrow mut r;
  println(s);
  s:=loop{if i==0{continue outer;}break with mark(21);};
  println(s);
 }
 println(x);
}
''', '7\n7\n21\n21\n21\n')

exercise('exchange_continue', '''
fn mark(n:i64)->i64 { println(n);return n; }
fn main() {
 let x=7;
 outer:for(var i=0;i<2;i=i+1) {
  let r=borrow mut x;
  let s=borrow mut r;
  let saved=ref_replace(s,loop{if i==0{continue outer;}break with mark(21);});
  println(saved);
  println(s);
 }
 println(x);
}
''', '21\n7\n21\n21\n')

exercise('exchange_return', '''
fn mark(n:i64)->i64 { println(n);return n; }
fn work(early:bool)->i64 {
 let x=19;
 let r=borrow mut x;
 let old=ref_replace(r,loop{
  if early{return mark(9);}
  let r=31;
  break with mark(r);
 });
 println(r);
 return old;
}
fn main(){println(work(true));println(work(false));}
''', '9\n9\n31\n31\n19\n')

exercise('exchange_trap', '''
fn mark(n:i64)->i64 { println(n);return n; }
fn main() {
 let x=4;
 let r=borrow mut x;
 println(ref_replace(r,mark(1)+5/mark(0)+mark(99)));
}
''', '1\n0\n', status=70, trap='invalid division')

exercise('tuple_loan', '''
fn mark(n:i64)->i64 { println(n); return n; }
fn swap(root:(i64,(bool,i64)), early:bool)->i64 {
  let edit=borrow mut root;
  let next=borrow mut edit;
  let old=ref_replace(next,loop{
    if early{return mark(9);}
    break with (mark(5),(true,mark(7)));
  });
  let (a,(flag,b))=old;
  let (c,(after,d))=next;
  println(flag); println(after);
  return a+b+c+d;
}
fn main(){println(swap((1,(false,2)),true));println(swap((1,(false,2)),false));}
''', '9\n9\n5\n7\nfalse\ntrue\n15\n')

exercise('tuple_loan_trap', '''
fn mark(n:i64)->i64 { println(n); return n; }
fn main() {
  let root=(1,(2,false));
  let edit=borrow mut root;
  let old=ref_replace(edit,(mark(4)/mark(0),(mark(99),true)));
  let (x,(y,b))=old;
  println(x+y);
  println(b);
}
''', '4\n0\n', status=70, trap='invalid division')

# Exactly one root slot, two stores (initial value and :=), three reads.
# Acquiring or reborrowing doesn't allocate a slot, perform a load, or bump
# the wire version of an otherwise v1 program.
source = t.source('slot_shape', '''
fn main(){
 let x=4;
 { let r=borrow mut x; let s=borrow mut r; s:=s+1; println(s); }
 println(x);
}
''')
ir = command([t.compiler, '--emit-ir', source]).stdout
assert ir == command([t.compiler, '--emit-ir', source]).stdout
assert ir.startswith('(on1x.mir 1 ') and ir.count('(value ') == 1, ir
assert ir.count(' store void ') == 2 and ir.count(' load i64 ') == 3, ir

for name, text, diagnostic, checked_hir in [
    ('borrow_parameter', 'fn peek(x:borrow i64)->i64{return x;}fn main(){}',
     'borrowed parameters', True),
    ('borrow_call', 'fn set(x:ref i64){x:=2;} fn main(){let x=1;set(x);}',
     'borrowed parameters', True),
    ('aggregate', 'fn main(){let xs=array[1,2];let r=borrow xs;println(r[0]);}',
     '', True),
    ('managed_tuple', 'fn main(){let x=(1,(2,sx_string("text")));let r=borrow x;'
     'let (a,(b,c))=r;println(a+b);println(sx_text(c));}', '', True),
    ('function_tuple', 'fn id(n:i64)->i64{return n;}'
     'fn main(){let x=(1,(2,id));let r=borrow x;let (a,(b,f))=r;println(f(a+b));}', '', True),
    ('pattern_root', 'fn main(){let (x,y)=(1,2);let r=borrow x;println(r+y);}',
      'verified named copy root', True),
    ('function_layout', 'fn id(n:i64)->i64{return n;}'
     'fn main(){let x=id;let r=borrow x;println(r(3));}',
     '', True),
    ('unique', 'record Cell{n:i64} fn main(){let own=box(record<Cell>{n:3});'
     '{let r=borrow own;println(r["n"]);} drop(own);}', '', True),
    ('rc', 'fn main(){let x=sx_string("text");let r=borrow x;println(sx_text(r));}',
     '', True),
    ('managed_ref_replace', 'fn main(){let x=sx_string("text");let r=borrow mut x;'
     'println(sx_text(ref_replace(r,sx_string("next"))));}', '', True),
    ('owner_ref_replace', 'record Cell{n:i64} fn main(){let x=box(record<Cell>{n:1});'
     '{let r=borrow mut x;println(ref_replace(r,record<Cell>{n:2})["n"]);} drop(x);}',
     '', True),
    ('ref_replace_conflict', 'fn main(){let x=1;let r=borrow mut x;'
     'println(ref_replace(r,r+2));}', 'during its exclusive borrow', False),
    ('defer', 'fn main(){let x=1;let r=borrow x;println(r);defer println(2);}',
     '', True),
    ('managed_result', 'enum Result<T,E>{Ok(T),Err(E)}'
     'fn use(n:Result<sexpr,i64>)->Result<sexpr,i64>{let x=n?;'
     'return variant<Result<sexpr,i64>>(Ok,x);} fn main(){}', '', True),
    ('shared_store', 'fn main(){let x=1;let r=borrow x;r:=2;}',
     'exclusive', False),
    ('suspended_parent', 'fn main(){let x=1;let r=borrow mut x;'
     'let s=borrow mut r;r:=2;println(s);}', 'during its exclusive borrow', False),
]:
    source = t.source(name, text)
    if checked_hir:
        command([t.compiler, '--check', source])
    for mode in ('--emit-ir', '--emit-ir-c', '--emit-vm'):
        output = t.work / 'must_not_be_written'
        args = [t.compiler, mode, source, '-o', output]
        p = run(args)
        assert p.returncode == 1 and diagnostic in p.stderr and not output.exists(), (name, mode, p)
        assert source.name + ':' in p.stderr, p
        if checked_hir:
            assert 'ON1X-MIR-UNSUPPORTED' in p.stderr, p
        if mode == '--emit-vm' and name in ('aggregate', 'function_layout', 'function_tuple'):
            assert 'verified named copy root' in p.stderr, p
        # Rejecting a new artifact must also leave a previous one untouched.
        previous = t.work / f'{name}{mode}-previous-artifact'
        sentinel = b'previous artifact\x00\xff\n'
        previous.write_bytes(sentinel)
        rejected = run([*args[:-1], previous])
        assert (rejected.returncode, rejected.stderr) == (1, p.stderr), (name, mode, rejected)
        assert previous.is_file() and previous.read_bytes() == sentinel, (name, mode, previous)

# Existing VM-only unmanaged Result layout can propagate an error through
# the RHS of a scalar loan store. This does not admit managed Result payloads
# or make nominal Results part of the ordinary MIR route.
source = t.source('unmanaged_result_vm', '''
enum Result<T,E>{Ok(T),Err(E)}
fn value(ok:bool)->Result<i64,u8>{
 if ok{return variant<Result<i64,u8>>(Ok,9);}
 return variant<Result<i64,u8>>(Err,2u8);
}
fn work(ok:bool)->Result<i64,u8>{
 let x=1;
 {let r=borrow mut x; r:=value(ok)?;}
 println(x);
 return variant<Result<i64,u8>>(Ok,x);
}
fn show(res:Result<i64,u8>){match res with {Ok(n)=>println(n),Err(e)=>println(e)};}
fn main(){show(work(false));show(work(true));}
''')
p = run([t.compiler, '--emit-ir', source])
assert p.returncode == 1 and 'ON1X-MIR-UNSUPPORTED' in p.stderr and not p.stdout, p
bytecode = t.work / 'unmanaged_result_vm.o1vm'
command([t.compiler, '--emit-vm', source, '-o', bytecode])
for executable in ([t.build('unmanaged_result_c11', source=source)], [vm, bytecode]):
    p = command(executable)
    assert p.stdout == '2\n9\n9\n' and not p.stderr, p

# VM lifts this lambda into a fresh HIR function and must prove loans against
# that exact tree rather than retain the original HIR provenance.
source = t.source('vm_lifted_local_borrow', '''
fn main(){
 let do_swap=fn(x:i64)->i64{
  let r=borrow mut x;
  r:=r+5;
  return r;
 };
 println(do_swap(8));
}
''')
bytecode = t.work / 'vm_lifted_local_borrow.o1vm'
command([t.compiler, '--emit-vm', source, '-o', bytecode])
for executable in ([t.build('vm_lifted_local_borrow_c11', source=source)], [vm, bytecode]):
    p = command(executable)
    assert p.stdout == '13\n' and not p.stderr, p

source = t.source('vm_captured_tuple_loan', '''
fn main() {
  let root=(4,(false,2));
  let go=fn()->i64 {
    let captured=root;
    let next=borrow mut captured;
    let (a,(flag,b))=ref_replace(next,(1,(true,2)));
    println(flag);
    return a+b;
  };
  { let edit=borrow mut root; edit:=(99,(true,9)); }
  println(go());
}
''')
bytecode = t.work / 'vm_captured_tuple_loan.o1vm'
command([t.compiler, '--emit-vm', source, '-o', bytecode])
for executable in ([t.build('vm_captured_tuple_loan_c11', source=source)], [vm, bytecode]):
    p = command(executable)
    assert p.stdout == 'false\n6\n' and not p.stderr, p

print(f'copy-value lexical borrow differential PASS; Archon={native}')
