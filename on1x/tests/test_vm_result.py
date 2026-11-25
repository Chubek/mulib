#!/usr/bin/env python3
"""Qualify copy-only Result propagation in VM artifacts against direct C11."""
from support import NativeTests, run


t = NativeTests()
source = t.source("copy_result", """\
enum Result<T,E>{Ok(T),Err(E)}
fn mark(n:i64)->i64{println(n);return n;}
fn fetch(n:i64)->Result<i64,i64>{
  println(n);
  if n<0{return variant<Result<i64,i64>>(Err,30-n);}
  return variant<Result<i64,i64>>(Ok,n);
}
fn bump(n:i64)->Result<i64,i64>{
  let value=fetch(n)?;
  return variant<Result<i64,i64>>(Ok,value+1);
}
fn sum(a:i64,b:i64,c:i64)->i64{return a+b+c;}
fn classify(n:i64,check:bool)->Result<bool,i64>{
  let value=sum(mark(1),bump(n)?,mark(2));
  let verdict=(check && (fetch(-5)?>0)) || value==8;
  return variant<Result<bool,i64>>(Ok,verdict);
}
fn success(b:bool)->i64{println(b);if b{return 7;}return 0;}
fn observe(n:i64,check:bool)->i64{
  return match classify(n,check) with {
    Ok(b)=>success(b),
    Err(e)=>{println(e);e}
  };
}
fn main()->i64{
  let ok=observe(4,false);
  let arg_error=observe(-2,false);
  let rhs_error=observe(4,true);
  return ok+arg_error+rhs_error;
}
""")

# Hand-derived: success skips fetch(-5); an Err in the second argument skips
# mark(2); taking the && RHS propagates Err before reaching the || alternative.
expected_stdout = "1\n4\n2\ntrue\n1\n-2\n32\n1\n4\n2\n-5\n35\n"
expected_status = 7 + 32 + 35


def check(label, command):
    p = run(command)
    assert (p.returncode, p.stdout, p.stderr) == (expected_status, expected_stdout, ""), (label, p)


check("direct C11", [t.build("copy_result", source=source)])
runner = t.compiler.with_name("on1x-vm")
for suffix, flags in (("", []), ("-opt", ["--optimize-ir"])):
    image = t.work / f"copy_result{suffix}.o1vm"
    p = run([t.compiler, "--emit-vm", source, *flags, "-o", image])
    assert p.returncode == 0, ("VM emission", flags, p)
    assert image.read_bytes().startswith(b"O1VM")
    p = run([runner, "--verify", image])
    assert p.returncode == 0, ("VM verification", flags, p)
    check(f"saved VM{suffix}", [runner, image])

# Success on the VM source path must not broaden the scalar MIR CLI profile.
output = t.work / "preserved.mir"
for mode in ("--emit-ir", "--emit-ir-c", "--run-ir"):
    output.write_bytes(b"preserved")
    p = run([t.compiler, mode, source, "-o", output])
    assert p.returncode == 1 and "ON1X-MIR-UNSUPPORTED" in p.stderr, (mode, p)
    assert str(source) in p.stderr and not p.stdout, (mode, p)
    assert output.read_bytes() == b"preserved", mode

print("copy-only Result: direct C11 and saved VM agree with oracle; scalar MIR rejects")
