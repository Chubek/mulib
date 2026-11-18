#!/usr/bin/env python3
"""Category concrete evidence: native On1x PPX, checked core and closed boundaries."""
import os
import hashlib
import copy
from pathlib import Path
import sys
import tempfile
from support import NativeTests, run

t=NativeTests()
example=t.root/'stdppx/examples/category/eq.on1x'
processor=t.compiler.parent/'stdppx/on1x-ppx-protocols'
cache=t.compiler.parent/'CMakeCache.txt'
disabled=cache.exists() and 'ON1X_BUILD_NATIVE_PPX:BOOL=OFF' in cache.read_text()
if disabled:
    p=run([t.compiler,'--check',example])
    assert p.returncode and 'ON1X_BUILD_NATIVE_PPX=OFF' in p.stderr,p.stderr
    print('Category native execution: UNSUPPORTED (ON1X_BUILD_NATIVE_PPX=OFF)')
    sys.exit(77)
assert processor.exists(),'Configured native Category processor has not been built'

def compiled(name,source,expected,options=(),via=False):
    out=t.work/(name+'.c');exe=t.work/name
    p=run([t.compiler,'--emit-ir-c' if via else '--emit-c',source,*options,'-o',out]);assert p.returncode==0,p.stderr
    p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I',t.root/'runtime',out,
           t.root/'runtime/on1x_runtime.c',*t.flags,'-o',exe]);assert p.returncode==0,p.stderr
    p=run([exe]);assert p.returncode==0 and p.stdout==expected,p.stderr
    return out.read_text()

def rejected(name,source,diagnostic,options=()):
    path=t.source(name,source);out=t.work/'preserve.c';out.write_text('preserved')
    p=run([t.compiler,'--emit-c',path,*options,'-o',out])
    assert p.returncode!=0 and diagnostic in p.stderr and not p.stdout and out.read_text()=='preserved',(name,p.stderr)
    return p

compiled('eq',example,'true\nfalse\n')
compiled('ordering',t.root/'stdppx/examples/category/ordering.on1x','-1\n1\n')
compiled('evidence',t.root/'stdppx/examples/category/evidence.on1x','9\n3\n3\n9\ntrue\n7\n')
compiled('mir',example,'true\nfalse\n',via=True)
compiled('ordinary_ppx',example,'true\nfalse\n',options=['--stdppx','identity'])
rejected_label=run([t.compiler,'--check',example,'--ppx','Protocols',t.root/'stdppx/identity.py'])
assert rejected_label.returncode and 'conflicts with active bundled extension' in rejected_label.stderr,rejected_label.stderr
mutator=t.work/'erase_interface.py'
mutator.write_text('#!'+sys.executable+'\nimport sys\nsys.path.insert(0,'+repr(str(t.root/'stdlib'))+')\n'
                   'from sexpr import Atom as A,loads,dumps\nt=loads(sys.stdin.read())\n'
                   'for f in t[2]:\n'
                   ' if str(f[1]).startswith("on1x_extension_") or f[1] in ("Eq_i64_eq","Different_eq"):\n'
                   '  f[3]=[A("I64Type")]\n'
                   '  f[4]=[A("Block"),[[A("Return"),[A("DecimalLiteral"),"42",f[-1]],f[-1]]],f[-1]]\n'
                   'print(dumps(t))\n')
mutator.chmod(0o755)
p=run([t.compiler,'--check',example,'--ppx','mutator',mutator])
assert p.returncode and 'ON1X-EXT-INTERFACE' in p.stderr and not p.stdout,p.stderr
redirect=t.work/'redirect_evidence.py'
redirect.write_text('#!'+sys.executable+'\nimport sys\nsys.path.insert(0,'+repr(str(t.root/'stdlib'))+')\n'
                   'from sexpr import loads,dumps\nt=loads(sys.stdin.read())\n'
                   'for f in t[2]:\n'
                   ' if f[1]=="Eq_i64_eq": f[4][1][0][1][1][1]="Different_eq"\n'
                   'print(dumps(t))\n')
redirect.chmod(0o755)
rejected('redirect_evidence',example.read_text(),'selector no longer forwards',
         ['--ppx','redirect',redirect])
for mode in ['--check-types','--check-memory']:
    p=run([t.compiler,mode,example])
    assert p.returncode==0 or (p.returncode==2 and 'unavailable' in p.stderr),p.stderr
    if p.returncode==2:print(mode+': UNSUPPORTED (optional SDK unavailable)')
source=example.read_text();parity=t.source('activation',source)
first=compiled('source_enabled',parity,'true\nfalse\n')
parity.write_text(source.replace('@enable Protocols','',1))
second=compiled('cli_enabled',parity,'true\nfalse\n',['--enable','Protocols,Protocols','--enable','Protocols'])
assert first==second,'CLI/directive generated code drift'

declarations='''
protocol Eq<Self>{fn eq(a:Self,b:Self)->bool;}
impl Eq for i64{fn eq(a:i64,b:i64)->bool{return a==b;}}
'''
def client(inner,main='println(Eq_i64_eq(1,1));'):
    return '@enable Protocols\n@extension Protocols{'+inner+'}\nfn main(){'+main+'}'

cases=[
    ('unknown_feature','@enable Protocolz\nfn main(){}','ON1X-CAT-ENABLE'),
    ('unsupported_feature','@enable Functors\nfn main(){}','unimplemented'),
    ('disabled',client(declarations),'explicitly disabled'),
    ('missing_enable','@extension Protocols{'+declarations+'}fn main(){}','requires @enable'),
    ('late_enable',client(declarations)+'\n@enable Protocols','late @enable'),
    ('nested_enable','fn main(){@enable Protocols}','expected'),
    ('quoted_enable','fn main(){let x=quote{@enable Protocols};}','expected'),
    ('unbalanced','@enable Protocols\n@extension Protocols{protocol P<Self>{','unterminated'),
    ('wrong_delimiter','@enable Protocols\n@extension Protocols{ ( ] }','mismatched'),
    ('wrong_signature',client(declarations.replace('fn eq(a:i64','fn eq(a:bool')),'member parameter type mismatch'),
    ('wrong_result',client(declarations.replace('->bool{return','->i64{return')),'member result type mismatch'),
    ('bad_body',client(declarations.replace('return a==b;','return 42;')),'expected bool, got i64'),
    ('bad_call',client(declarations,'println(Eq_i64_eq(true,false));'),'expected i64, got bool'),
    ('missing_member',client(declarations.replace('fn eq(a:i64,b:i64)->bool{return a==b;}','fn other(a:i64,b:i64)->bool{return true;}')),'unexpected protocol member'),
    ('duplicate_protocol',client(declarations+declarations),'duplicate nominal protocol'),
    ('duplicate_parameter',client('protocol P<Self>{fn f(x:Self,x:Self)->bool;}',''),'duplicate parameter name'),
    ('duplicate_member',client('protocol P<Self>{fn f(x:Self)->bool;fn f(y:Self)->bool;}',''),'duplicate member'),
    ('duplicate_impl',client(declarations+'impl Eq for i64{fn eq(a:i64,b:i64)->bool{return true;}}'),'ON1X-CAT-COHERENCE'),
    ('orphan',client('impl Foreign for i64{fn eq(a:i64,b:i64)->bool{return true;}}'),'not defined in this module'),
    ('associated',client('protocol P<Self>{type Item;}'),'expected fn'),
    ('inheritance',client('protocol P<Self>:Eq<Self>{fn eq(a:Self)->bool;}'),'expected {'),
    ('generic_constraint',client('fn minimum<T:Ord>(a:T)->T{return a;}'),'only protocol/impl/witness'),
    ('unknown_body',client(declarations.replace('return a==b;','return unchecked(a,b);')),'unknown'),
    ('collision',client(declarations)+'fn Eq_i64_eq(a:i64,b:i64)->bool{return true;}','selector collision'),
]
for name,text,error in cases:
    rejected(name,text,error,['--disable','Protocols'] if name=='disabled' else [])
for name in ['../../evil','protocols','Protocols,','Traits']:
    p=run([t.compiler,'--enable',name,'--check',example]);assert p.returncode and 'ON1X-CAT-ENABLE' in p.stderr,p.stderr
p=run([t.compiler,'--enable','Protocols','--disable','Protocols','--check',example]);assert p.returncode and 'conflict' in p.stderr,p.stderr
rejected('source_ppx',source+'\n@ppx arbitrary "./malicious"','--allow-source-ppx')
rejected('comment_inert','# @enable Protocols\n@extension Protocols{}fn main(){}','requires @enable')
rejected('string_inert','fn main(){println("@enable Protocols");}\n@extension Protocols{}','requires @enable')

# Declaration order cannot select a different canonical witness.
reordered='impl Eq for i64{fn eq(a:i64,b:i64)->bool{return a==b;}}protocol Eq<Self>{fn eq(a:Self,b:Self)->bool;}'
compiled('reordered',t.source('reordered',client(reordered)),'true\n')
compiled('dedup',t.source('dedup','@enable Protocols;\n'+source),'true\nfalse\n')
compiled('ordinary_identifiers',t.source('ordinary_identifiers','fn protocol(impl:i64)->i64{return impl;}fn main(){println(protocol(42));}'),'42\n')
compiled('macro_coexist',t.source('macro_coexist',client(declarations,'println(same!(1,1));')+
         'macro same{rule($x:Expression,$y:Expression)=>expression{Eq_i64_eq($x,$y)};}'),'true\n')
compiled('scalar_targets',t.source('scalar_targets',client('''
protocol Identity<Self>{fn id(x:Self)->Self;}
impl Identity for u64{fn id(x:u64)->u64{return x;}}
impl Identity for f32{fn id(x:f32)->f32{return x;}}
impl Identity for bool{fn id(x:bool)->bool{return x;}}
''','println(Identity_u64_id(18446744073709551615u64));println(Identity_f32_id(1.5f32));println(Identity_bool_id(true));')),
         '18446744073709551615\n0x1.800000p+0\ntrue\n')

# The reusable parser API is itself compiled/executed independently of the PPX.
api=t.source('authoring_api','''import "'''+str(t.root/'stdlib/ppx/category/syntax.on1x')+'''";
fn token(text:string)->sexpr{let at=ppx_node("Loc",sx_cons(sx_string("api"),ppx_list3(sx_symbol("0"),sx_symbol("1"),sx_symbol("1"))));return ppx_node("SourceToken",ppx_list2(sx_string(text),at));}
fn main(){let tokens=ppx_list4(token("protocol"),token("P"),token("<"),token("Self"));
 let tail=ppx_list4(token(">"),token("{"),token("fn"),token("ok"));
 let signature=ppx_list4(token("("),token("x"),token(":"),token("Self"));
 let result=ppx_list4(token(")"),token("->"),token("bool"),token(";"));
 var all=ppx_list2(token("}"),token("<eof>"));
 let groups=ppx_list4(tokens,tail,signature,result);var rev=sexpr_reverse(groups);
 while sx_kind(rev)==3{var pieces=sexpr_reverse(sx_head(rev));while sx_kind(pieces)==3{all=sx_cons(sx_head(pieces),all);pieces=sx_tail(pieces);}rev=sx_tail(rev);}
 let (protocol,end)=ppx_category_parse_protocol(all);assert(visitor_tag(protocol)=="Protocol");assert(sx_text(sexpr_nth(protocol,1))=="P");assert(ppx_peek(end)=="<eof>");println("authoring API OK");}
''')
compiled('authoring_api',api,'authoring API OK\n')

# Imported source enables only itself. Exports here are checked ordinary scalar
# functions; protocol/witness manifests are deliberately not imported as evidence.
module=t.source('library',client(declarations,'').replace('fn main(){}','fn answer()->bool{return Eq_i64_eq(1,1);}'))
consumer=t.source('consumer','import library as A;import library as B;fn main(){println(A.answer());println(B.answer());}')
compiled('module_client',consumer,'true\ntrue\n')
left=t.source('flat_left',client(declarations,'').replace('fn main(){}','fn left_answer()->bool{return Eq_i64_eq(1,1);}'))
right=t.source('flat_right',client(declarations.replace('Eq','Equal'),'').replace('fn main(){}','fn right_answer()->bool{return Equal_i64_eq(1,1);}'))
flat=t.source('flat_category','import "flat_left.on1x";import "flat_right.on1x";fn main(){println(left_answer());println(right_answer());}')
compiled('flat_category',flat,'true\ntrue\n')
rejected('module_no_activation','import library as A;@extension Protocols{}fn main(){}','requires @enable')
p=run([t.compiler,'--disable','Protocols','--check',consumer]);assert p.returncode and 'explicitly disabled' in p.stderr,p.stderr
t.source('bad_import','@enable Misspelled\nfn unused(){}')
rejected('unused_bad_import','import bad_import;fn main(){}','ON1X-CAT-ENABLE')
p=run([t.compiler,'--deps',consumer]);assert p.returncode==0 and str(module.resolve()) in p.stdout,p.stderr
p=run([t.compiler,'--emit-ast',example]);assert p.returncode and 'inspection does not execute' in p.stderr,p.stderr

# Environment must not select a PPX executable or schema. No PATH lookup; no
# Python interpreter is needed to execute the bundled processor.
hostile=dict(os.environ,ON1X_PPX_SCHEMA='/does/not/exist',ON1X_PPX_LABEL='evil',ON1X_PPX_PROTOCOL='evil',
             ON1X_EXTENSION_PHASE='evil',PATH='/does/not/exist')
p=run([t.compiler,'--check',example],env=hostile);assert p.returncode==0,p.stderr
p=run([t.compiler,'--list-extensions']);assert p.returncode==0 and 'Protocols:' in p.stdout and 'Functors: UNAVAILABLE' in p.stdout and str(processor) in p.stdout,p.stdout
p=run([t.compiler,'--explain-extension','Protocols','--disable','Protocols']);assert p.returncode==0 and 'disabled by CLI' in p.stdout,p.stdout
# Linux authorization roots at the actual running image, not a spoofed argv[0].
fake=t.work/'spoofed/on1xc';fake.parent.mkdir(exist_ok=True);fake.write_text('not a compiler')
p=run(['bash','-c','exec -a "$1" "$2" --check "$3"','category-test',fake,t.compiler,example])
assert p.returncode==0,p.stderr

# Direct protocol packets: deterministic metadata, no output on errors, and
# token/source/module binding independent of the compiler's trusted producer.
sys.path.insert(0,str(t.root/'stdlib'))
from sexpr import Atom as S, dumps, loads

# Read-only typed feedback is pinned to a complete concrete request snapshot.
# These reports are inspected, never accepted as cached proof inputs.
def checked_plan(path,options=()):
    p=run([t.compiler,'--emit-category-plan',path,*options]);assert p.returncode==0,p.stderr
    return loads(p.stdout),p.stdout
report,wire=checked_plan(example)
assert report[0]=='on1x.category-plan' and report[3]=='well_typed'
batch=report[4][0];snapshot=batch[8];plan=batch[9]
assert len(plan[3])==2 and batch[2]=='2'
assert wire==checked_plan(example)[1]
assert wire==checked_plan(example,['--stdppx','identity'])[1]
assert snapshot[0]=='EvidenceSnapshot' and len(snapshot[2])==64
assert [S('SourceDigest'),str(example.resolve()),hashlib.sha256(example.read_bytes()).hexdigest()] in report[5]
assert snapshot[4][0][2][0][2][0]=='ForallType'
assert plan[3][0][1]=='Different' and plan[3][1][1]=='Eq_i64'
typed_env={**os.environ,'ON1X_PPX_PROTOCOL':'1','ON1X_PPX_LABEL':'Protocols',
           'ON1X_PPX_SCHEMA':str(t.root/'grammar/PPX.schema.json'),'ON1X_EXTENSION_PHASE':'evidence-v1'}
p=run([processor],input=dumps(snapshot),env=typed_env)
assert p.returncode==0 and loads(p.stdout)==plan,p.stderr
# The shared authoring API executes independently of the bundled entry point.
api_exe=t.build('evidence_api',text='import "'+str(t.root/'stdlib/ppx/category/typed.on1x')+'";\n'
               'fn main(){io_stdout(sexpr_print(ppx_category_make_evidence_plan(sexpr_parse(io_stdin()))));}')
p=run([api_exe],input=dumps(snapshot),env=typed_env)
assert p.returncode==0 and loads(p.stdout)==plan,p.stderr
for name,change in [
    ('wrong_type',lambda q:q[6][0].__setitem__(2,[S('I64Type')])),
    ('missing_fact',lambda q:q[6].pop()),
    ('extra_fact',lambda q:q[6].append(q[6][-1])),
    ('missing_member',lambda q:q[5][0][5].clear()),
    ('unknown_interface',lambda q:q[5][0].__setitem__(2,'Foreign')),
    ('invalid_binder',lambda q:q[4][0][2][0][2].__setitem__(1,S('2'))),
    ('wrong_origin',lambda q:q.__setitem__(3,'forged.on1x')),
    ('invalid_fingerprint',lambda q:q.__setitem__(2,'not-a-fingerprint')),
    ('version',lambda q:q.__setitem__(1,S('2'))),
    ('fact_budget',lambda q:q.__setitem__(6,[q[6][0]]*257)),
]:
    packet_bad=copy.deepcopy(snapshot);change(packet_bad)
    p=run([processor],input=dumps(packet_bad),env=typed_env)
    assert p.returncode and not p.stdout,(name,p.stderr)
module_report,_=checked_plan(consumer)
assert len(module_report[4])==1,'aliases duplicated evidence'
assert module_report[4][0][8][3]==str(module.resolve())
before=module_report[4][0][8][2];module.write_text(module.read_text()+'\n# new source revision\n')
assert checked_plan(consumer)[0][4][0][8][2]!=before,'stale source snapshot identity'
# Evidence execution still obeys source-PPX authorization.
rejected('plan_source_ppx',source+'\n@ppx arbitrary "./malicious"','--allow-source-ppx',
         ['--emit-category-plan'])
raw='@extension Protocols { protocol P<Self>{fn ok(x:Self)->bool;} }'
spelling=['protocol','P','<','Self','>','{','fn','ok','(','x',':','Self',')','->','bool',';','}']
start=[S('Loc'),'packet.on1x',S('0'),S('1'),S('1')]
end=[S('Loc'),'packet.on1x',S(str(len(raw))),S('1'),S(str(len(raw)+1))]
tokens=[];cursor=raw.index('protocol')
for text in spelling:
    offset=raw.index(text,cursor);cursor=offset+len(text)
    tokens.append([S('SourceToken'),text,[S('Loc'),'packet.on1x',S(str(offset)),S('1'),S(str(offset+1))]])
packet=[S('ModuleSyntax'),S('1'),'packet.on1x',[],[[S('DeclarationCarrier'),'Protocols',raw,start,end,tokens]],hashlib.sha256(b'packet.on1x').hexdigest()]
env={**os.environ,'ON1X_PPX_PROTOCOL':'1','ON1X_PPX_LABEL':'Protocols','ON1X_PPX_SCHEMA':str(t.root/'grammar/PPX.schema.json')}
p=run([processor],input=dumps(packet),env=env);assert p.returncode==0,p.stderr
assert p.stdout==run([processor],input=dumps(packet),env=env).stdout
assert 'packet.on1x::protocol::P' in p.stdout
for name,wire in [('old_root','(Unit 1 ())'),('truncated','(ModuleSyntax 1 ('),
                  ('version',dumps(packet).replace('ModuleSyntax 1','ModuleSyntax 2',1)),
                  ('origin',dumps(packet).replace('"packet.on1x"','"different.on1x"',1)),
                  ('source',dumps(packet).replace('protocol P<Self>','protocol Q<Self>',1)),
                  ('owner',dumps(packet).replace('"Protocols"','"Functors"',1)),
                  ('unknown','(Unknown 1 ())')]:
    p=run([processor],input=wire,env=env)
    assert p.returncode and not p.stdout,(name,p.stderr)
rejected('carrier_budget','@enable Protocols\n@extension Protocols{'+'x '*16385+'}fn main(){}','token budget exhausted')
rejected('declaration_budget',client(''.join(f'protocol P{i}<Self>{{fn f(x:Self)->bool;}}' for i in range(65))),'declaration budget exhausted')

# Relocated installed copies find only their own pinned native artifacts.
with tempfile.TemporaryDirectory(dir=t.work,prefix='category-install-') as temporary:
    base=Path(temporary);old=base/'old';prefix=base/'relocated'
    p=run(['cmake','--install',t.compiler.parent,'--prefix',old]);assert p.returncode==0,p.stderr
    old.rename(prefix);compiler=prefix/'bin/on1xc';native=prefix/'libexec/on1x/on1x-ppx-protocols'
    local=base/'eq.on1x';local.write_text(source)
    env=dict(hostile,ON1X_STDLIB=str(prefix/'share/on1x/stdlib'))
    p=run([compiler,'--check',local],env=env,cwd=base);assert p.returncode==0,p.stderr
    p=run([compiler,'--emit-category-plan',local],env=env,cwd=base)
    assert p.returncode==0 and loads(p.stdout)[3]=='well_typed',p.stderr
    p=run([compiler,'--emit-c',local],env=env,cwd=base);assert p.returncode==0,p.stderr
    out=base/'relocated.c';out.write_text(p.stdout);exe=base/'relocated-example'
    p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I',prefix/'share/on1x/runtime',out,
           prefix/'share/on1x/runtime/on1x_runtime.c',*t.flags,'-o',exe]);assert p.returncode==0,p.stderr
    p=run([exe],env=env);assert p.returncode==0 and p.stdout=='true\nfalse\n',p.stderr
    original=native.read_bytes();native.write_bytes(original+b'changed')
    p=run([compiler,'--check',local],env=env);assert p.returncode and 'digest mismatch' in p.stderr,p.stderr
    native.write_bytes(original)
    schema=native.parent/'category.schema.json';original_schema=schema.read_bytes();schema.write_bytes(original_schema+b' ')
    p=run([compiler,'--check',local],env=env);assert p.returncode and 'digest mismatch' in p.stderr,p.stderr
    schema.write_bytes(original_schema);native.rename(native.with_suffix('.missing'))
    p=run([compiler,'--check',local],env=env);assert p.returncode and 'no PATH or Python fallback' in p.stderr,p.stderr

print('Category concrete evidence: native syntax/typed phases, host admission, activation, import identity and relocation PASS')
