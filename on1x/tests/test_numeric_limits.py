#!/usr/bin/env python3
"""Suffixed literals, scalar MIR v3 and independent numeric HIR obligations."""
import os
from support import NativeTests, run

t=NativeTests()
expected='18446744073709551615\n255\n0x1.800000p+1\n0\n1\n2\n1065353217\n'
t.valid('numeric_literals',expected)
source=t.root/'examples/numeric_literals.on1x'

def differential(path,expected):
    wire=run([t.compiler,'--emit-ir',path])
    assert wire.returncode==0 and wire.stdout.startswith('(on1x.mir 3 '),wire.stderr
    saved=t.work/(path.stem+'.mir');saved.write_text(wire.stdout)
    printed=run([t.compiler,'--ir-input','--emit-ir',saved])
    assert printed.returncode==0 and printed.stdout==wire.stdout,printed.stderr
    for options in [[],['--optimize-ir']]:
        p=run([t.compiler,'--ir-input','--run-ir',saved,*options])
        assert p.returncode==0 and p.stdout==expected,p.stderr
        out=t.work/(path.stem+('_opt' if options else '_mir')+'.c');exe=out.with_suffix('')
        p=run([t.compiler,'--ir-input','--emit-ir-c',saved,*options,'-o',out]);assert p.returncode==0,p.stderr
        p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I',t.root/'runtime',out,
               t.root/'runtime/on1x_runtime.c',*t.flags,'-o',exe]);assert p.returncode==0,p.stderr
        p=run([exe]);assert p.returncode==0 and p.stdout==expected,p.stderr
    return wire.stdout

wire=differential(source,expected)
for mode in ['--check-types','--check-memory']:
    p=run([t.compiler,mode,source])
    assert p.returncode==0 or (p.returncode==2 and 'unavailable' in p.stderr),p.stderr
    if p.returncode==2:print(mode+': UNSUPPORTED (optional SDK unavailable)')

# The compiler, Python PPX and native PPX all preserve typed literal categories.
for processor in [t.root/'stdppx/identity.py',t.compiler.parent/'stdppx/on1x-ppx-identity',
                  t.root/'stdppx/fold.py',t.compiler.parent/'stdppx/on1x-ppx-fold']:
    if not processor.exists():
        print(str(processor)+': UNSUPPORTED (processor not built)')
        continue
    out=t.work/('literal_ppx_'+processor.name+'.c')
    environment=dict(os.environ,ON1X_PPX_SCHEMA=str((t.root/'grammar/PPX.schema.json').resolve()))
    p=run([t.compiler,'--emit-c',source,'--ppx','numeric',processor,'-o',out],env=environment);assert p.returncode==0,p.stderr
    exe=out.with_suffix('')
    p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I',t.root/'runtime',out,
           t.root/'runtime/on1x_runtime.c',*t.flags,'-o',exe]);assert p.returncode==0,p.stderr
    p=run([exe]);assert p.returncode==0 and p.stdout==expected,p.stderr

# Full intrinsic descriptor coverage, all numeric conversion directions and
# boundary-bearing values execute through HIR, evaluator and both MIR C paths.
body=[]
for width in [8,16,32,64]:
    n=f'u{width}'
    for operation in ['wrapping_add','wrapping_sub','wrapping_mul','and','or','xor']:
        body.append(f'println({n}_{operation}(3{n},2{n}));')
    for operation in ['shl','shr']:body.append(f'println({n}_{operation}(3{n},1));')
    body.append(f'println({n}_not(3{n}));')
    for other in [8,16,32,64]:body.append(f'println({n}_from_u{other}(42u{other}));')
    for f in ['f32','f64']:
        body += [f'println({f}_from_{n}(42{n}));',f'println({n}_from_{f}(42.75{f}));']
    body += [f'println({n}_to_i64(42{n}));',f'println({n}_to_bits({n}_from_bits(-1)));']
    for operation in ['+','-','*','/','%','==','!=','<','<=','>','>=']:
        body.append(f'println(3{n}{operation}2{n});')
for f in ['f32','f64']:
    for op in ['from_i64','from_bits']:body.append(f'println({f}_to_bits({f}_{op}(0)));')
    for op in ['is_nan','is_finite','signbit']:body.append(f'println({f}_{op}(-0.0{f}));')
    body.append(f'println({f}_to_i64(42.75{f}));')
body += ['println(f32_from_f64(1.5));','println(f64_from_f32(1.5f32));','println(-1.5f32);']
full=t.source('numeric_descriptors','fn main(){'+''.join(body)+'}')
baseline=t.build('numeric_descriptors',source=full);p=run([baseline]);assert p.returncode==0,p.stderr
wire=differential(full,p.stdout)

# Syntax categories must agree with PPX: unsigned literals are numeric, not
# legacy signed-i64 IntegerLiteral values. Literal rules compare type + value.
t.valid('numeric_macro_categories','unsigned\nfloat\ninteger\nunsigned\n255\n',text='''
macro kind {
  rule($x:IntegerLiteral)=>expression{"integer"};
  rule($x:UnsignedLiteral)=>expression{"unsigned"};
  rule($x:FloatLiteral)=>expression{"float"};
}
macro datum {
  rule(255)=>expression{"signed"};
  rule(0xffu8)=>expression{"unsigned"};
}
macro numeric { rule($x:NumericLiteral)=>expression{$x}; }
fn main(){println(kind!(255u8));println(kind!(1.5f32));println(kind!(42));
  println(datum!(255u8));println(numeric!(0xffu8));}
''')
for name,rule,expression in [
    ('unsigned_not_legacy','rule($x:IntegerLiteral)=>expression{$x};','1u8'),
    ('unsigned_datum_width','rule(255u8)=>expression{42};','255u16'),
    ('unsigned_datum_signed','rule(255u8)=>expression{42};','255')]:
    bad=t.source(name,'macro only{'+rule+'}fn main(){println(only!('+expression+'));}')
    p=run([t.compiler,'--emit-c',bad])
    assert p.returncode==1 and 'no typed rule' in p.stderr and not p.stdout,p.stderr

for name,changed in [('old_version',wire.replace('(on1x.mir 3 ','(on1x.mir 2 ',1)),
                     ('unknown_descriptor',wire.replace('"u8_wrapping_add"','"raw_foreign"',1)),
                     ('wrong_type',wire.replace('"u8_wrapping_add"','"u64_wrapping_add"',1)),
                     ('oversized_descriptor',wire.replace('"u8_wrapping_add"','"'+'x'*65+'"',1))]:
    assert changed!=wire,name
    saved=t.work/(name+'.mir');saved.write_text(changed)
    p=run([t.compiler,'--ir-input','--run-ir',saved])
    assert p.returncode==1 and 'ON1X-MIR-' in p.stderr and not p.stdout,(name,p.stderr)

for name,expression in [('u8_overflow','256u8'),('u16_overflow','65536u16'),
                        ('u32_overflow','4294967296u32'),('u64_overflow','18446744073709551616u64'),
                        ('negative_unsigned','-1u8'),('wrong_suffix','1u128'),('wrong_float_suffix','1.5f16'),
                        ('f32_overflow','1e39f32'),('f32_underflow','1e-9999f32'),
                        ('float_unsigned_suffix','1.5u8'),('float_hex_suffix','0x1.0f32'),
                        ('unsigned_oversized','0'*1023+'u8'),('f32_oversized','0'*1022+'f32')]:
    bad=t.source(name,'fn main(){println('+expression+');}')
    for mode in ['--emit-c','--emit-ir','--run-ir']:
        out=t.work/'preserve.c';out.write_text('preserve')
        p=run([t.compiler,mode,bad,'-o',out])
        assert p.returncode==1 and 'error:' in p.stderr and not p.stdout and out.read_text()=='preserve',(name,mode,p.stderr)

t.valid('hex_suffix_compat','7986\n3940\n',text='fn main(){println(0x1f32);println(0xf64);}')
t.valid('typed_literal_budget','0\n0x0.000000p+0\n',text='fn main(){println('+'0'*1022+'u8);println('+'0'*1021+'f32);}')
print('Suffixed numeric literals, scalar MIR v3 and available checker/PPX profiles PASS')
