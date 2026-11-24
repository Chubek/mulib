#!/usr/bin/env python3
"""Decision-phi lowering: source effects, lexical state, traps and native parity."""
from support import NativeTests, run

t = NativeTests()
archon = t.compiler.parent / 'backends/mir-archon/on1x-archon-mir'
abi = archon.parent / 'libon1x_archon_scalar_abi.a'


def command(args):
    p = run(args)
    assert p.returncode == 0, (list(map(str, args)), p.stderr)
    return p


runtime = t.work / 'runtime.o'
command([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
         '-I', t.root / 'runtime', '-c', t.root / 'runtime/on1x_runtime.c',
         *t.flags, '-o', runtime])


def exercise(name, text, output, trap=None, *, success_status=0):
    source = t.source(name, text)
    profiles = []
    for option in [[], ['--optimize-ir']]:
        suffix = 'opt' if option else 'plain'
        profiles.append(('eval-' + suffix, run([t.compiler, '--run-ir', *option, source])))
        for mode in ['--emit-c', '--emit-ir-c']:
            if mode == '--emit-c' and option:
                continue
            code = t.work / f'{name}-{mode}-{suffix}.c'
            exe = code.with_suffix('')
            command([t.compiler, mode, *option, source, '-o', code])
            command([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                     '-I', t.root / 'runtime', code, runtime, *t.flags, '-lm', '-o', exe])
            profiles.append((mode + suffix, run([exe])))
        if archon.is_file() and abi.is_file():
            ir, asm = t.work / f'{name}-{suffix}.mir', t.work / f'{name}-{suffix}.s'
            exe = asm.with_suffix('')
            command([t.compiler, '--emit-ir', *option, source, '-o', ir])
            command([archon, '--ir-input', ir, '-o', asm])
            command([t.cc, asm, abi, runtime, *t.flags, '-lm', '-o', exe])
            profiles.append(('native-' + suffix, run([exe])))
    for profile, p in profiles:
        assert p.stdout == output, (name, profile, p.stdout, output, p.stderr)
        assert p.returncode == (70 if trap else success_status), (name, profile, p.returncode, p.stderr)
        assert (trap in p.stderr) if trap else not p.stderr, (name, profile, p.stderr)


exercise('effects', '''
fn mark(n:i64,b:bool)->bool { println(n);return b; }
fn choose(a:bool,b:bool)->bool {
 return (mark(1,a)&&mark(2,b))||(mark(3,b)&&mark(4,a));
}
fn main() {
 println(choose(false,false));println(choose(false,true));
 println(choose(true,false));println(choose(true,true));
 var n=0;var answer=false;
 while mark(10,n<3)&&(mark(11,true)||mark(99,false)) {
  var answer=n==1;
  println(answer&&(mark(12,true)||mark(98,false)));
  n=n+1;
 }
 println(answer);println(n);
 println(false&&(1/0==0));println(true||(1/0==0));
}
''', '1\n3\nfalse\n1\n3\n4\nfalse\n1\n2\n3\nfalse\n1\n2\ntrue\n'
     '10\n11\nfalse\n10\n11\n12\ntrue\n10\n11\nfalse\n10\nfalse\n3\nfalse\ntrue\n')

for name, expr, trace in [
    ('condition_trap', '(mark(1,true)&&(1/0==0))||mark(2,true)', '1\n'),
    ('selected_trap', 'mark(3,false)||(mark(4,true)&&(1/0==0))', '3\n4\n'),
]:
    exercise(name, 'fn mark(n:i64,b:bool)->bool{println(n);return b;}'
             'fn main(){println(' + expr + ');println(99);}', trace, 'invalid division')

# Both outcomes exit 70. The trace and diagnostic must distinguish a normal
# return from the first fault in left-to-right call argument evaluation.
ordered_call = '''
fn first(n:i64)->i64 { println(1);return 24/n; }
fn second(n:i64)->i64 { println(2);return 13%n; }
fn pair(a:i64,b:i64)->i64 { println(3);return a*10+b; }
'''
for name, args, trace, trap in [
    ('call_values', 'first(4),second(3)', '1\n2\n3\n61\n', None),
    ('call_first_fault', 'first(0),second(0)', '1\n', 'invalid division'),
    ('call_second_fault', 'first(4),second(0)', '1\n2\n', 'invalid modulo'),
]:
    exercise(name, ordered_call + 'fn main()->i64{println(pair(' + args + '));return 70;}',
             trace, trap, success_status=70)

# A structural regression gate independent of runtime timings and optimization.
source = t.source('storage_gate', '''
fn pick(a:bool,b:bool)->bool{return a&&b;}
fn main(){println(pick(true,false));}
''')
ir = command([t.compiler, '--emit-ir', source]).stdout
assert 'slot ' not in ir and 'store ' not in ir and 'load ' not in ir, ir
print('decision phi source differential PASS; native=' + str(archon.is_file() and abi.is_file()))
