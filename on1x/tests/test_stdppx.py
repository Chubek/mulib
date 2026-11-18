"""Execute all shipped processors against compiler ASTs and strict native C11."""
import json
import os
import sys
from support import NativeTests, run

t=NativeTests()
cases={
 'identity':('fn main(){println(42);}', '42\n'),
 'checks':('fn main(){let assert=fn(x:bool){tool_fail("shadow");};@checks("assert_eq",2,2);}', ''),
 'control':('fn main(){var n=0;@control("until",n==3,{n=n+1;});@control("while",n<4,{n=n+1;});println(n);}', '4\n'),
 'unroll':('fn main(){var n=0;@unroll("repeat",3,{let x=1;n=n+x;});println(n);}', '3\n'),
 'derive':('@derive("derive","arity","signature") fn f(x:i64)->i64{return x;}fn main(){println(f_arity());println(f_signature());}', '1\nfn(i64) -> i64\n'),
 'fold':('fn main(){println((20+1)*2);}', '42\n'),
 'pipe':('fn a(x:i64)->i64{println(1);return x+1;}fn b(x:i64)->i64{println(2);return x*2;}fn main(){println(@pipe("apply",20,a,b));}', '1\n2\n42\n'),
 'compose':('fn a(x:i64)->i64{return x+1;}fn b(x:i64)->i64{return x*2;}fn main(){println(@compose("apply",20,a,b));}', '41\n'),
 'cond':('fn main(){@cond("branches",false,{println(1);},true,{println(2);},{println(3);});}', '2\n'),
 'tap':('fn value()->i64{println(1);return 42;}fn main(){var callback=fn(x:i64){println(x);};println(@tap("call",value(),callback));}', '1\n42\n42\n'),
 'assertions':('fn value()->i64{println(1);return 42;}fn main(){@assertions("between",value(),40,44);}', '1\n'),
 'trace':('@trace("entry","work") fn f()->i64{return 42;}fn main(){println(f());}', '42\n'),
 'logging':('fn main(){@logging("info","hello");}', ''),
 'format':('fn value(s:string)->string{println(s);return s;}fn main(){println(@format("string","{0}+{1}={{ok}}",value("A"),value("B")));}', 'A\nB\nA+B={ok}\n'),
 'embed':('fn main(){io_stdout(@embed("text","asset.txt"));}', 'embedded\n'),
 'regex':('fn value()->string{println(1);return "abc";}fn main(){assert(@regex("prefix",value(),"ab"));assert(!@regex("suffix","a","long"));}', '1\n'),
 'contract':('@contract("requires",x>0) fn f(x:i64)->i64{return x;}fn main(){println(f(42));}', '42\n'),
 'match_int':('fn value()->i64{println(1);return 7;}fn main(){@match_int("cases",value(),7,{println(42);},{println(0);});}', '1\n42\n'),
 'state_machine':('fn main(){println(@state_machine("transition",0,1,0,1,42));}', '42\n'),
 'cfg':('@cfg("enabled","yes") fn f()->i64{return 42;}@cfg("disabled","yes") fn f()->i64{return 0;}fn main(){println(f());}', '42\n'),
 'test':('@test("case") fn works(){println(42);}fn main(){on1x_run_tests();}', '42\n'),
 'return_guard':('@return_guard("nonnegative") fn f()->i64{let nested=fn()->i64{return -1;};return -nested();}fn main(){println(f());}', '1\n'),
 'timing':('@timing("elapsed") fn f(x:bool){if x{return;}else{return;}}fn main(){f(true);f(false);}', ''),
 'ffi_bind':('@ffi_bind("raw_scalar") extern fn fixture_add(x:i64)->i64;fn main(){println(fixture_add(41));}', '42\n'),
}
(t.work/'asset.txt').write_text('embedded\n')
manifest=t.work/'ffi.json'
# Fixture metadata tests protocol admission only; it grants no production review.
manifest.write_text(json.dumps({'schema':'on1x-raw-ffi/v1','target':'test-host','functions':
                              {'fixture_add':{'parameters':['i64'],'return':'i64','reviewed':True}}}))
fixture=t.work/'ffi.c';fixture.write_text('#include <stdint.h>\nint64_t fixture_add(int64_t x){return x+1;}\n')
env={**os.environ,'ON1X_PPX_EMBED_ROOT':str(t.work),'ON1X_PPX_FEATURES':'yes',
     'ON1X_PPX_FFI_MANIFEST':str(manifest),'ON1X_PPX_FFI_TARGET':'test-host',
     'ON1X_PPX_SCHEMA':str(t.root/'grammar/PPX.schema.json')}
for processor,(text,expected) in cases.items():
    source=t.source(processor,text);c=t.work/(processor+'.c');exe=t.work/(processor+'.exe')
    p=run([t.compiler,'--emit-c',source,'--stdppx',processor,'-o',c],env=env)
    assert p.returncode == 0,(processor,p.stderr)
    extra=[]
    if processor=='timing':extra=[t.root/'stdppx/runtime/on1x_ppx_timing.c']
    if processor=='ffi_bind':extra=[fixture]
    p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I',t.root/'runtime',
           c,t.root/'runtime/on1x_runtime.c',*extra,*t.flags,'-o',exe])
    assert p.returncode == 0,(processor,p.stderr)
    p=run([exe]);assert p.returncode == 0 and p.stdout == expected,(processor,p.stdout,p.stderr)
    if processor=='timing':assert p.stderr.count('on1x timing')==2,p.stderr
    if processor=='trace':assert 'TRACE work' in p.stderr,p.stderr
    if processor=='logging':assert 'INFO: hello' in p.stderr,p.stderr

# Run the native equivalents as real compiler plugins, not just source checks.
for processor in ('pipe','compose','control','unroll'):
    exe=t.build('native_'+processor,source=t.root/'stdppx/native'/(processor+'.on1x'))
    p=run([t.compiler,'--emit-c',t.work/(processor+'.on1x'),'--ppx',processor,exe],env=env)
    assert p.returncode == 0,p.stderr
    assert p.stdout == (t.work/(processor+'.c')).read_text()

# The seven newly supplied native processors must execute the full common API,
# not merely compile. Compare observable values/effects with Python references.
for processor in ('assertions','cond','format','logging','state_machine','tap','trace'):
    plugin=t.build('native_'+processor,source=t.root/'stdppx/native'/(processor+'.on1x'))
    c=t.work/('native_client_'+processor+'.c');exe=t.work/('native_client_'+processor)
    p=run([t.compiler,'--emit-c',t.work/(processor+'.on1x'),'--ppx',processor,plugin,'-o',c],env=env)
    assert p.returncode==0,(processor,p.stderr)
    p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I',t.root/'runtime',c,t.root/'runtime/on1x_runtime.c',*t.flags,'-o',exe])
    assert p.returncode==0,(processor,p.stderr)
    actual=run([exe]);reference=run([t.work/(processor+'.exe')])
    assert (actual.returncode,actual.stdout,actual.stderr)==(reference.returncode,reference.stdout,reference.stderr),(processor,actual,reference)
    bad=t.source('native_bad_'+processor,'fn main(){@'+processor+'("unknown");}')
    p=run([t.compiler,'--emit-c',bad,'--ppx',processor,plugin],env=env)
    assert p.returncode!=0 and not p.stdout,(processor,p.stderr)
    protocol={**env,'ON1X_PPX_PROTOCOL':'1','ON1X_PPX_LABEL':processor}
    p=run([plugin],input='(Unit 1 ())',env=protocol)
    assert p.returncode==0,(processor,p.stderr)
    p=run([plugin],input='(Unit 1 (',env=protocol)
    assert p.returncode!=0 and not p.stdout,(processor,p.stderr)
    wrong=t.source('unconsumed_'+processor,'fn main(){println(@'+processor+'("unknown"));}')
    ast=run([t.compiler,'--emit-ast',wrong]);assert ast.returncode==0,ast.stderr
    p=run([plugin],input=ast.stdout,env=protocol)
    assert p.returncode!=0 and not p.stdout,(processor,p.stderr)

for processor,source,expected in [
 ('cond','fn main(){@cond("choose",true,{println(42);},{println(0);});}','42\n'),
 ('tap','fn main(){println(@tap("identity",42));}','42\n'),
 ('format','fn main(){println(@format("concat","a","b"));}','ab\n'),
 ('logging','fn main(){@logging("print",42);}','42\n'),
 ('state_machine','fn main(){var x=0;@state_machine("set",x,42);println(x);}','42\n'),
 ('assertions','fn main(){let assert=fn(x:bool){tool_fail("shadow");};@assertions("assert_true",true);@assertions("assert_false",false);@assertions("assert_eq",1,1);@assertions("assert_ne",1,2);}',''),
 ('assertions','fn item(x:i64)->i64{println(x);return x;}fn main(){@assertions("between",item(2),item(1),item(3));}','2\n1\n3\n'),
 ('state_machine','fn item(x:i64)->i64{println(x);return x;}fn main(){println(@state_machine("transition",item(7),item(8),7,8,42,1,2,3));}','7\n8\n42\n'),
 ('cond','fn test(x:i64)->bool{println(x);return x==2;}fn main(){@cond("branches",test(1),{println(0);},test(2),{println(42);},test(3),{println(0);},{println(0);});}','1\n2\n42\n'),
]:
    path=t.source('alias_'+processor,source)
    for kind,flags in [('python',['--stdppx',processor]),('native',['--ppx',processor,t.work/('native_'+processor)])]:
        c=t.work/'alias.c';exe=t.work/'alias'
        p=run([t.compiler,'--emit-c',path,*flags,'-o',c],env=env);assert p.returncode==0,(processor,kind,p.stderr)
        p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I',t.root/'runtime',c,t.root/'runtime/on1x_runtime.c',*t.flags,'-o',exe]);assert p.returncode==0,p.stderr
        p=run([exe]);assert p.returncode==0 and p.stdout==expected,(processor,kind,p.stdout,p.stderr)

for processor,source,diagnostic in [
 ('assertions','fn main(){@assertions("between",1,2);}','assertions'),
 ('format','fn main(){println(@format("string","{0}{0}","x"));}','format'),
 ('state_machine','fn main(){println(@state_machine("transition",0,0,1,2,3,1,2,4));}','duplicate'),
 ('trace','@trace("entry",42) fn main(){}','string literal'),
 ('cond','fn main(){@cond("branches",true,42,{println(0);});}','Block'),
]:
    path=t.source('bad_native_common_'+processor,source)
    for flags in [['--stdppx',processor],['--ppx',processor,t.work/('native_'+processor)]]:
        p=run([t.compiler,'--emit-c',path,*flags],env=env)
        assert p.returncode!=0 and not p.stdout and diagnostic in p.stderr,(processor,p.stderr)

for kind in ['python','native']:
    args=[]
    for processor in ['trace','tap','assertions','cond','format','logging']:
        args += ['--stdppx',processor] if kind=='python' else ['--ppx',processor,t.work/('native_'+processor)]
    exe=t.work/('combined_'+kind)
    p=run([sys.executable,t.root/'tools/on1x-build.py',t.root/'stdppx/examples/native_extensions.on1x',
           '--on1xc',t.compiler,'--cc',t.cc,*args,'-o',exe,*['--cflag='+flag for flag in t.flags]],env=env)
    assert p.returncode==0,(kind,p.stderr)
    p=run([exe]);assert p.returncode==0 and p.stdout=='42\nanswer: 42\n' and p.stderr=='TRACE example\nINFO: done\n',(kind,p.stdout,p.stderr)

clock=t.work/'clock_bounds'
p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',
       t.root/'tests/test_ppx_clock.c',*t.flags,'-o',clock])
assert p.returncode == 0,p.stderr
for mode in range(8):
    p=run([clock,mode])
    if mode in (0,7):
        assert p.returncode == 0,(mode,p.stderr)
    else:
        diagnostic='clock failed' if mode==6 else 'nanosecond time out of range'
        assert p.returncode == 73 and diagnostic in p.stderr,(mode,p.returncode,p.stderr)

for processor,text,diagnostic in [
 ('unroll','fn main(){@unroll("repeat",256,{@unroll("repeat",256,{println(1);});});}','node limit'),
 ('assertions','fn main(){@assertions("between",1,2);}','between'),
 ('format','fn main(){@format("string","{0} {0}","x");}','occur once'),
 ('contract','@contract("requires",42) fn f(){}fn main(){f();}','bool'),
 ('pipe','fn main(){@pipe("apply",42,1);}','identifiers'),
]:
    path=t.source('bad_'+processor,text)
    p=run([t.compiler,'--emit-c',path,'--stdppx',processor],env=env)
    assert p.returncode != 0 and not p.stdout and diagnostic in p.stderr,(processor,p.stderr)
for example,expected in [('transformations','Literal text expansion\n4\n'),('instrumentation','25\n'),
                         ('feature_tests','build succeeded\n'),('embed','Hello from an embedded On1x resource!\n\n')]:
    source=t.root/'stdppx/examples'/(example+'.on1x');exe=t.work/('example_'+example)
    environment={**env,'ON1X_PPX_EMBED_ROOT':str(t.root/'stdppx/examples'),'ON1X_PPX_FEATURES':'test_suite'}
    p=run([sys.executable,t.root/'tools/on1x-build.py',source,'--on1xc',t.compiler,'--cc',t.cc,
           '--allow-source-ppx','-o',exe,*[f'--cflag={flag}' for flag in t.flags]],env=environment)
    assert p.returncode == 0,(example,p.stderr)
    p=run([exe]);assert p.returncode == 0 and p.stdout == expected,(example,p.stdout,p.stderr)
print('24 Python PPXs compiled/executed; native control/pipe/compose/unroll parity; clock bounds and bounded failures: PASS')
