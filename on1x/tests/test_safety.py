#!/usr/bin/env python3
"""Chisel HIR admission and actual MuLIB C11 execution."""
import sys
from support import NativeTests, run
t = NativeTests()
source = t.root/'examples/memory.on1x'
report = run([t.compiler, '--check-memory', source])
assert report.returncode == 0 and 'on1x.memory/v1' in report.stdout, report.stderr
assert report.stdout == run([t.compiler, '--check-memory', source]).stdout
f64_source=t.source('f64_memory','fn half(x:f64)->f64{return x/f64_from_i64(2);}fn main(){println(half(3.0));}')
p=run([t.compiler,'--check-memory',f64_source])
assert p.returncode==0 and 'on1x.memory/v1' in p.stdout,p.stderr
code = t.work/'checked.c'
p = run([t.compiler, '--emit-c', '--memory-check', source, '-o', code])
assert p.returncode == 0, p.stderr
assert code.read_text() == run([t.compiler, '--emit-c', source]).stdout
exe = t.work/'memory'
p = run([sys.executable, t.root/'tools/on1x-build.py', source, '--memory-check',
         '--on1xc', t.compiler, '--cc', t.cc, '-o', exe, *['--cflag='+f for f in t.flags]])
assert p.returncode == 0, p.stderr
p = run([exe]);assert p.returncode == 0 and p.stdout == '2\n1\n3\n3\n3\n', p.stderr
for name, text, diagnostic in [
    ('extern', 'extern fn opaque(); fn main(){opaque();}', 'raw external'),
    ('closure', 'fn main(){let f=fn()->i64{return 1;};println(f());}', 'memory model unavailable'),
    ('indirect', 'fn f()->i64{return 1;}fn main(){let g=f;println(g());}', 'first-class callable'),
    ('temporary', 'fn main(){println(json_size(json_parse_or_die("{}")));}', 'named live owner'),
    ('buffer', 'fn main(){let b=buffer_new();}', 'intrinsic memory model unavailable'),
]:
    path=t.source(name,text);code.write_text('keep')
    p=run([t.compiler,'--emit-c','--memory-check',path,'-o',code])
    assert p.returncode == 1 and diagnostic in p.stderr and code.read_text() == 'keep', (name,p.stderr)

# Reviewed scalar-only extern evidence is an explicit, bounded exception to the
# raw-extern rejection. The ABI declared by the source must match exactly.
foreign_name='on1x_ffi_'+'0'*64+'_admitted'
foreign=t.source('admitted_foreign','extern fn '+foreign_name+'(x:i64)->i64;fn main(){println('+foreign_name+'(7));}')
admission=t.work/'admitted-extern.txt'
admission.write_text('on1x.admitted-extern/v1\nidentity '+'0'*64+'\nextern '+foreign_name+' (i64) -> i64\n')
p=run([t.compiler,'--check-memory','--admitted-extern',admission,foreign])
assert p.returncode==0 and 'admitted raw extern (reviewed scalar-only evidence)' in p.stdout,p.stderr
p=run([t.compiler,'--check', '--admitted-extern',admission,foreign])
assert p.returncode==2 and '--admitted-extern requires' in p.stderr,p.stderr
for name,wire,diagnostic in [
    ('bad_schema','on1x.admitted-extern/v999\nidentity '+'0'*64+'\nextern '+foreign_name+' (i64) -> i64\n','unknown admission schema'),
    ('bad_result','on1x.admitted-extern/v1\nidentity '+'0'*64+'\nextern '+foreign_name+' (i64) -> void\n','admitted extern result drift'),
    ('bad_parameter','on1x.admitted-extern/v2\nidentity '+'0'*64+'\nextern '+foreign_name+' (bool) -> i64\n','admitted extern parameter type drift'),
    ('trailing_comma','on1x.admitted-extern/v2\nidentity '+'0'*64+'\nextern '+foreign_name+' (i64,) -> i64\n','trailing comma'),
    ('v1_bool','on1x.admitted-extern/v1\nidentity '+'0'*64+'\nextern '+foreign_name+' (bool) -> i64\n','unsupported scalar type'),
    ('v1_float','on1x.admitted-extern/v1\nidentity '+'0'*64+'\nextern '+foreign_name+' (i64) -> f64\n','unsupported scalar type'),
    ('v2_f32','on1x.admitted-extern/v2\nidentity '+'0'*64+'\nextern '+foreign_name+' (f32) -> i64\n','unsupported scalar type'),
    ('v2_unsigned','on1x.admitted-extern/v2\nidentity '+'0'*64+'\nextern '+foreign_name+' (u64) -> i64\n','unsupported scalar type'),
    ('v2_void_parameter','on1x.admitted-extern/v2\nidentity '+'0'*64+'\nextern '+foreign_name+' (void) -> i64\n','unsupported scalar type'),
    ('extra','on1x.admitted-extern/v1\nidentity '+'0'*64+'\nextern on1x_ffi_'+'0'*64+'_absent () -> void\n','absent external declaration'),
]:
    evidence=t.work/(name+'.txt');evidence.write_text(wire)
    p=run([t.compiler,'--check-memory','--admitted-extern',evidence,foreign])
    assert p.returncode==1 and diagnostic in p.stderr,(name,p.stderr)
typed_name='on1x_ffi_'+'1'*64+'_typed'
typed=t.source('admitted_typed','extern fn '+typed_name+'(x:bool,y:f64,z:i64)->f64;'
               'fn main(){println('+typed_name+'(true,1.5,2));}')
typed_wire='on1x.admitted-extern/v2\nidentity '+'1'*64+'\nextern '+typed_name+' (bool,f64,i64) -> f64\n'
admission.write_text(typed_wire)
p=run([t.compiler,'--check-memory','--admitted-extern',admission,typed])
assert p.returncode==0 and 'admitted raw extern' in p.stdout,p.stderr
for before,after,diagnostic in [('bool,f64,i64','f64,bool,i64','parameter type drift'),
                                ('-> f64','-> bool','result drift')]:
    admission.write_text(typed_wire.replace(before,after))
    code.write_text('keep')
    p=run([t.compiler,'--emit-c','--memory-check','--admitted-extern',admission,typed,'-o',code])
    assert p.returncode==1 and diagnostic in p.stderr and code.read_text()=='keep',p.stderr
extended=t.source('admitted_extended','extern fn '+typed_name+'(a:u8,b:u16,c:u32,d:u64,x:f32)->f32;'
                  'fn main(){println('+typed_name+'(u8_from_i64(1),u16_from_i64(1),u32_from_i64(1),u64_from_i64(1),f32_from_i64(1)));}')
extended_wire='on1x.admitted-extern/v3\nidentity '+'1'*64+'\nextern '+typed_name+' (u8,u16,u32,u64,f32) -> f32\n'
admission.write_text(extended_wire)
p=run([t.compiler,'--check-memory','--admitted-extern',admission,extended])
assert p.returncode==0,p.stderr
for old_schema in ['v1','v2']:
    admission.write_text(extended_wire.replace('v3',old_schema,1))
    p=run([t.compiler,'--check-memory','--admitted-extern',admission,extended])
    assert p.returncode==1 and 'unsupported scalar type' in p.stderr,p.stderr
for options in [['--tokens'],['--emit-ast'],['--ir-input','--emit-ir']]:
    p=run([t.compiler,*options,'--memory-check',source]);assert p.returncode==2,p.stderr
for name,text in [('numeric_tuple','fn pair(x:u64)->(u64,f32){return(x,1.5f32);}fn main(){let(a,b)=pair(42u64);println(a);println(b);}'),
                   ('numeric_parameter_pattern','fn take((a,b):(u8,(f32,u64)))->u64{println(a);return a64(b);}fn a64((x,y):(f32,u64))->u64{println(x);return y;}fn main(){println(take((1u8,(2f32,3u64))));}')]:
    p=run([t.compiler,'--check-memory',t.source(name,text)])
    assert p.returncode==0,p.stderr
mixed=t.source('unsupported_tuple_memory','fn main(){let mixed=(1u8,"text");}')
p=run([t.compiler,'--check-memory',mixed])
assert p.returncode==1 and 'tuple memory model' in p.stderr,p.stderr
print('Chisel checked HIR, deterministic admission, output preservation and real MuLIB execution PASS')
