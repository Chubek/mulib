"""A relocated installed compiler finds its own libraries without checkout paths."""
import os
import sys
import tempfile
from pathlib import Path
from support import NativeTests, run

t=NativeTests()
with tempfile.TemporaryDirectory(dir=t.work,prefix='install-') as temporary:
    base=Path(temporary);prefix=base/'original';relocated=base/'relocated'
    p=run(['cmake','--install',t.compiler.parent,'--prefix',prefix]);assert p.returncode == 0,p.stderr
    prefix.rename(relocated)
    compiler=relocated/'bin/on1xc';library=relocated/'share/on1x'
    source=base/'client.on1x'
    source.write_text('import std.Int as I;fn main(){println(@pipe("apply",identity(-7),I.abs));}')
    env={k:v for k,v in os.environ.items() if k not in ('ON1X_STDLIB','ON1X_PATH','ON1X_PPX_SCHEMA')}
    p=run([compiler,'--deps',source],cwd=base,env=env);assert p.returncode == 0,p.stderr
    assert str(library/'stdlib/Int.on1x') in p.stdout and str(t.root/'stdlib') not in p.stdout,p.stdout
    exe=base/'client'
    p=run([sys.executable,library/'tools/on1x-build.py',source,'--on1xc',compiler,
           '--cc',t.cc,'--stdppx','pipe','-o',exe,*[f'--cflag={flag}' for flag in t.flags]],env=env,cwd=base)
    assert p.returncode == 0,p.stderr
    p=run([exe]);assert p.returncode == 0 and p.stdout == '7\n',p.stderr
    source.write_text('from std import mulib;fn main(){assert(mulib.fits_size(42));println(mulib.abi_version());}')
    p=run([compiler,'--deps',source],cwd=base,env=env);assert p.returncode==0,p.stderr
    assert str(library/'std/mulib/__init__.on1x') in p.stdout and str(t.root/'std/mulib') not in p.stdout
    p=run([sys.executable,library/'tools/on1x-build.py',source,'--on1xc',compiler,
           '--cc',t.cc,'-o',exe,*[f'--cflag={flag}' for flag in t.flags]],env=env,cwd=base)
    assert p.returncode==0,p.stderr
    p=run([exe]);assert p.returncode==0 and p.stdout=='1\n',p.stderr
    source.write_text('import std.mulib as M;fn main(){assert(M.has_kernel("yantlr"));'
                      'assert(M.fixture_size("library/yantlr/example.y1")>0);println("installed fixtures OK");}')
    p=run([sys.executable,library/'tools/on1x-build.py',source,'--on1xc',compiler,
           '--mulib-root',t.root.parent,'--mulib-fixture','yantlr','--cc',t.cc,'-o',exe,
           *[f'--cflag={flag}' for flag in t.flags]],env=env,cwd=base)
    assert p.returncode==0,p.stderr
    p=run([exe],cwd=base);assert p.returncode==0 and p.stdout=='installed fixtures OK\n',p.stderr
    source.write_text('import std.ini.ini as I;fn main(){let(status,value)=I.basis_ini_get("x=42\\n","","x");assert(status==0);println(value);}')
    p=run([sys.executable,library/'tools/on1x-build.py',source,'--on1xc',compiler,
           '--mulib-root',t.root.parent,'--cc',t.cc,'-o',exe,
           *[f'--cflag={flag}' for flag in t.flags]],env=env,cwd=base)
    assert p.returncode==0,p.stderr
    p=run([exe],cwd=base);assert p.returncode==0 and p.stdout=='42\n',p.stderr
print('Relocated compiler, prelude, stdlib, stdppx and build helper: PASS')
