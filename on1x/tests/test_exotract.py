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
def build(names,dest,flags=()):
    return run(command('build',*names,'--output',dest,'--exotract',a.exotract,
                       '--compiler',a.cc,'--resource-dir',resource,
                       *['--cflag='+f for f in [*a.cflag,*flags]]))
def client(path,dest,expected,extra=()):
    exe=dest/'client'
    p=run([a.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',*a.cflag,path,
           '-I',dest,'-I',a.root.parent/'include',*extra,dest/'libmulib.a','-lm','-o',exe])
    assert p.returncode==0,p.stderr
    p=run([exe]);assert p.returncode==0 and p.stdout==expected,(p.stdout,p.stderr)
data=json.loads(run(command('list')).stdout)
assert len(data['modules'])+len(data['auxiliary_headers'])==len(list((a.root.parent/'include').glob('mulib-*.h')))
assert 'b+tree' in data['modules'] and 'microjs' in data['modules']
p=run(command('plan','json','vec','parsec'));assert p.returncode==0,p.stderr
plan=json.loads(p.stdout)
assert plan['implementation_macros']==['MULIB_IMPL_PARSEC','MULIB_IMPL_STRBUF','MULIB_IMPL_UTF8','MULIB_IMPL_JSON','MULIB_IMPL_VEC']
assert plan['safe_export'] is False and plan['implementation_translation_units']==1
for name,libraries,source,expected in [
    ('core',['json','vec','parsec'],'client.c','[42]\n'),
    ('microjs',['microjs'],'microjs.c','42\n'),
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
    else:assert len(symbols['microjs']['archive_exports'])==15
    client(a.root/'contracts-exotract/examples'/source,dest,expected)
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
print('Exotract inventory, typed manifests, static archives, C11/On1x clients and rollback PASS')
