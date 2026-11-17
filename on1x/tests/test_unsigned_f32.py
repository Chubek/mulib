#!/usr/bin/env python3
"""Exact unsigned/binary32 values, explicit conversions and C11 behavior."""
import math
import random
import struct
from support import NativeTests, run

t = NativeTests()
t.valid('unsigned_f32', '18446744073709551615\n0\n42\n0x1.800000p+1\n2\n0x1.000000p+64\n')
ast=run([t.compiler,'--emit-ast',t.root/'examples/unsigned_f32.on1x'])
assert ast.returncode==0 and 'F32Type' in ast.stdout and 'U32Type' in ast.stdout and 'U64Type' in ast.stdout,ast.stderr
ppx_c=t.work/'ppx_numeric.c'
p=run([t.compiler,'--emit-c',t.root/'examples/unsigned_f32.on1x','--ppx','identity',t.root/'stdppx/identity.py','-o',ppx_c])
assert p.returncode==0,p.stderr
assert ppx_c.read_text()==(t.work/'unsigned_f32.c').read_text()

# Exact user-declared raw scalar ABI, including the canonical flattening path.
raw_source=t.source('raw_numeric','''
extern fn raw_numeric((a,b):(u8,u16),c:u32,d:u64,x:f32)->f32;
extern fn raw_wide(x:u64)->u64;
fn main(){println(raw_numeric((u8_from_bits(-1),u16_from_bits(-1)),u32_from_bits(-1),u64_from_bits(-1),f32_from_f64(1.5)));
println(raw_wide(u64_from_bits(-1)));}
''')
raw_c=t.work/'raw_main.c'
p=run([t.compiler,'--emit-ast',raw_source])
assert p.returncode==0 and all(tag in p.stdout for tag in ['U8Type','U16Type','U32Type','U64Type','F32Type']),p.stderr
p=run([t.compiler,'--emit-c',raw_source,'-o',raw_c]);assert p.returncode==0,p.stderr
provider='''#include <stdint.h>
float raw_numeric(uint8_t a,uint16_t b,uint32_t c,uint64_t d,float x){
 if(a!=UINT8_MAX||b!=UINT16_MAX||c!=UINT32_MAX||d!=UINT64_MAX)return -42.0f;
 return x*2.0f;
}
uint64_t raw_wide(uint64_t x){return x;}
'''
for language in ['c','c++']:
    source=t.work/('raw_provider.c' if language=='c' else 'raw_provider.cpp')
    source.write_text(provider if language=='c' else 'extern "C" {\n'+provider+'}\n')
    obj=t.work/('raw_'+language+'.o')
    p=run([t.cc,'-x',language,'-std=c11' if language=='c' else '-std=c++20','-Wall','-Wextra','-Wpedantic','-Werror',
           '-c',source,*t.flags,'-o',obj]);assert p.returncode==0,p.stderr
    exe=t.work/('raw_'+language)
    p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I',t.root/'runtime',raw_c,obj,
           t.root/'runtime/on1x_runtime.c',*t.flags,'-o',exe]);assert p.returncode==0,p.stderr
    p=run([exe]);assert p.returncode==0 and p.stdout=='0x1.800000p+1\n18446744073709551615\n',p.stderr

def signed(n): return n if n < (1 << 63) else n - (1 << 64)
def value32(n): return struct.unpack('>f', struct.pack('>I', n))[0]
def bits32(x):
    if math.isnan(x): return 0x7fc00000
    try: return struct.unpack('>I', struct.pack('>f', x))[0]
    except OverflowError: return 0xff800000 if x < 0 else 0x7f800000
def integer_bits32(n):
    if not n:return 0
    exponent=n.bit_length()-1
    if exponent<=23:mantissa=n << (23-exponent)
    else:
        shift=exponent-23;mantissa=n>>shift;remainder=n&((1<<shift)-1);half=1<<(shift-1)
        if remainder>half or (remainder==half and mantissa&1):mantissa+=1
        if mantissa==(1<<24):mantissa>>=1;exponent+=1
    return ((exponent+127)<<23)|(mantissa&0x7fffff)

rng = random.Random(0x32A64)
body, expected = [], []
for width in [8, 16, 32, 64]:
    mask = (1 << width) - 1
    corpus = [0, 1, mask, mask-1, 1 << (width-1)] + [rng.getrandbits(width) for _ in range(12)]
    for a, b in zip(corpus, reversed(corpus)):
        prefix = f'u{width}'
        statements = [f'let a={prefix}_from_bits({signed(a)});let b={prefix}_from_bits({signed(b)});']
        operations = [('wrapping_add', (a+b)&mask), ('wrapping_sub', (a-b)&mask),
                      ('wrapping_mul', (a*b)&mask), ('and', a&b), ('or', a|b), ('xor', a^b)]
        for op, result in operations:
            statements.append(f'println({prefix}_{op}(a,b));'); expected.append(str(result))
        statements += [f'println({prefix}_not(a));', 'println(a<b);', f'println({prefix}_shr(a,{width-1}));']
        expected += [str((~a)&mask), str(a<b).lower(), str(a >> (width-1))]
        if a+b <= mask: statements.append('println(a+b);'); expected.append(str(a+b))
        if a>=b: statements.append('println(a-b);'); expected.append(str(a-b))
        if a*b <= mask: statements.append('println(a*b);'); expected.append(str(a*b))
        if b: statements += ['println(a/b);', 'println(a%b);']; expected += [str(a//b), str(a%b)]
        body.append('{'+''.join(statements)+'}')
unsigned_exe = t.valid('unsigned_oracle', '\n'.join(expected)+'\n', text='fn main(){'+''.join(body)+'}')
body,expected=[],[]
for width in [8,16,32,64]:
    n=f'u{width}'
    for source_width in [8,16,32,64]:
        body.append(f'println({n}_from_u{source_width}(u{source_width}_from_i64(42)));');expected.append('42')
    for expression in [f'{n}_to_i64({n}_from_i64(42))',f'{n}_to_bits({n}_from_i64(42))',
                       f'f64_to_i64(f64_from_{n}({n}_from_i64(42)))',
                       f'f32_to_i64(f32_from_{n}({n}_from_i64(42)))',
                       f'{n}_from_f32(f32_from_f64(42.75))',f'{n}_from_f64(42.75)']:
        body.append('println('+expression+');');expected.append('42')
for expression,result in [('f32_to_i64(f32_from_f64(-42.75))','-42'),
                           ('f32_to_i64(f32_from_f64(-9.223372036854776e18))','-9223372036854775808'),
                           ('u64_from_f32(f32_from_bits(0x5f7fffff))',str((1<<64)-(1<<40))),
                           ('u8_from_f64(255.999)','255'),
                           ('f64_to_bits(f64_from_f32(f32_from_bits(1)))',str(874<<52))]:
    body.append('println('+expression+');');expected.append(result)
t.valid('numeric_conversions','\n'.join(expected)+'\n',text='fn main(){'+''.join(body)+'}')
t.valid('f32_print','0x0.000000p+0\n-0x0.000000p+0\n0x0.000002p-126\ninf\n-inf\nnan\n',text='fn main(){'+''.join(
    f'println(f32_from_bits({bits}));' for bits in [0,0x80000000,1,0x7f800000,0xff800000,0xffc00001])+'}')

corpus = [0, 1 << 31, 1, 0x80000001, 0x7fffff, 0x800000, 0x7f7fffff, 0xff7fffff,
          0x3f800000, 0xbf800000, 0x7f800000, 0xff800000, 0x7fc00001]
corpus += [rng.getrandbits(32) for _ in range(30)]
body, expected = [], []
for a, b in zip(corpus, reversed(corpus)):
    x, y = value32(a), value32(b)
    body.append(f'{{let a=f32_from_bits({a});let b=f32_from_bits({b});')
    for op in ['+', '-', '*', '/']:
        if op == '/':
            result = (math.nan if x == 0 or math.isnan(x) else
                      math.copysign(math.inf, x * math.copysign(1, y))) if y == 0 else x/y
        else: result = {'+': lambda: x+y, '-': lambda: x-y, '*': lambda: x*y}[op]()
        body.append(f'println(f32_to_bits(a{op}b));'); expected.append(str(bits32(result)))
    for op, result in [('==', x==y), ('!=', x!=y), ('<', x<y), ('<=', x<=y), ('>', x>y), ('>=', x>=y)]:
        body.append(f'println(a{op}b);'); expected.append(str(result).lower())
    body += ['println(f32_to_bits(-a));', 'println(f32_is_nan(a));', 'println(f32_is_finite(a));', 'println(f32_signbit(a));', '}']
    expected += [str(bits32(-x)), str(math.isnan(x)).lower(), str(math.isfinite(x)).lower(),
                 str(not math.isnan(x) and bool(a>>31)).lower()]
for x in [0., -0., 1., -1., 1.000000059604644775390625, 1.000000178813934326171875,
          2.**-150, 2.**-149, 2.**128, float.fromhex('0x1.fffffep127'),
          float.fromhex('0x1.fffffe8p127'), float.fromhex('0x1.ffffffp127'),
          -float.fromhex('0x1.ffffffp127'), 1e300, -1e300]:
    body.append(f'println(f32_to_bits(f32_from_f64({repr(x)})));'); expected.append(str(bits32(x)))
for n in [0, 1, (1<<24)-1, (1<<24)+1, (1<<24)+3, (1<<63)-1, (1<<64)-1,
          (1<<63)+(1<<39)+1,(1<<63)+(3<<39)-1]:
    body.append(f'println(f32_to_bits(f32_from_u64(u64_from_bits({signed(n)}))));')
    expected.append(str(integer_bits32(n)))
float_exe = t.valid('f32_oracle', '\n'.join(expected)+'\n', text='fn main(){'+''.join(body)+'}')

# Same types compose with inference, polymorphic functions, aggregates, closures
# and staging; no fake i64/double carrier is involved in their C11 ABI.
t.valid('numeric_composition', '42\n0x1.800000p+1\n255\n', text='''
fn id(x)->_ { return x; }
fn add(x,y)->_ { return x+y; }
fn main(){
  let u=u16_from_i64(21); let r=record{word:id(add(u,u)), real:f32_from_f64(1.5)};
  let a=array[r["real"], r["real"]];
  let captured=fn(x:f32)->f32{return x+r["real"];};
  println(r["word"]); println(run(quote{lift(captured)(lift(a[0]))}));
  let (small, wide)=(u8_from_bits(-1),u64_from_bits(-1));
  assert(wide>u64_from_i64(0));println(small);
}
''')
for name, text in [
    ('mixed', 'u8_from_i64(1)+u16_from_i64(1)'),
    ('signed_unsigned', 'u64_from_i64(1)+1'),
    ('mixed_float', 'f32_from_i64(1)+1.0'),
    ('negative_unsigned', '-u32_from_i64(1)'),
    ('float_mod', 'f32_from_i64(1)%f32_from_i64(1)'),
]:
    source=t.source(name,'fn main(){println('+text+');}')
    out=t.work/(name+'.c');out.write_text('preserve')
    p=run([t.compiler,'--emit-c',source,'-o',out])
    assert p.returncode==1 and 'error:' in p.stderr and out.read_text()=='preserve',(name,p.stderr)
for text,diagnostic in [('fn main(){let x:u8=1;}','expected u8, got i64'),
                         ('fn u8_from_i64(x:i64)->i64{return x;}fn main(){}','intrinsic name cannot be redefined')]:
    p=run([t.compiler,'--check',t.source('numeric_migration',text)])
    assert p.returncode==1 and diagnostic in p.stderr,p.stderr

failures = [('neg_conversion','u8_from_i64(-1)'), ('narrow','u8_from_i64(256)'),
            ('cross_narrow','u16_from_u64(u64_from_i64(65536))'),
            ('signed_range','u64_to_i64(u64_from_bits(-1))'),
            ('float_bits','f32_from_bits(-1)'), ('float_bits_high','f32_from_bits(4294967296)'),
            ('float_i64','f32_to_i64(f32_from_bits(0x7f800000))'),
            ('float_unsigned','u64_from_f32(f32_from_bits(0x5f800000))'),
            ('negative_float_unsigned','u8_from_f64(-0.5)')]
for width in [8,16,32,64]:
    n=f'u{width}';maxv=f'{n}_from_bits(-1)';zero=f'{n}_from_i64(0)';one=f'{n}_from_i64(1)'
    failures += [(n+'_add',maxv+'+'+one), (n+'_sub',zero+'-'+one), (n+'_mul',maxv+'*'+maxv),
                 (n+'_div',one+'/'+zero), (n+'_mod',one+'%'+zero),
                 (n+'_shift',f'{n}_shr({one},{width})'), (n+'_shift_negative',f'{n}_shr({one},-1)'),
                 (n+'_shift_overflow',f'{n}_shl({maxv},1)')]
for name, expression in failures:
    exe=t.build(name,text='fn main(){println('+expression+');}')
    p=run([exe]);assert p.returncode==70 and 'arithmetic error' in p.stderr and not p.stdout,(name,p.returncode,p.stdout,p.stderr)
    message=p.stderr.split(': ',1)[-1].strip()
    for options in [[],['--optimize-ir']]:
        interpreted=run([t.compiler,'--run-ir',t.work/(name+'.on1x'),*options])
        assert interpreted.returncode==70 and not interpreted.stdout and message in interpreted.stderr,(name,options,interpreted.stderr)

for mode in ['--emit-ir','--run-ir']:
    p=run([t.compiler,mode,t.root/'examples/unsigned_f32.on1x'])
    assert p.returncode==1 and 'ON1X-MIR-UNSUPPORTED' in p.stderr,p.stderr
for mode in ['--check-memory','--check-types']:
    p=run([t.compiler,mode,t.root/'examples/unsigned_f32.on1x'])
    if mode=='--check-types':
        assert p.returncode==0 or (p.returncode==2 and 'unavailable' in p.stderr),p.stderr
    elif p.returncode!=2: # unavailable SDK is not exercised by default builds
        assert p.returncode!=0 and 'memory model' in p.stderr,p.stderr # closure path remains unsupported
checked=run([t.compiler,'--check-memory',t.work/'numeric_conversions.on1x'])
assert checked.returncode==0 or (checked.returncode==2 and 'unavailable' in checked.stderr),checked.stderr

environment=t.work/'bad_environment.c'
environment.write_text('#include <xmmintrin.h>\nvoid raw_bad_environment(void){_mm_setcsr(_mm_getcsr()|0x8000u);}\n')
exe=t.build('f32_environment',text='extern fn raw_bad_environment();fn main(){raw_bad_environment();println(f32_from_i64(1));}',extra=[environment])
p=run([exe]);assert p.returncode==70 and 'environment' in p.stderr and not p.stdout,p.stderr

# Optimization must not change the defined finite-width or binary32 results.
for name in ['unsigned_oracle','f32_oracle']:
    exe=t.work/(name+'_optimized')
    p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-O2','-I',t.root/'runtime',
           t.work/(name+'.c'),t.root/'runtime/on1x_runtime.c',*t.flags,'-o',exe])
    assert p.returncode==0,p.stderr
    baseline=run([t.work/name]);optimized=run([exe])
    assert optimized.returncode==0 and optimized.stdout==baseline.stdout,optimized.stderr

# Numeric v3 has a real independent evaluator and C11 emitter, not a widened
# signed carrier dialect. Reuse the exact integer/binary32 oracle outputs.
for name in ['unsigned_oracle','f32_oracle','numeric_conversions','f32_print']:
    baseline=run([t.work/name]);source=t.work/(name+'.on1x')
    for options in [[],['--optimize-ir']]:
        p=run([t.compiler,'--run-ir',source,*options])
        assert p.returncode==0 and p.stdout==baseline.stdout,(name,options,p.stderr)
        out=t.work/(name+('_opt' if options else '_mir')+'.c');exe=out.with_suffix('')
        p=run([t.compiler,'--emit-ir-c',source,*options,'-o',out]);assert p.returncode==0,p.stderr
        p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I',t.root/'runtime',out,
               t.root/'runtime/on1x_runtime.c',*t.flags,'-o',exe]);assert p.returncode==0,p.stderr
        p=run([exe]);assert p.returncode==0 and p.stdout==baseline.stdout,(name,p.stderr)

# Generated-code ABI versions are mandatory even for an older installed header.
for macro in ['ON1X_UNSIGNED_ABI_VERSION','ON1X_F32_ABI_VERSION']:
    include=t.work/macro;include.mkdir(exist_ok=True)
    (include/'on1x_runtime.h').write_text((t.root/'runtime/on1x_runtime.h').read_text().replace(
        '#define '+macro+' 1','#define '+macro+' 0'))
    p=run([t.cc,'-std=c11','-I',include,'-c',t.work/'unsigned_f32.c','-o',include/'mismatch.o'])
    assert p.returncode!=0 and 'runtime ABI' in p.stderr,p.stderr
print('Unsigned widths and binary32 HIR/C11, bit oracles, conversions, failures and composition PASS')
