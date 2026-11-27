#!/usr/bin/env python3
"""Independent C11 and MIR reference oracles for saved VM artifacts."""
from pathlib import Path
import subprocess
import sys

compiler, runner, cc = sys.argv[1:4]
root, work = map(Path, sys.argv[4:])
work.mkdir(parents=True, exist_ok=True)

def invoke(*argv, status=0):
    p = subprocess.run(argv, capture_output=True, timeout=45)
    assert p.returncode == status, (argv, p.returncode, p.stderr)
    return p

def exercise(name, text, output, status=0, trap=None, *, mir_source=True):
    src, ir, image, c, exe = [work/(name+s) for s in ('.on1x','.mir','.o1vm','.c','.exe')]
    src.write_text(text)
    invoke(compiler, '--emit-c', src, '-o', c)
    invoke(cc, '-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I',root/'runtime',
           c,root/'runtime/on1x_runtime.c','-lm','-o',exe)
    invoke(compiler, '--emit-vm', src, '-o', image)
    code = image.read_bytes()
    assert code.startswith(b'O1VM\x01\r\n') and b'(on1x.mir' not in code
    invoke(runner, '--verify', image)
    results = [invoke(exe,status=status), invoke(runner,image,status=status),
               invoke(compiler,'--run-vm',src,status=status)]
    if mir_source:
        invoke(compiler, '--emit-ir', src, '-o', ir)
        assert invoke(compiler, '--emit-vm', '--ir-input', ir).stdout == code
        results.append(invoke(compiler,'--run-ir',src,status=status))
    invoke(compiler,'--emit-vm','--optimize-ir',src,'-o',image)
    results.append(invoke(runner,image,status=status))
    for p in results:
        assert p.stdout == output.encode(), (name,p.stdout,p.stderr)
        assert (trap.encode() in p.stderr) if trap else not p.stderr, (name,p.stderr)
    if trap and mir_source:
        # The VM must retain the original failing operator's exact location.
        assert results[1].stderr.split(b': ON1X-VM:')[0] == results[3].stderr.split(b': ON1X-MIR-EVAL:')[0]
    return image

exercise('recursion', '''
fn fib(n:i64)->i64{if n<2{return n;}return fib(n-1)+fib(n-2);}
fn main()->i64{println(false && (1/0==0));println(fib(9));return 3;}
''','false\n34\n',3)
exercise('tuple', '''
fn pair((x,(y,z)):(i64,(bool,f64)))->(bool,i64){return (y && f64_signbit(z),x);}
fn main(){let(flag,n)=pair((42,(true,-0.0)));println(flag);println(n);}
''','true\n42\n')
exercise('numbers', '''
fn main(){println(u64_wrapping_add(18446744073709551615u64,1u64));
println(f32_signbit(-0.0f32));println(f64_is_nan(0.0/0.0));println(f32_from_i64(3));}
''','0\ntrue\ntrue\n0x1.800000p+1\n')
exercise('trap', '''
fn mark(x:i64)->i64{println(x);return x;}
fn main(){println(mark(7)/mark(0));println(99);}
''','7\n0\n',70,'invalid division')
exercise('shift', '''
fn main(){println(7);println(u8_shl(255u8,1));}
''','7\n',70,'unsigned left shift overflow')

closures = '''
fn pos(x:i64)->i64{return x;}
fn choose(which:i64,bias:i64)->fn(i64)->i64 {
 println(1);
 if which==0 {return pos;}
 if which==1 {return fn(x:i64)->i64{return bias+x;};}
 let pair=(bias,2);
 return fn(x:i64)->i64 {let (a,b)=pair;return a-x*b;};
}
fn apply(f:fn(i64)->i64,x:i64)->i64{return f(x);}
fn arg(n:i64)->i64{println(2);return n;}
'''
exercise('closed_callbacks', closures+'''
fn main(){
 println(choose(0,10)(arg(3)));
 let a=choose(1,10);{let bias=99;assert(bias==99);println(apply(a,arg(3)));}
 let b=choose(2,10);println(apply(b,arg(3)));println(b(5));
}
''','1\n2\n3\n1\n2\n13\n1\n2\n4\n0\n',mir_source=False)
exercise('closure_order', closures+'''
fn bad()->fn(i64)->i64{println(7);assert(false);return pos;}
fn main(){println(bad()(arg(3)));}
''','7\n',70,'assertion failed',mir_source=False)
exercise('captured_operand_trap', closures+'''
fn main(){println(choose(1,19)(arg(7)/0));}
''','1\n2\n',70,'invalid division',mir_source=False)
exercise('immediate_product', '''
record Point {x:i64,y:i64}
fn main(){let scale=3;
 println(fn(p:Point)->i64{return (p["x"]+p["y"])*scale;}(record<Point>{x:4,y:10}));
}
''','42\n',mir_source=False)

bad=work/'preserved.o1vm'
future_ir=work/'future.mir'
words=(work/'recursion.mir').read_bytes().split(maxsplit=2)
assert words[0]==b'(on1x.mir' and words[1] in (b'1',b'2',b'3',b'4')
future_ir.write_bytes(b' '.join((words[0],b'5',words[2])))
bad.write_bytes(b'preserve')
p=invoke(compiler,'--emit-vm','--ir-input',future_ir,'-o',bad,status=1)
assert b'unsupported format version' in p.stderr and not p.stdout and bad.read_bytes()==b'preserve'

future_image=work/'future.o1vm'
saved=(work/'recursion.o1vm').read_bytes()
version_offset=len(b'O1VM\x01\r\n')
future_image.write_bytes(saved[:version_offset]+b'\x05'+saved[version_offset+1:])
for argv in [(runner,'--verify',future_image),(runner,future_image)]:
    p=invoke(*argv,status=1)
    assert b'unsupported executable MIR version' in p.stderr and not p.stdout

for text in ['fn main(){println(false && missing);}',
             'fn main(){var x=3;let f=fn()->i64{return x;};println(f());}',
             'fn main(){let n=3;let f=fn()->i64{return n;};let g=fn()->i64{return f();};println(g());}',
             'extern fn external(x:i64)->i64;fn main(){println(external(4));}']:
    src=work/'rejected.on1x';src.write_text(text);bad.write_bytes(b'preserve')
    p=invoke(compiler,'--emit-vm',src,'-o',bad,status=1)
    assert str(src).encode() in p.stderr and not p.stdout and bad.read_bytes()==b'preserve'
src=work/'bounded.on1x'
src.write_text('fn main(){while true{}}')
invoke(compiler,'--emit-vm',src,'-o',bad)
assert b'step budget' in invoke(runner,'--steps','25',bad,status=75).stderr

src = work/'frame-limit.on1x'
image = work/'frame-limit.o1vm'
src.write_text('fn descend(n:i64)->i64{\n'
               '  return descend(n+1);\n'
               '}\n'
               'fn main(){println(7);println(descend(0));}\n')
invoke(compiler, '--emit-vm', src, '-o', image)
invoke(runner, '--verify', image)
expected = invoke(compiler, '--run-ir', src, status=75)
assert expected.stdout == b'7\n'
assert expected.stderr.startswith(f'{src}:1:'.encode())
assert expected.stderr.endswith(b': evaluation call-depth budget exhausted\n')
for argv in [(runner, image), (runner, '--depth', '1', image),
             (compiler, '--run-vm', src)]:
    actual = invoke(*argv, status=75)
    assert actual.stdout == expected.stdout
    assert actual.stderr == expected.stderr.replace(b': ON1X-MIR-EVAL:', b': ON1X-VM:'), actual.stderr

bad.write_bytes(b'O1VM\x01\r\n\x03'+b'\xff'*10)
assert b'integer overflow' in invoke(runner,'--verify',bad,status=1).stderr
print('C11 vs reference MIR vs bytecode (plain/optimized), saved artifacts, bounds and source rejection: PASS')
