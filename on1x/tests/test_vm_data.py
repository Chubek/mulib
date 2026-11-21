#!/usr/bin/env python3
"""VM-only value-layout lowering: compare against independently emitted C11."""
from support import NativeTests, run

t = NativeTests()
runner = t.compiler.with_name("on1x-vm")


def check(name, src, stdout, status=0):
    source = t.source(name, src)
    reference = run([t.build(name, source=source)])
    assert (reference.returncode, reference.stdout) == (status, stdout), reference
    for flags in ([], ["--optimize-ir"]):
        image = t.work / (name + ("-opt" if flags else "") + ".o1vm")
        emit = run([t.compiler, "--emit-vm", source, *flags, "-o", image])
        assert emit.returncode == 0, (name, emit.stderr)
        assert image.read_bytes().startswith(b"O1VM")
        verified = run([runner, "--verify", image])
        assert verified.returncode == 0, verified.stderr
        vm = run([runner, image])
        assert (vm.returncode, vm.stdout) == (status, stdout), (name, vm)
        if status:
            assert source.name in vm.stderr, vm.stderr


check("variant", """
fn mark(i:i64)->i64{println(i);return i;}
fn pick(v:sum{No:record{},Yes:record{a:i64,b:i64}})->i64{
 return match v with {No(_)=>0,Yes(p)=>p["a"]-p["b"]};
}
fn main(){
 println(pick(variant<sum{No:record{},Yes:record{a:i64,b:i64}}>
    (Yes,record{b:mark(3),a:mark(8)})));
 println(pick(variant<sum{No:record{},Yes:record{a:i64,b:i64}}>(No,record{})));
}
""", "3\n8\n5\n0\n")

check("slices", """
fn total(a:array<record{x:i64},4>)->i64{
 let b=a[1:4][0:2];var i=0;var n=0;
 while i<b[]{n=n+b[i]["x"];i=i+1;}return n;
}
fn main(){println(total(array[record{x:1},record{x:2},record{x:8},record{x:16}]));}
""", "10\n")

check("empty", """
fn main(){let a:array<i64,0>=array[];let b=a[0:0];println(a[]);println(b[]);}
""", "0\n0\n")

check("ordered_bounds", """
fn mark(i:i64)->i64{println(i);return i;}
fn main(){let a=array[1,2,3];let b=a[mark(-1):mark(2)];println(b[]);}
""", "-1\n2\n", 70)

check("singleton", """
enum Single{Only(i64)}
fn f(x:Single)->i64{return match x with {Only(n)=>n};}
fn main(){let a=array[f(variant<Single>(Only,17))];println(a[0]);}
""", "17\n")

# Legacy match arm syntax elaborates to immediate lambdas, including a product
# parameter and a lexical capture. Exercise both conversions in one module.
check("legacy", """
fn main(){
 let extra=3;
 let s=variant<sum{No:record{},Yes:record{x:i64}}>(Yes,record{x:8});
 println(match(s){No(unused)=>0,Yes(p)=>extra+p["x"]});
}
""", "11\n")
