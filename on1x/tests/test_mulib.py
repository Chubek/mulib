"""Kernel/fixture matching, real native adapters, immutable resources and packaging."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys
from support import NativeTests, run

t=NativeTests();root=t.root.parent
tool=t.root/'tools/on1x-mulib.py'
helper=t.root/'tools/on1x-build.py'
spec=importlib.util.spec_from_file_location('house',t.root/'stdlib/mulib/housekeeping.py')
house=importlib.util.module_from_spec(spec);spec.loader.exec_module(house)

def command(*args):return run([sys.executable,tool,*args])
def build(name,text,options=()):
    source=t.source(name,text);exe=t.work/name
    p=run([sys.executable,helper,source,'--on1xc',t.compiler,'--cc',t.cc,'-o',exe,
           *options,*['--cflag='+flag for flag in t.flags]])
    assert p.returncode==0,(name,p.stderr)
    return exe
def execute(exe,expected):
    p=run([exe],cwd=t.work)
    assert p.returncode==0 and p.stdout==expected,(exe,p.returncode,p.stdout,p.stderr)

p=command('list');assert p.returncode==0,p.stderr
inventory=json.loads(p.stdout)
assert len(inventory['modules'])==len(house.inventory(root))
assert {p.name for p in (root/'library').iterdir() if p.is_dir()}<=set(inventory['fixtures'])
assert all((root/'library'/name).is_dir() for name,g in inventory['fixtures'].items() if g['kind']!='empty')
assert inventory['fixtures']['immgtk-widgets']['kind']=='empty'
assert len(inventory['fixtures']['webidl']['matched_files'])>=1000
assert len(inventory['fixtures']['librl']['matched_files'])==45
elvis = inventory['fixtures']['elvis']['matched_files']
assert {
    f'library/elvis/{name}.elvs' for name in ('Bing', 'DuckDuckGo', 'Google', 'MDN', 'Scholar')
} <= set(elvis)
for group in inventory['fixtures'].values():
    for path in group['matched_files']:assert (root/path).is_file()
for args,message in [(('plan','absent'),'known MuLIB modules'),
                     (('plan','--fixture','absent'),'known MuLIB fixtures'),
                     (('plan','--fixture','immgtk-widgets'),'empty')]:
    p=command(*args);assert p.returncode!=0 and message in p.stderr,p.stderr

core=house.plan(root,['json','vec','parsec'])
assert core['implementation_macros']==['MULIB_IMPL_PARSEC','MULIB_IMPL_STRBUF','MULIB_IMPL_UTF8','MULIB_IMPL_JSON','MULIB_IMPL_VEC']
mune=house.plan(root,['mune'])
assert mune['implementation_macros'].count('MULIB_IMPL_PARSER')==1
assert len(mune['required_symbols']['mune'])==52 and mune['implementation_translation_units']==1
for path in ['tests/test_mune_runtime.c','tests/test_mune_runtime_alloc.c','tools/check_mune_runtime.py']:
    assert mune['inputs'][str((root/path).resolve())]==hashlib.sha256((root/path).read_bytes()).hexdigest()
provider=house.plan(root,['yantlr'],['yantlr'])
assert ['parsec','peg','parsec'] in provider['dependency_cycles']
assert 'MULIB_IMPL_PARSER' in provider['implementation_macros']
assert 'MULIB_IMPL_YANTLR_STDLIB' in provider['implementation_macros']
assert house.plan(root,['fst'])['implementation_macros'][-1]=='MULIB_IMPL_FST_SERIALIZE'
elvi=house.plan(root,[],['elvis'])
assert elvi['requested_fixtures']==['elvis'] and len(elvi['required_symbols']['elvi-scrape'])==6
assert elvi['implementation_translation_units']==1 and 'MULIB_IMPL_PARSER' not in elvi['implementation_macros']
assert elvi['link_libraries']==['m']
assert [f['path'] for f in elvi['files']]==elvis
elvi_kernel=house.plan(root,['elvi-scrape'])
assert elvi_kernel['implementation_macros']==elvi['implementation_macros']
assert elvi_kernel['inputs']==elvi['inputs'] and elvi_kernel['files']==elvi['files']

execute(build('housekeeping','''from std import mulib;
fn main(){
 assert(mulib.abi_version()==1);assert(mulib.pointer_bits()>=32);
 assert(mulib.fits_size(0));assert(!mulib.fits_size(-1));
 assert(mulib.fits_array(100,8));assert(!mulib.fits_array(mulib.max_size(),2));
 assert(!mulib.fits_array(-1,0));assert(mulib.fits_array(mulib.max_size(),0));
 assert(mulib.kernel_count()==0);assert(!mulib.has_kernel("json"));
 assert(mulib.kernel_name(-1)=="");assert(mulib.fixture_name(999)=="");
 let(ok,text)=mulib.fixture_text("absent");assert(!ok&&text=="");
 assert(mulib.fixture_size("absent")==-1);assert(mulib.fixture_byte("absent",0)==-1);
 println("housekeeping OK");}'''),'housekeeping OK\n')
missing=build('missing_text','from std import mulib;fn main(){io_stdout(mulib.require_fixture("absent"));}')
p=run([missing]);assert p.returncode!=0 and 'fixture is missing' in p.stderr and not p.stdout,p.stderr

report=t.work/'native-plan.json'
exe=build('provider','''import std.mulib as M;
extern fn fixture_yantlr_artifacts(source:string)->i64;
fn main(){
 assert(M.has_kernel("yantlr"));assert(M.has_kernel("json"));
 assert(M.kernel_header("yantlr")=="kernels/mulib-yantlr.h");
 assert(text_len(M.kernel_sha256("yantlr"))==64);
 let path="library/yantlr/example.y1";
 assert(M.has_fixture(path));assert(M.fixture_group(path)=="yantlr");
 assert(M.fixture_byte(path,0)==47);assert(M.fixture_byte(path,-1)==-1);
 assert(M.fixture_byte(path,M.fixture_size(path))==-1);
 assert(fixture_yantlr_artifacts(M.require_fixture(path))==1);
 assert(fixture_yantlr_artifacts("invalid YANTLR")==-1);
 assert(fixture_yantlr_artifacts(M.require_fixture(path))==1);
 let document=json_parse_or_die("[1,2,3]");println(json_size(document));
 println("kernel + fixture OK");}''',
 ['--mulib-fixture','yantlr','--extra-c',t.root/'tests/mulib/yantlr.c','--mulib-plan',report])
execute(exe,'3\nkernel + fixture OK\n')
plan=json.loads(report.read_text());assert plan['plan']['implementation_translation_units']==1
assert str(root/'library/yantlr/mulib-yantlr-stdlib.h') in plan['inputs']
assert plan['executable_sha256']==hashlib.sha256(exe.read_bytes()).hexdigest()
assert plan['compiler_target'] and plan['compiler_version']
relocated=t.work/'relocated';relocated.mkdir(exist_ok=True)
shutil.copy2(exe,relocated/'app');execute(relocated/'app','3\nkernel + fixture OK\n')

execute(build('elvi-provider',(t.root/'tests/mulib/elvi-scrape.on1x').read_text(),
  ['--mulib-fixture','elvis','--extra-c',t.root/'tests/mulib/elvi-scrape.c']),'elvi providers OK\n')

execute(build('optml','''from std.mulib import require_fixture;
extern fn fixture_optml_count(source:string)->i64;
fn main(){println(fixture_optml_count(require_fixture("library/optim/default.optml")));
 assert(fixture_optml_count("not-a-rule")==-1);}''',
 ['--mulib','optml','--extra-c',t.root/'tests/mulib/optml.c']),'75\n')
execute(build('shiraz','''import std.mulib as M;
extern fn fixture_shiraz_registry()->bool;
fn main(){assert(M.has_kernel("kvdb"));assert(fixture_shiraz_registry());println("registry OK");}''',
 ['--mulib','shiraz','--extra-c',t.root/'tests/mulib/shiraz.c']),'registry OK\n')

embedded_report=t.work/'embedded-plan.json'
execute(build('embedded','''import std.mulib as M;
extern fn fixture_muw3_script(source:string)->bool;
fn main(){
 assert(M.has_kernel("muw3-mune")&&M.has_kernel("microchan"));
 assert(M.has_fixture("library/lmuw3/lmuw3.lua"));
 assert(M.has_fixture("library/microchan/profiles/muw3.prof"));
 assert(fixture_muw3_script("local w=require('lmuw3');return w==muw3 and w.json_decode('[21,21]')[2]==21"));
 assert(!fixture_muw3_script("error('intentional')"));
 assert(fixture_muw3_script("return require('lmuw3').json_encode({answer=42})=='{\\\"answer\\\":42}'"));
 println("embedded lmuw3 OK");}''',
 ['--mulib','muw3-mune','--extra-c',t.root/'tests/mulib/lmuw3.c','--mulib-plan',embedded_report]),'embedded lmuw3 OK\n')
embedded=json.loads(embedded_report.read_text())
assert str(root/'kernels/detail/mulib-lmuw3.inc') in embedded['inputs']

# Compiler-produced dependency escaping and implementation-only includes.
dependency_dir=t.work/'includes with spaces';dependency_dir.mkdir(exist_ok=True)
detail=dependency_dir/'detail #$.inc';detail.write_text('static const int selected=42;\n')
impl=dependency_dir/'impl.c';impl.write_text('#define ENABLE_IMPL\n#ifdef ENABLE_IMPL\n#include "detail #$.inc"\n#endif\n')
inputs=house.compile_inputs(t.cc,[],impl)
assert inputs[str(detail.resolve())]==hashlib.sha256(detail.read_bytes()).hexdigest()

# Binary fixtures preserve NUL and all byte values instead of silently truncating.
binary_root=t.work/'binary';(binary_root/'library').mkdir(parents=True,exist_ok=True)
data=bytes(range(256));(binary_root/'library/bytes').write_bytes(data)
recipe={'modules':[],'files':[dict(path='library/bytes',group='binary',bytes=len(data),sha256=hashlib.sha256(data).hexdigest())]}
(binary_root/'library/empty').write_bytes(b'')
recipe['files'].append(dict(path='library/empty',group='binary',bytes=0,sha256=hashlib.sha256(b'').hexdigest()))
metadata=t.work/'binary_data.c';metadata.write_text(house.metadata_source(binary_root,recipe))
client=t.work/'binary_client.c';client.write_text('''#include "runtime.h"
#include <assert.h>
#include <string.h>
int main(void){assert(on1x_mulib_has_fixture("library/bytes"));
 assert(!on1x_mulib_fixture_is_text("library/bytes"));
 assert(!strcmp(on1x_mulib_fixture_text("library/bytes"),""));
 for(int i=0;i<256;++i)assert(on1x_mulib_fixture_byte("library/bytes",i)==i);
 assert(on1x_mulib_fixture_byte("library/bytes",256)==-1);
 assert(on1x_mulib_fixture_is_text("library/empty"));
 assert(on1x_mulib_fixture_size("library/empty")==0);
 assert(!strcmp(on1x_mulib_fixture_text("library/empty"),""));
 assert(!on1x_mulib_has_fixture(NULL));return 0;}''')
p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I',t.root/'stdlib/mulib',metadata,client,*t.flags,'-o',t.work/'binary_client'])
assert p.returncode==0,p.stderr
execute(t.work/'binary_client','')

bundle=t.work/'bundle'
if bundle.exists():shutil.rmtree(bundle)
p=command('bundle','--fixture','yantlr','--output',bundle);assert p.returncode==0,p.stderr
assert (bundle/'library/yantlr/example.y1').read_bytes()==(root/'library/yantlr/example.y1').read_bytes()
assert command('verify',bundle,'--sources').returncode==0
assert command('bundle','--fixture','yantlr','--output',bundle).returncode!=0
moved=t.work/'moved-bundle'
if moved.exists():shutil.rmtree(moved)
bundle.rename(moved);assert command('verify',moved).returncode==0
(moved/'library/yantlr/example.y1').write_text('drift')
p=command('verify',moved);assert p.returncode!=0 and 'drift' in p.stderr
escape=binary_root/'library/escape'
if escape.is_symlink():escape.unlink()
escape.symlink_to(root/'MANIFEST.yaml')
try:house.local(binary_root,'library/escape');raise AssertionError('symlink escape accepted')
except ValueError:pass
try:house.fixture_files(binary_root,inventory['fixtures']['yantlr']);raise AssertionError('missing fixtures accepted')
except ValueError:pass

# A generator/kernel mismatch rejects without updating checked-in generated files.
fake=t.work/'stale';(fake/'tools').mkdir(parents=True,exist_ok=True)
(fake/'library/lmuw3').mkdir(parents=True,exist_ok=True);(fake/'kernels/detail').mkdir(parents=True,exist_ok=True)
shutil.copy2(root/'tools/embed_lmuw3.py',fake/'tools/embed_lmuw3.py')
(fake/'library/lmuw3/lmuw3.lua').write_text('return {}\n')
(fake/'kernels/detail/mulib-lmuw3.inc').write_text('stale')
try:house.check_generated(fake,{'generator_checks':['tools/embed_lmuw3.py']});raise AssertionError('stale embedding accepted')
except ValueError as e:assert 'mismatch' in str(e)
assert (fake/'kernels/detail/mulib-lmuw3.inc').read_text()=='stale'

for options,diagnostic in [(['--mulib','absent'],'known MuLIB modules'),
                           (['--mulib','json','--cflag=-DMULIB_IMPL_JSON'],'single generated TU'),
                           (['--mulib','json','--extra-c',t.root/'runtime/on1x_mulib_impl.c'],'duplicate MuLIB implementation')]:
    output=t.work/'preserved';output.write_text('preserved')
    p=run([sys.executable,helper,t.work/'housekeeping.on1x','--on1xc',t.compiler,'--cc',t.cc,'-o',output,*options])
    assert p.returncode!=0 and diagnostic in p.stderr and output.read_text()=='preserved',p.stderr
bad=t.source('bad_link','extern fn missing_native()->i64;fn main(){println(missing_native());}')
p=run([sys.executable,helper,bad,'--on1xc',t.compiler,'--cc',t.cc,'-o',output])
assert p.returncode!=0 and output.read_text()=='preserved'
print('MuLIB kernel/fixture inventory, native providers, binary resources, relocation and rollback: PASS')
