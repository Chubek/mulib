#!/usr/bin/env python3
"""HIR/MIR/C11 binary64 values/literals: independent oracle and closed boundaries."""
import math
import os
import random
import struct
from support import NativeTests, run

tests = NativeTests()
tests.valid('f64', '0x1.5400000000000p+5\n')


def signed(bits):
    return bits if bits < (1 << 63) else bits - (1 << 64)


def value(bits):
    return struct.unpack('>d', struct.pack('>Q', bits))[0]


def bits(number):
    if math.isnan(number):
        return 0x7ff8000000000000
    return struct.unpack('>Q', struct.pack('>d', number))[0]


def literal(number):
    return f'f64_from_bits({signed(bits(number))})'


# Independent Python binary64 arithmetic/bit serialization oracle. Cover normal,
# subnormal, signed zero, overflow, infinities and NaNs plus seeded random bits.
rng = random.Random(0xF641)
corpus = [0, 1 << 63, 1, (1 << 63) | 1, 0xfffffffffffff,
          0x10000000000000, 0x7fefffffffffffff, 0xffefffffffffffff,
          0x3ff0000000000000, 0xbff0000000000000, 0x7ff0000000000000,
          0xfff0000000000000, 0x7ff8000000000001, 0xfff0000000000001]
corpus += [rng.getrandbits(64) for _ in range(100)]
statements, expected = [], []
for raw in corpus:
    a = value(raw)
    statements.append(f'println(f64_to_bits(f64_from_bits({signed(raw)})));')
    expected.append(str(signed(bits(a))))
    statements.append(f'println(f64_signbit(f64_from_bits({signed(raw)})));')
    expected.append(str(bool(bits(a) >> 63)).lower())
    statements.append(f'println(f64_is_finite(f64_from_bits({signed(raw)})));')
    expected.append(str(math.isfinite(a)).lower())

for i in range(120):
    a, b = value(rng.choice(corpus)), value(rng.choice(corpus))
    # Python division raises on zero; its other arithmetic has the IEEE result.
    for op in ['+', '-', '*', '/']:
        if op == '/' and b == 0:
            result = math.nan if a == 0 or math.isnan(a) else math.copysign(math.inf, a * math.copysign(1, b))
        else:
            result = {'+': lambda: a + b, '-': lambda: a - b,
                      '*': lambda: a * b, '/': lambda: a / b}[op]()
        statements.append(f'println(f64_to_bits({literal(a)} {op} {literal(b)}));')
        expected.append(str(signed(bits(result))))
    for op in ['==', '!=', '<', '<=', '>', '>=']:
        result = {'==': a == b, '!=': a != b, '<': a < b,
                  '<=': a <= b, '>': a > b, '>=': a >= b}[op]
        statements.append(f'println({literal(a)} {op} {literal(b)});')
        expected.append(str(result).lower())

for integer in [0, 1, -1, (1 << 53) - 1, (1 << 53) + 1, (1 << 53) + 3,
                (1 << 63) - 1, -(1 << 63)] + [signed(rng.getrandbits(64)) for _ in range(64)]:
    statements.append(f'println(f64_to_bits(f64_from_i64({integer})));')
    expected.append(str(signed(bits(float(integer)))))

for number in [0.0, -0.0, 42.75, -42.75, -(2.0 ** 63),
               math.nextafter(2.0 ** 63, 0.0), math.nextafter(-(2.0 ** 63), 0.0)]:
    statements.append(f'println(f64_to_i64({literal(number)}));')
    expected.append(str(int(number)))

# Keep the canonical parser's 2 MiB/source and statement budgets intact.
exe = tests.valid('float_oracle', '\n'.join(expected) + '\n',
                  text='fn main() {\n' + '\n'.join(statements) + '\n}')
for optimization in [[], ['--optimize-ir']]:
    p = run([tests.compiler, '--run-ir', tests.work/'float_oracle.on1x', *optimization])
    assert p.returncode == 0 and p.stdout == '\n'.join(expected) + '\n', p.stderr
    out = tests.work/('float_oracle_mir_opt.c' if optimization else 'float_oracle_mir.c')
    executable = out.with_suffix('')
    p = run([tests.compiler, '--emit-ir-c', tests.work/'float_oracle.on1x', *optimization, '-o', out])
    assert p.returncode == 0, p.stderr
    p = run([tests.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-I', tests.root/'runtime', out, tests.root/'runtime/on1x_runtime.c',
             *tests.flags, '-o', executable])
    assert p.returncode == 0, p.stderr
    p = run([executable])
    assert p.returncode == 0 and p.stdout == '\n'.join(expected) + '\n', p.stderr

# Decimal literals are rounded by the compiler, not the generated C compiler.
spellings = [repr(value(raw)) for raw in corpus if math.isfinite(value(raw))]
spellings += ['1e3', '1E+3', '0.0e-99999', '0e99999',
              '1.00000000000000011102230246251565404236316680908203125',
              '1.00000000000000033306690738754696212708950042724609375']
expected_literals = ''.join(str(signed(bits(float(s)))) + '\n' for s in spellings)
tests.valid('float_literals', expected_literals, text='fn main(){' +
            ''.join('println(f64_to_bits(' + s + '));' for s in spellings) + '}')
p = run([tests.compiler, '--run-ir', tests.work/'float_literals.on1x'])
assert p.returncode == 0 and p.stdout == expected_literals, p.stderr
# Repeat with optimized C, without fast math. No float-host folding occurs in on1xc.
for optimization in ['-O2', '-O3']:
    optimized = tests.work / ('oracle_' + optimization[1:])
    p = run([tests.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             optimization, '-I', tests.root / 'runtime', tests.work / 'float_oracle.c',
             tests.root / 'runtime/on1x_runtime.c', *tests.flags, '-o', optimized])
    assert p.returncode == 0, p.stderr
    p = run([optimized])
    assert p.returncode == 0 and p.stdout == '\n'.join(expected) + '\n', (optimization, p.stderr)

tests.source('float_library', '''
fn make(offset: f64) -> fn(f64) -> f64 {
  return fn(x: f64) -> f64 { return offset + x; };
}
fn duplicate(x) -> _ { return (x, x); }
''')
tests.valid('float_composition', '0x1.5400000000000p+5\n42\ntrue\n', text='''
import "float_library.on1x";
fn use(id: forall(a) fn(a) -> a) -> f64 { return id(f64_from_i64(42)); }
fn twice(x) -> _ { return x + x; }
fn negate(x) -> _ { return -x; }
fn default_integer(x) -> _ { return x + x; }
fn main() {
  let one = f64_from_i64(1); let two = twice(one);
  let (a, b) = duplicate(make(f64_from_i64(40)));
  var f = a; f = f;
  let code: code<f64> = quote { lift(f)(lift(two)) / lift(two) };
  println(f(f64_from_i64(5) / two));
  println(f64_to_i64(run(code) + f64_from_i64(21)));
  assert(use(fn(x) -> _ { return x; }) == b(two));
  assert(negate(one) == -one);
  println(default_integer(21) == 42);
}
''')
tests.valid('float_printing', '0x0.0000000000000p+0\n-0x0.0000000000000p+0\n'
            '0x0.0000000000001p-1022\n0x1.fffffffffffffp+1023\ninf\n-inf\nnan\n', text='''
fn main() {
  let zero = f64_from_i64(0); let one = f64_from_i64(1);
  println(zero); println(-zero); println(f64_from_bits(1));
  println(f64_from_bits(9218868437227405311));
  println(one / zero); println(-one / zero); println(zero / zero);
}
''')

allocator = ['-include', tests.root / 'tests/fixtures/closure_allocator.h',
             tests.root / 'tests/fixtures/closure_allocator.c']
allocated = tests.valid('float_owned_capture', '', extra=allocator, text='''
extern fn live_environments() -> i64;
fn make(x: f64) -> fn() -> f64 { return fn() -> f64 { return x; }; }
fn main() {
  { let f = make(f64_from_i64(42)); let pair = (f, f64_from_i64(0));
    let (g, z) = pair; assert(g() + z == f64_from_i64(42)); }
  assert(live_environments() == 0);
}
''')
p = run([allocated], env={**os.environ, 'ON1X_TEST_ENV_FAIL_AFTER': '0'})
assert p.returncode == 72 and 'environment allocation failure' in p.stderr, p

# Both Python and native processors carry the additive type constructor through
# schema validation/reconstruction; signature deriving remains syntactic only.
ppx_source = tests.source('float_ppx', '''
@derive("derive", "signature") fn half(x: f64) -> f64 { return x / 2.0; }
fn main() { println(half_signature()); println(half(85.0)); }
''')
plugin = tests.build('float_native_derive', source=tests.root / 'stdppx/native/derive.on1x')
env = {**os.environ, 'ON1X_PPX_SCHEMA': str((tests.root / 'grammar/PPX.schema.json').resolve())}
for name, flags in [('python', ['--stdppx', 'derive']), ('native', ['--ppx', 'derive', plugin])]:
    out, exe = tests.work / ('float_ppx_' + name + '.c'), tests.work / ('float_ppx_' + name)
    p = run([tests.compiler, '--emit-c', ppx_source, *flags, '-o', out], env=env)
    assert p.returncode == 0, (name, p.stderr)
    p = run([tests.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-I', tests.root / 'runtime', out, tests.root / 'runtime/on1x_runtime.c',
             *tests.flags, '-o', exe])
    assert p.returncode == 0, (name, p.stderr)
    p = run([exe])
    assert p.returncode == 0 and p.stdout == 'fn(f64) -> f64\n0x1.5400000000000p+5\n', (name, p)

# Actual separate-TU definitions and _Generic signature checks; no fake pointer ABI.
abi = tests.work / 'float_boundary.c'
abi.write_text('''
#include <on1x_runtime.h>
#include <stdint.h>
#include <stdio.h>
double raw_float(double a, double b, int64_t c) { return a / b + (double)c; }
double raw_mark(int64_t n) { printf("%lld\\n", (long long)n); return (double)n; }
_Static_assert(_Generic(&raw_float, double (*)(double,double,int64_t): 1, default: 0), "double ABI");
void raw_mode(int64_t mode);
#include <xmmintrin.h>
void raw_mode(int64_t mode) {
  unsigned csr = _mm_getcsr();
  if(mode==0) csr |= 0x2000u;
  if(mode==1) csr |= 0x8000u;
  if(mode==2) csr |= 0x0040u;
  if(mode==3) csr &= ~0x0200u;
  _mm_setcsr(csr);
}
''')
abi_program = '''
extern fn raw_float((a, (b, c)): (f64, (f64, i64))) -> f64;
extern fn raw_mark(n: i64) -> f64;
fn main() {
  println(raw_float((raw_mark(84), (raw_mark(2), 0))));
  assert(raw_mark(1) < raw_mark(2));
  assert(false && raw_mark(99) == f64_from_i64(99) || true);
}
'''
tests.valid('float_abi', '84\n2\n0x1.5000000000000p+5\n1\n2\n', text=abi_program, extra=[abi])

# C++ definitions also use a C ABI, compiled independently of generated ISO C11.
cxx = tests.cc + '++' if tests.cc.endswith('clang') else tests.cc[:-3] + 'g++' if tests.cc.endswith('gcc') else None
if cxx is not None:
    cpp = tests.work / 'float_boundary.cpp'
    cpp.write_text('''
#include <on1x_runtime.h>
#include <type_traits>
extern "C" double raw_cpp(double a, double b) { return a * b; }
static_assert(std::is_same_v<decltype(&raw_cpp), double (*)(double,double)>);
static_assert(alignof(double)==8 && sizeof(double)==8);
''')
    obj = tests.work / 'float_boundary_cpp.o'
    p = run([cxx, '-std=c++20', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-I', tests.root / 'runtime', '-c', cpp, *tests.flags, '-o', obj])
    assert p.returncode == 0, p.stderr
    tests.valid('float_cpp_abi', '0x1.5000000000000p+5\n', extra=[obj], text='''
extern fn raw_cpp(a: f64, b: f64) -> f64;
fn main() { println(raw_cpp(f64_from_i64(21), f64_from_i64(2))); }
''')

# GCC can select x87 even on x86_64. Recompile the real evaluator translation
# unit with that mode and use the canonical frontend, not a substitute evaluator.
# The operation must fail at its source location instead of double-rounding.
if cxx is not None and tests.cc.endswith('gcc'):
    evaluator = tests.work/'evaluate_x87.o'
    p = run([cxx, '-std=c++20', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-mfpmath=387', '-I', tests.root/'include', '-c',
             tests.root/'src/backend/evaluate.cpp', *tests.flags, '-o', evaluator])
    assert p.returncode == 0, p.stderr
    probe = tests.work/'evaluate_x87.cpp'
    probe.write_text('''
#include <on1x/backend/mir.hpp>
#include <string>
int main(){
  auto unit=on1x::parse(on1x::scan("fn main(){println(1.0+2.0);}","x87.on1x"));
  on1x::check(unit);auto module=on1x::mir::lower(unit);
  try{(void)on1x::mir::evaluate(module);}
  catch(const on1x::Error& error){
    return error.where.file=="x87.on1x"&&error.where.line==1&&
      std::string(error.what()).find("requires Linux x86_64 SSE2 binary64 evaluation")!=std::string::npos?0:1;
  }
  return 1;
}
''')
    executable = tests.work/'evaluate_x87'
    p = run([cxx, '-std=c++20', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-I', tests.root/'include', probe, evaluator,
             tests.compiler.parent/'libon1x_frontend.a', tests.compiler.parent/'libon1x_scanner.a',
             *tests.flags, '-o', executable])
    assert p.returncode == 0, p.stderr
    p = run([executable])
    assert p.returncode == 0 and not p.stdout and not p.stderr, p

# Force a wrong actual prototype to be visible alongside generated declarations.
bad_header = tests.work / 'float_wrong_abi.h'
bad_header.write_text('#include <stdint.h>\nint64_t raw_float(double,double,int64_t);\n')
p = run([tests.cc, '-std=c11', '-I', tests.root / 'runtime', '-include', bad_header,
         '-c', tests.work / 'float_abi.c', '-o', tests.work / 'bad_abi.o'])
assert p.returncode != 0 and 'raw_float' in p.stderr, p

for number in [math.nan, math.inf, -math.inf, 2.0 ** 63, math.nextafter(-(2.0 ** 63), -math.inf)]:
    name = 'float_bad_cast_' + str(bits(number))
    exe = tests.build(name, text=f'fn main() {{ println(f64_to_i64({literal(number)})); }}')
    p = run([exe])
    assert p.returncode == 70 and 'f64 to i64 out of range' in p.stderr and not p.stdout, p

for mode in range(4):
    exe = tests.build('float_bad_mode_' + str(mode), extra=[abi], text=f'''
extern fn raw_mode(n: i64);
fn main() {{ raw_mode({mode}); println(f64_from_i64(1) / f64_from_i64(0)); }}
''')
    p = run([exe])
    assert p.returncode == 70 and 'unsupported f64 environment' in p.stderr and not p.stdout, p

negative = [
    ('mixed', 'let x = f64_from_i64(1) + 2;'),
    ('modulo', 'let x = f64_from_i64(1) % f64_from_i64(2);'),
    ('integer_literal', 'let x: f64 = 1;'),
    ('malformed_float_literal', 'let x = 1.5e+;'),
    ('condition', 'if f64_from_i64(1) {}'),
    ('logical', 'let x = !f64_from_i64(1);'),
    ('cast_arity', 'let x = f64_to_i64();'),
    ('cast_type', 'let x = f64_to_i64(1);'),
    ('bits_type', 'let x = f64_from_bits(true);'),
    ('nan_type', 'let x = f64_is_nan(1);'),
    ('same_numeric_monotype', 'let f=fn(x)->_{return -x;}; f(1); f(f64_from_i64(1));'),
    ('mutable_capture', 'var x=f64_from_i64(1); let f=fn()->f64{return x;};'),
    ('float_overflow', 'let x=1e309;'),
    ('float_underflow', 'let x=1e-9999;'),
    ('unknown_float_suffix', 'let x=1.0f16;'),
    ('float_oversized', 'let x=' + '1'*1024 + '.0;'),
]
for name, body in negative:
    path = tests.source('float_reject_' + name, 'fn main() {' + body + '}')
    output = tests.work / 'preserved.c'
    output.write_text('preserve me')
    for mode in ['--emit-c', '--emit-ir', '--emit-ir-c', '--run-ir']:
        p = run([tests.compiler, mode, path, '-o', output])
        assert p.returncode == 1 and ': error:' in p.stderr and not p.stdout, (name, mode, p)
        assert output.read_text() == 'preserve me', (name, mode)

# Raw foreign float declarations still require foreign descriptors, even unused.
unsupported = tests.source('float_foreign_mir', 'extern fn foreign(x:f64)->f64; fn main(){}')
for mode in ['--emit-ir', '--emit-ir-c', '--run-ir', '--dump-passes']:
    p = run([tests.compiler, mode, unsupported])
    assert p.returncode == 1 and 'ON1X-MIR-UNSUPPORTED' in p.stderr and not p.stdout, (mode, p)
path = tests.root / 'examples/f64.on1x'

# No float-specific ABI requirement is added to ordinary integer output.
plain = tests.source('plain_integer', 'fn main() { println(42); }')
p = run([tests.compiler, '--emit-c', plain])
assert p.returncode == 0 and 'ON1X_F64_ABI_VERSION' not in p.stdout, p
source = tests.work / 'float_oracle.c'
for flag in ['-ffast-math', '-ffinite-math-only']:
    p = run([tests.cc, '-std=c11', flag, '-I', tests.root / 'runtime', '-c', source,
             '-o', tests.work / 'forbidden.o'])
    assert p.returncode != 0 and 'forbids fast/finite-only math' in p.stderr, p

incompatible = tests.work / 'incompatible-runtime'
incompatible.mkdir(exist_ok=True)
header = (tests.root / 'runtime/on1x_runtime.h').read_text()
(incompatible / 'on1x_runtime.h').write_text(header.replace('#define ON1X_F64_ABI_VERSION 1', '#define ON1X_F64_ABI_VERSION 2'))
p = run([tests.cc, '-std=c11', '-I', incompatible, '-fsyntax-only', source])
assert p.returncode != 0 and 'unsupported On1x f64 target or runtime ABI' in p.stderr, p
p = run([tests.cc, '-std=c11', '-U__SSE2__', '-I', tests.root / 'runtime', '-fsyntax-only', source])
assert p.returncode != 0 and 'unsupported On1x f64 target or runtime ABI' in p.stderr, p
first = run([tests.compiler, '--emit-c', path])
second = run([tests.compiler, '--emit-c', path])
assert first.returncode == second.returncode == 0 and first.stdout == second.stdout

# The normal build helper, not just the native test linker, must execute the slice.
built = tests.work / 'helper-float'
p = run(['python3', tests.root / 'tools/on1x-build.py', path, '--on1xc', tests.compiler,
         '--cc', tests.cc, '-o', built, *['--cflag=' + flag for flag in tests.flags]])
assert p.returncode == 0, p.stderr
p = run([built])
assert p.returncode == 0 and p.stdout == '0x1.5400000000000p+5\n', p
print('f64 value profile: arithmetic/bit oracle, composition, raw ABI and rejection PASS')
