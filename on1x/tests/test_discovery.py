"""Search-list and manifest boundary tests through the actual compiler/C11."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
from support import NativeTests, run

t=NativeTests()
# A fresh, automatically cleaned workspace makes repeated CTest runs independent
# of precedence/drift fixtures deliberately created near the end of this test.
workspace=tempfile.TemporaryDirectory(dir=t.work,prefix='run-')
t.work=Path(workspace.name)
env={k:v for k,v in os.environ.items() if k not in (
    'ON1X_LIB_PATH','ON1X_STDLIB_PATH','ON1X_PPX_PATH','ON1X_STDPPX_PATH',
    'ON1X_MULIB_PATH','ON1X_PATH','ON1X_STDLIB','ON1X_MULIB_ROOT','ON1X_PPX_SCHEMA')}
helper=t.root/'tools/on1x-build.py'

def write(path,text):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(text)
    return path

def execute(name,text,expected,options=(),extra_env=None):
    source=t.source(name,text);exe=t.work/(name+'.exe')
    p=run([sys.executable,helper,source,'--on1xc',t.compiler,'--cc',t.cc,'-o',exe,
           *options,*['--cflag='+f for f in t.flags]],env={**env,**(extra_env or {})})
    assert p.returncode==0,(name,p.stderr)
    p=run([exe],env=env)
    assert p.returncode==0 and p.stdout==expected,(name,p.stdout,p.stderr)
    return source

def reject(name,text,message,options=(),extra_env=None):
    source=t.source(name,text);out=t.work/'preserved.c';out.write_text('preserved')
    p=run([t.compiler,'--emit-c',source,'-o',out,*options],env={**env,**(extra_env or {})})
    assert p.returncode!=0 and message in p.stderr,(name,p)
    assert out.read_text()=='preserved'
    return source

stdlib_manifest='(On1xfile 1 (kind "stdlib") (name "std") (root "."))\n'
roots=[t.work/'libraries one',t.work/'libraries two']
for i,root in enumerate(roots):
    write(root/'values.on1x',f'fn answer()->i64{{return {41+i};}}')
execute('env_lib','import values;fn main(){println(values.answer());}','42\n',
        extra_env={'ON1X_LIB_PATH':f'{t.work}/missing::{roots[1]}:'})
execute('cli_lib','import values;fn main(){println(values.answer());}','41\n',
        ['--lib-path',f'{roots[0]}:{roots[1]}'],{'ON1X_LIB_PATH':str(roots[1])})
execute('repeat_lib','import values;fn main(){println(values.answer());}','42\n',
        ['--lib-path',str(t.work/'missing'),'--lib-path',str(roots[1])])
execute('mixed_cli_order','import values;fn main(){println(values.answer());}','41\n',
        ['-I',roots[0],'--lib-path',roots[1]])

# A library's manifest supplies the source directory, not an executable command.
package=roots[1]/'arithmetic'
write(package/'On1xfile','(On1xfile 1 (kind "library") (name "arithmetic") (root "src"))')
write(package/'src/__init__.on1x','from .number import answer;')
write(package/'src/number.on1x','fn answer()->i64{return 42;}')
execute('manifest_library','import arithmetic;from arithmetic import number;'
        'fn main(){println(arithmetic.answer()+number.answer());}','84\n',
        extra_env={'ON1X_LIB_PATH':str(roots[1])})
execute('direct_package','import arithmetic.number as N;fn main(){println(N.answer());}','42\n',
        ['--lib-path',package])

# A stdlib is discovered as a direct manifest root or as container/stdlib.
# Both explicit/selective std imports and the implicit prelude share resolution.
custom=t.work/'custom ecosystem/stdlib'
write(custom/'On1xfile',stdlib_manifest)
write(custom/'pervasive.on1x','fn identity(x:i64)->i64{return x+1;}')
write(custom/'Answer.on1x','fn value()->i64{return 41;}')
client='from std import Answer;import std.Answer as A;fn main(){println(identity(A.value()));println(Answer.value());}'
for variable in ['ON1X_LIB_PATH','ON1X_STDLIB_PATH']:
    execute('stdlib_'+variable,client,'42\n41\n',extra_env={variable:f'{t.work}/absent::{custom.parent}'})
for flag in ['--lib-path','--stdlib-path']:
    execute('stdlib_'+flag[2:],client,'42\n41\n',[flag,f'{t.work}/absent:{custom}'])
    p=run([t.compiler,'--print-stdlib',flag,custom],env=env)
    assert p.returncode==0 and Path(p.stdout.strip())==custom.resolve(),p
# Ordinary project imports cannot shadow std.*.
write(t.work/'Answer.on1x','fn value()->i64{return -1;}')
execute('anchored_std',client,'42\n41\n',['--stdlib-path',custom])
# Legacy explicit paths still allow unmanifested roots and no-prelude builds.
legacy=t.work/'legacy stdlib';write(legacy/'value.on1x','fn value()->i64{return 7;}')
execute('legacy_stdlib','import std.value as V;fn main(){println(V.value());}','7\n',
        ['--stdlib',legacy,'--no-prelude'],{'ON1X_STDLIB_PATH':str(custom),'ON1X_LIB_PATH':str(custom)})
reject('missing_std_manifest','fn main(){}','requires an On1xfile',
       ['--stdlib-path',legacy])
copy=t.work/'relocated stdlib'
shutil.copytree(t.root/'stdlib',copy,dirs_exist_ok=True,ignore=shutil.ignore_patterns('__pycache__'))
execute('stdlib_runtime','import std.numeric.Real as R;fn main(){println(R.basis_real_add("1.5","2.5"));}',
        '4\n',extra_env={'ON1X_STDLIB_PATH':str(copy)})
execute('stdlib_runtime_fallback','import std.numeric.Real as R;fn main(){println(R.basis_real_add("1.5","2.5"));}',
        '4\n',extra_env={'ON1X_STDLIB_PATH':str(custom)})

# Script PPXs are found by manifest name, but only after explicit authorization.
ppxroots=[t.work/'processors one',t.work/'processors two']
def processor(root,name='chosen',value=42,manifest=True):
    header=f'# on1x: (On1xfile 1 (kind "ppx") (name "{name}") (protocol "1"))\n' if manifest else '# missing metadata\n'
    return write(root/(name+'.py'),'#!/usr/bin/env python3\n'+header+
                 'import sys\ns=sys.stdin.read()\n'
                 f'sys.stdout.write(s.replace(\'"314159"\',\'"{value}"\'))\n')
processor(ppxroots[0],value=41);processor(ppxroots[1])
ppxclient='fn main(){println(314159);}'
for variable in ['ON1X_PPX_PATH','ON1X_STDPPX_PATH']:
    execute('ppx_'+variable,ppxclient,'42\n',['--use-ppx','chosen'],
            {variable:f'{t.work}/absent::{ppxroots[1]}:'})
for flag in ['--ppx-path','--stdppx-path']:
    execute('ppx_'+flag[2:],ppxclient,'41\n',[flag,f'{ppxroots[0]}:{ppxroots[1]}','--stdppx','chosen'],
            {'ON1X_PPX_PATH':str(ppxroots[1])})
execute('registered_name',ppxclient,'42\n',['--ppx-path',ppxroots[1],'--ppx','chosen','chosen'])
sourceclient='@ppx chosen "chosen";'+ppxclient
reject('source_authority',sourceclient,'requires --allow-source-ppx',
       ['--ppx-path',ppxroots[1]])
execute('source_discovery',sourceclient,'42\n',
        ['--allow-source-ppx','--ppx-path',ppxroots[1]])
source=t.source('discovery_deps',sourceclient)
p=run([t.compiler,'--deps',source,'--ppx-path',ppxroots[1]],env=env)
assert p.returncode==0,p
execute('no_implicit_ppx',ppxclient,'314159\n',extra_env={'ON1X_PPX_PATH':str(ppxroots[1])})
processor(ppxroots[0],'missing_metadata',manifest=False)
reject('missing_ppx_manifest',ppxclient,'second-line on1x manifest',
       ['--ppx-path',ppxroots[0],'--use-ppx','missing_metadata'])
write(ppxroots[0]/'wrong.py','#!/usr/bin/env python3\n'
      '# on1x: (On1xfile 1 (kind "ppx") (name "other") (protocol "1"))\nraise Exception("must not run")\n')
reject('mismatched_ppx_manifest',ppxclient,'does not match',
       ['--ppx-path',ppxroots[0],'--use-ppx','wrong'])
reject('missing_ppx',ppxclient,'PPX not found',['--use-ppx','not_installed'])
# Empty entries do not grant CWD discovery.
processor(t.work,'cwd_only')
source=t.source('no_cwd_ppx',ppxclient)
p=run([t.compiler,'--check',source,'--use-ppx','cwd_only'],
      env={**env,'ON1X_PPX_PATH':'::'},cwd=t.work)
assert p.returncode!=0 and 'PPX not found' in p.stderr,p

# Native On1x-authored processors use the same manifest resolver. CMake installs
# their manifest plus schema; callers need no executable/schema filename.
native=t.compiler.parent/'stdppx'
if (native/'on1x-ppx-identity').is_file():
    execute('native_discovery',ppxclient,'314159\n',
            ['--ppx-path',native,'--use-ppx','identity'])
    native_package=t.work/'native package'
    native_package.mkdir(exist_ok=True)
    shutil.copy2(native/'on1x-ppx-identity',native_package/'identity')
    shutil.copy2(t.root/'grammar/PPX.schema.json',native_package/'schema.json')
    manifest='(On1xfile 1 (kind "ppx") (name "custom") (protocol "1") (processor "identity" "identity" "schema.json"))'
    write(native_package/'On1xfile',manifest)
    execute('native_manifest',ppxclient,'314159\n',
            ['--stdppx-path',native_package,'--use-ppx','identity'])
    (native_package/'schema.json').unlink()
    reject('missing_native_schema',ppxclient,'missing manifest input',
           ['--ppx-path',native_package,'--use-ppx','identity'])
    write(native_package/'On1xfile',manifest.replace('"identity" "identity"','"identity" "../identity"'))
    reject('escaping_native_manifest',ppxclient,'nonlocal',
           ['--ppx-path',native_package,'--use-ppx','identity'])
    write(native_package/'On1xfile',manifest.replace('"identity" "identity"','"identity" "escape"'))
    alias=native_package/'escape'
    if alias.is_symlink():alias.unlink()
    alias.symlink_to(native/'on1x-ppx-identity')
    reject('symlink_native_manifest',ppxclient,'escaping manifest',
           ['--ppx-path',native_package,'--use-ppx','identity'])
else:
    print('Native PPX discovery: UNSUPPORTED (native PPX build disabled)')

# Malformed manifests and finite budgets fail without publishing output.
bad=t.work/'bad library'
for i,text in enumerate([
    '(On1xfile 2 (kind "stdlib") (name "std"))',
    '(On1xfile 1 (kind "stdlib") (kind "stdlib") (name "std"))',
    '(On1xfile 1 (kind "stdlib") (name "std") (root ".."))',
    '(On1xfile 1 (kind "stdlib") (name "std") (command "touch evil"))',
]):
    write(bad/'On1xfile',text)
    reject('bad_manifest_'+str(i),'fn main(){}','manifest',['--stdlib-path',bad])
write(bad/'On1xfile',' '*65537)
reject('manifest_limit','fn main(){}','manifest byte limit',['--stdlib-path',bad])
reject('path_limit','fn main(){}','path count',['--lib-path',':'.join('x'+str(i) for i in range(257))])
for flag in ['--lib-path','--stdlib-path','--ppx-path','--stdppx-path']:
    p=run([t.compiler,flag],env=env)
    assert p.returncode!=0 and 'requires' in p.stderr,p
for resource in ['/etc/passwd','../runtime/on1x_runtime.c']:
    p=run([t.compiler,'--stdlib-resource',resource],env=env)
    assert p.returncode!=0 and 'local relative path' in p.stderr,p

# MuLIB lookup accepts include directories across a list. The existing MANIFEST
# still owns implementation macros/dependencies; unknown names never guess ABI.
spec=importlib.util.spec_from_file_location('paths_house',t.root/'stdlib/mulib/housekeeping.py')
house=importlib.util.module_from_spec(spec);spec.loader.exec_module(house)
kernels=[t.work/'kernel one',t.work/'kernel two']
for directory in kernels:directory.mkdir(exist_ok=True)
header=kernels[1]/'mulib-bitset.h'
header.write_bytes((t.root.parent/'include/mulib-bitset.h').read_bytes()+b'\n/* search-path fixture */\n')
shim=write(t.work/'kernel_adapter.c','''#include <mulib-bitset.h>
#include <stdint.h>
int64_t check_bits(void) {
 mulib_bitset_t b;
 if (!mulib_bitset_init(&b, 16)) return -1;
 (void)mulib_bitset_set(&b, 7, 1);
 int64_t count = (int64_t)mulib_bitset_count(&b);
 mulib_bitset_free(&b); return count;
}
''')
report=t.work/'paths-plan.json'
kernelclient='import std.mulib as M;extern fn check_bits()->i64;fn main(){assert(M.has_kernel("bitset"));println(check_bits());}'
options=['--mulib','bitset','--extra-c',shim,'--mulib-plan',report]
execute('kernel_env',kernelclient,'1\n',options,{'ON1X_MULIB_PATH':f'{kernels[0]}::{kernels[1]}:'})
record=json.loads(report.read_text())
assert str(header.resolve()) in record['inputs']
assert record['plan']['implementation_macros'].count('MULIB_IMPL_BITSET')==1
assert record['plan']['modules'][0]['header_sha256']==hashlib.sha256(header.read_bytes()).hexdigest()
execute('kernel_cli',kernelclient,'1\n',[*options,'--mulib-path',f'{kernels[0]}:{kernels[1]}'])
other=kernels[0]/'mulib-strbuf.h'
shutil.copy2(t.root.parent/'include/mulib-strbuf.h',other)
execute('kernel_multi_root',kernelclient,'1\n',
        [*options,'--mulib','strbuf','--mulib-path',f'{kernels[0]}:{kernels[1]}'])
inputs=json.loads(report.read_text())['inputs']
assert str(other.resolve()) in inputs and str(header.resolve()) in inputs
for tool in ['on1x-mulib.py','on1x-exotract.py']:
    p=run([sys.executable,t.root/'tools'/tool,'--mulib-path',str(kernels[1]),'plan','bitset'],env=env)
    assert p.returncode==0 and json.loads(p.stdout)['modules'][0]['header_path']==str(header.resolve()),p
# CLI entries precede environment entries.
first=kernels[0]/header.name;first.write_bytes(header.read_bytes()+b'/* first */\n')
execute('kernel_precedence',kernelclient,'1\n',[*options,'--mulib-path',kernels[0]],
        {'ON1X_MULIB_PATH':str(kernels[1])})
assert str(first.resolve()) in json.loads(report.read_text())['inputs']
# Reviewed/core source pins may not silently select a changed external header.
write(kernels[0]/'mulib-json.h',(t.root.parent/'include/mulib-json.h').read_text()+'\n/* drift */\n')
output=t.work/'preserved.exe';output.write_text('preserved')
p=run([sys.executable,helper,t.source('core_pin','fn main(){}'),'--on1xc',t.compiler,'--cc',t.cc,
       '-o',output,'--mulib','json','--mulib-path',kernels[0]],env=env)
assert p.returncode!=0 and 'source-pinned' in p.stderr and output.read_text()=='preserved',p
execute('integrated_mulib','from std import mulib;fn main(){println(mulib.abi_version());}','1\n')
print('Library/stdlib, PPX/stdppx manifests and MuLIB kernel search lists: PASS')
