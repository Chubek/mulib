#!/usr/bin/env python3
"""Actual Exotract/ExoML discovery -> one-TU archive -> C11/On1x execution."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

cli=argparse.ArgumentParser()
cli.add_argument('exotract',type=Path)
cli.add_argument('on1xc',type=Path)
cli.add_argument('root',type=Path)
cli.add_argument('work',type=Path)
cli.add_argument('cc')
cli.add_argument('--cflag',action='append',default=[])
a=cli.parse_args();a.work.mkdir(parents=True,exist_ok=True)
tool=a.root/'tools/on1x-exotract.py'
resource=subprocess.check_output(['clang','-print-resource-dir'],text=True).strip()
def run(args):
    return subprocess.run([str(x) for x in args],capture_output=True,text=True,timeout=180)
def command(*args):return [sys.executable,tool,*args]
def build(names,dest,flags=(),fixtures=()):
    return run(command('build',*names,'--output',dest,'--exotract',a.exotract,
                       '--compiler',a.cc,'--resource-dir',resource,
                       *['--cflag='+f for f in [*a.cflag,*flags]],
                       *[arg for name in fixtures for arg in ['--fixture',name]]))
def client(path,dest,expected,extra=()):
    exe=dest/'client'
    p=run([a.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',*a.cflag,path,
           '-I',dest,'-I',a.root.parent/'kernels',*extra,dest/'libmulib.a','-lm','-o',exe])
    assert p.returncode==0,p.stderr
    p=run([exe]);assert p.returncode==0 and p.stdout==expected,(p.stdout,p.stderr)
data=json.loads(run(command('list')).stdout)
assert (len(data['modules']) + len(data['auxiliary_headers']) + len(data['reserved_headers'])
        == len(list((a.root.parent/'kernels').glob('mulib-*.h'))))
assert 'b+tree' in data['modules'] and 'microjs' in data['modules']
p=run(command('plan','json','vec','parsec'));assert p.returncode==0,p.stderr
plan=json.loads(p.stdout)
assert plan['implementation_macros']==['MULIB_IMPL_PARSEC','MULIB_IMPL_STRBUF','MULIB_IMPL_UTF8','MULIB_IMPL_JSON','MULIB_IMPL_VEC']
assert plan['safe_export'] is False and plan['implementation_translation_units']==1
for name,libraries,source,expected in [
    ('core',['json','vec','parsec'],'client.c','[42]\n'),
    ('microjs',['microjs'],'microjs.c','42\n'),
    ('mune',['mune'],'mune.c','mune: archive OK\n'),
    ('elvi-scrape',['elvi-scrape'],'elvi-scrape.c','elvi-scrape: archive OK\n'),
    ('http3',['http3','quic'],'http3.c','quic + http3: archive OK\n'),
]:
    dest=a.work/name
    if dest.exists():shutil.rmtree(dest)
    p=build(libraries,dest);assert p.returncode==0,p.stderr
    lock=json.loads((dest/'lock.json').read_text())
    for artifact,digest in lock['artifacts'].items():assert hashlib.sha256((dest/artifact).read_bytes()).hexdigest()==digest
    for library in libraries:
        binding=json.loads((dest/(library+'.on1x-import.json')).read_text())
        assert binding['unsafe_by_default'] and binding['cdecl']['complete']
        assert all(entry['safe'] is False for entry in binding['imports'])
    members=run(['ar','t',dest/'libmulib.a']);assert members.stdout=='mulib.o\n',members.stdout
    symbols=json.loads((dest/'symbols.json').read_text())['libraries']
    if name=='core':
        assert len(symbols['json']['archive_exports'])==20
        assert len(symbols['vec']['archive_exports'])==7
        assert len(symbols['parsec']['archive_exports'])==18
        assert 'mulib_parser_from_peg' in symbols['parsec']['declaration_only']
    elif name=='elvi-scrape':
        assert len(symbols['elvi-scrape']['archive_exports'])==6 and not symbols['elvi-scrape']['declaration_only']
        recipe=json.loads((dest/'build-plan.json').read_text())
        assert 'MULIB_IMPL_PARSER' not in recipe['implementation_macros']
        evidence=json.loads((a.root/'contracts-exoml/catalog.json').read_text())['profiles']['elvi-scrape']['evidence_sha256']
        expected_files=sorted(path for path in evidence if path.endswith('.elvs'))
        assert {'library/elvis/Google.elvs', 'library/elvis/Bing.elvs'} < set(expected_files)
        assert [f['path'] for f in recipe['files']]==expected_files
        for path,digest in evidence.items():assert lock['inputs'][str((a.root.parent/path).resolve())]==digest
    elif name=='mune':
        assert len(symbols['mune']['archive_exports'])==52 and not symbols['mune']['declaration_only']
        recipe=json.loads((dest/'build-plan.json').read_text())
        assert recipe['implementation_macros'].count('MULIB_IMPL_PARSER')==1
        for path in ['kernels/mulib-mune.h','manual/mulib-mune.md','tests/test_mune_runtime.c',
                     'tests/test_mune_runtime_alloc.c','tools/check_mune_runtime.py']:
            assert lock['inputs'][str((a.root.parent/path).resolve())]==hashlib.sha256((a.root.parent/path).read_bytes()).hexdigest()
    elif name=='http3':
        assert len(symbols['http3']['archive_exports'])==8 and not symbols['http3']['declaration_only']
        assert len(symbols['quic']['archive_exports'])==12 and not symbols['quic']['declaration_only']
        recipe=json.loads((dest/'build-plan.json').read_text())
        switches=recipe['implementation_macros']
        assert all(switches.count(m)==1 for m in ['MULIB_IMPL_HTTP3_CLIENT', 'MULIB_IMPL_QUIC_CLIENT', 'MULIB_IMPL_TLS'])
        assert 'MULIB_IMPL_PARSER' not in switches
        for path in ['kernels/detail/mulib-quic-client.h','kernels/detail/mulib-http3-client.h',
                     'kernels/detail/mulib-qpack-tables.h']:
            assert lock['inputs'][str((a.root.parent/path).resolve())]==hashlib.sha256((a.root.parent/path).read_bytes()).hexdigest()
    else:assert len(symbols['microjs']['archive_exports'])==15
    client(a.root/'contracts-exotract/examples'/source,dest,expected,['-pthread','-ldl'] if name=='mune' else [])
    keep=(dest/'lock.json').read_bytes();p=build(libraries,dest)
    assert p.returncode!=0 and (dest/'lock.json').read_bytes()==keep
# Actual On1x JSON facade links the same archive; no duplicate implementation TU.
dest=a.work/'core';generated=a.work/'json.c'
p=run([a.on1xc,'--emit-c',a.root/'examples/json.on1x','-o',generated]);assert p.returncode==0,p.stderr
client(generated,dest,'3\n{"items":[1,2,3],"ok":true}\n',
       ['-I',a.root/'runtime',a.root/'runtime/on1x_runtime.c',a.root/'runtime/on1x_json.c'])
failure=a.work/'failure'
if failure.exists():shutil.rmtree(failure)
for libraries,flags in [(['not-a-module'],[]),(['vec'],['-DMULIB_IMPL_VEC']),(['vec'],['-DMULIB_VEC_H'])]:
    p=build(libraries,failure,flags)
    assert p.returncode!=0 and not failure.exists(),p.stderr
    if flags==['-DMULIB_VEC_H']:assert 'EXOTRACT-REQUIRED-MISSING' in p.stderr,p.stderr
assert not list(a.work.glob('.exotract-*'))

# Locked archive consumption uses the same kernel/fixture plan as direct builds.
helper=a.root/'tools/on1x-build.py'
source=a.work/'http3-registry.on1x'
source.write_text('''import std.mulib as M;
fn main(){assert(M.has_kernel("quic")&&M.has_kernel("http3")&&M.has_kernel("tls"));
println("HTTP/3 native plan OK");}''')
for label, selection in [('direct', ['--mulib', 'http3']),
                         ('archive', ['--mulib-archive', a.work/'http3'])]:
    exe=a.work/('http3-registry-'+label)
    report=a.work/('http3-registry-'+label+'.json')
    p=run([sys.executable,helper,source,'--on1xc',a.on1xc,'--cc',a.cc,
           *selection,'--mulib-plan',report,'-o',exe,*['--cflag='+f for f in a.cflag]])
    assert p.returncode==0,p.stderr
    p=run([exe]);assert p.returncode==0 and p.stdout=='HTTP/3 native plan OK\n',(p.stdout,p.stderr)
exe=a.work/'archive_json'
p=run([sys.executable,helper,a.root/'examples/json.on1x','--on1xc',a.on1xc,'--cc',a.cc,
       '--mulib-archive',a.work/'core','-o',exe,*['--cflag='+f for f in a.cflag]])
assert p.returncode==0,p.stderr
p=run([exe]);assert p.returncode==0 and p.stdout=='3\n{"items":[1,2,3],"ok":true}\n',p.stderr
p=run([sys.executable,helper,a.root/'examples/json.on1x','--on1xc',a.on1xc,'--cc',a.cc,
       '--mulib-archive',a.work/'core','-o',exe,*['--cflag='+f for f in a.cflag],'--cflag=-fshort-enums'])
assert p.returncode!=0 and 'flags/target differ' in p.stderr,p.stderr
source=a.work/'mune.on1x'
source.write_text('''import std.mulib as M;
extern fn on1x_mune_contract_semantics()->i64;
fn main(){assert(M.has_kernel("mune")&&M.has_kernel("jit"));
assert(on1x_mune_contract_semantics()==1);println("mune On1x OK");}''')
exe=a.work/'mune-app'
p=run([sys.executable,helper,source,'--on1xc',a.on1xc,'--cc',a.cc,'--mulib-archive',a.work/'mune',
       '--extra-c',a.root/'tests/mulib/mune.c','-o',exe,*['--cflag='+f for f in a.cflag]])
assert p.returncode==0,p.stderr
p=run([exe]);assert p.returncode==0 and p.stdout=='mune On1x OK\n',(p.stdout,p.stderr)
source=a.root/'tests/mulib/elvi-scrape.on1x'
exe=a.work/'elvi-scrape-app'
p=run([sys.executable,helper,source,'--on1xc',a.on1xc,'--cc',a.cc,'--mulib-archive',a.work/'elvi-scrape',
       '--extra-c',a.root/'tests/mulib/elvi-scrape.c','-o',exe,*['--cflag='+f for f in a.cflag]])
assert p.returncode==0,p.stderr
p=run([exe]);assert p.returncode==0 and p.stdout=='elvi providers OK\n',(p.stdout,p.stderr)
for kernel,fixtures,adapter,text,expected in [
 ('yantlr',['yantlr'],'yantlr.c','''import std.mulib as M;
 extern fn fixture_yantlr_artifacts(source:string)->i64;
 fn main(){println(fixture_yantlr_artifacts(M.require_fixture("library/yantlr/example.y1")));}''','1\n'),
 ('shiraz',[],'shiraz.c','''import std.mulib as M;
 extern fn fixture_shiraz_registry()->bool;
 fn main(){assert(M.has_kernel("kvdb"));println(fixture_shiraz_registry());}''','true\n'),
]:
    dest=a.work/kernel
    if dest.exists():shutil.rmtree(dest)
    p=build([kernel],dest,fixtures=fixtures);assert p.returncode==0,(kernel,p.stderr)
    plan=json.loads((dest/'build-plan.json').read_text())
    assert plan['fixtures'] and plan['implementation_translation_units']==1
    symbols=json.loads((dest/'symbols.json').read_text())['defined_symbols']
    if kernel=='yantlr':
        assert 'mulib_yantlr_stdlib_create' in symbols
        binding=json.loads((dest/'fixture-yantlr-mulib-yantlr-stdlib.on1x-import.json').read_text())
        assert binding['cdecl']['complete'] and binding['unsafe_by_default']
    else:
        assert 'mulib_shiraz_runtime_create' in symbols
        assert run(['ar','t',dest/'libmulib.a']).stdout.splitlines()==['mulib.o','fixture_1.o','fixture_2.o','fixture_3.o']
    source=a.work/(kernel+'.on1x');source.write_text(text)
    exe=a.work/(kernel+'-app')
    args=[sys.executable,helper,source,'--on1xc',a.on1xc,'--cc',a.cc,'--mulib-archive',dest,
          '--extra-c',a.root/'tests/mulib'/adapter,'-o',exe,*['--cflag='+f for f in a.cflag]]
    p=run(args);assert p.returncode==0,p.stderr
    p=run([exe]);assert p.returncode==0 and p.stdout==expected,(p.stdout,p.stderr)
    fixture=dest/plan['files'][0]['path'];original=fixture.read_bytes();fixture.write_bytes(original+b'\n')
    before=exe.read_bytes();p=run(args)
    assert p.returncode!=0 and 'artifact drift' in p.stderr and exe.read_bytes()==before,p.stderr
    fixture.write_bytes(original)

# Standard-library bridge kernels share the locked implementation archive. The
# adapter TU contains declarations/direct calls, not another implementation set.
dest=a.work/'stdlib-bridge'
if dest.exists():shutil.rmtree(dest)
p=build(['json','yaml','toml','ini','cryptohash','socket'],dest);assert p.returncode==0,p.stderr
source=a.work/'stdlib-bridge.on1x'
source.write_text('''import std.json.normalize as J;import std.ini.ini as I;import std.crypto.crypto as C;
fn main(){let(status,text)=J.basis_json_normalize("[1,2,3]");assert(status==0);println(text);
let(found,empty)=I.basis_ini_get("x=\\n","","x");assert(found==0&&empty=="");
let(ok,hash)=C.basis_crypto_blake2s("abc");assert(ok==0);println(hash);}''')
exe=a.work/'stdlib-bridge-app'
p=run([sys.executable,helper,source,'--on1xc',a.on1xc,'--cc',a.cc,'--mulib-archive',dest,
       '-o',exe,*['--cflag='+f for f in a.cflag]])
assert p.returncode==0,p.stderr
p=run([exe]);assert p.returncode==0 and p.stdout=='[1,2,3]\n508c5e8c327c14e2e1a72ba34eeb452f37458b209ed63a294d999b4c86675982\n',(p.stdout,p.stderr)
print('Exotract inventory, typed manifests, static archives, C11/On1x clients and rollback PASS')
