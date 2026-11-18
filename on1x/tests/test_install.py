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
    env={k:v for k,v in os.environ.items() if k not in (
        'ON1X_STDLIB','ON1X_PATH','ON1X_PPX_SCHEMA','ON1X_LIB_PATH','ON1X_STDLIB_PATH',
        'ON1X_PPX_PATH','ON1X_STDPPX_PATH','ON1X_MULIB_PATH','ON1X_MULIB_ROOT')}
    assert (library/'stdlib/On1xfile').is_file()
    # Formatting is linked into on1xc and uses the file-local configuration
    # after relocation; no checkout cwd or on-demand build tool is needed.
    fmt_dir=base/'format'
    fmt_dir.mkdir()
    (fmt_dir/'.on1xfmt').write_text('tabstop = 4\n')
    fmt_source=fmt_dir/'listing.on1x'
    fmt_source.write_text('fn main(){println(42);}\n')
    expected='fn main() {\n    println(42);\n}\n'
    p=run([compiler,'fmt',fmt_source],cwd=base,env=env)
    assert p.returncode==0 and p.stdout==expected,p
    fmt_output=fmt_dir/'formatted.on1x'
    p=run([compiler,'--format',fmt_source,'-o',fmt_output],cwd=base,env=env)
    if os.name == 'posix':
        assert p.returncode==0 and fmt_output.read_text()==expected,p
        p=run([compiler,'fmt',fmt_output,'--check'],cwd=base,env=env)
        assert p.returncode==0 and not p.stdout,p
        p=run([compiler,'fmt',fmt_source,'--check'],cwd=base,env=env)
        assert p.returncode==1 and not p.stdout,p
        fmt_source.write_text('fn main( malformed\n')
        p=run([compiler,'fmt',fmt_source,'-o',fmt_output],cwd=base,env=env)
        assert p.returncode!=0 and fmt_output.read_text()==expected,p
    p=run([compiler,'--deps',source],cwd=base,env=env);assert p.returncode == 0,p.stderr
    assert str(library/'stdlib/Int.on1x') in p.stdout and str(t.root/'stdlib') not in p.stdout,p.stdout
    exe=base/'client'
    p=run([sys.executable,library/'tools/on1x-build.py',source,'--on1xc',compiler,
           '--cc',t.cc,'--stdppx','pipe','-o',exe,*[f'--cflag={flag}' for flag in t.flags]],env=env,cwd=base)
    assert p.returncode == 0,p.stderr
    p=run([exe]);assert p.returncode == 0 and p.stdout == '7\n',p.stderr
    if (relocated/'libexec/on1x/on1x-ppx-identity').is_file():
        assert (relocated/'libexec/on1x/On1xfile').is_file()
        p=run([compiler,'--check',source,'--stdppx','pipe','--use-ppx','identity',
               '--ppx-path',relocated/'libexec/on1x'],env=env,cwd=base)
        assert p.returncode==0,p.stderr
    if (relocated/'libexec/on1x/on1x-ppx-LuaEmbed').is_file():
        source.write_text('fn main(){println(@LuaEmbed("expr",'
                          '"llembed.emit(llembed.integer(42))"));}')
        p=run([sys.executable,library/'tools/on1x-build.py',source,'--on1xc',compiler,
               '--cc',t.cc,'--use-ppx','LuaEmbed','--stdppx-path',relocated/'libexec/on1x',
               '-o',exe,*[f'--cflag={flag}' for flag in t.flags]],env=env,cwd=base)
        assert p.returncode==0,p.stderr
        p=run([exe],cwd=base,env=env)
        assert p.returncode==0 and p.stdout=='42\n',p.stderr
    if (relocated/'libexec/on1x/on1x-ppx-GrammarEmbed').is_file():
        source.write_text('@GrammarEmbed("grammar","grammar Installed {rule TOP {\'ok\'}}") '
                          'fn marker(){}fn main(){println(Installed_matches("ok"));}')
        p=run([sys.executable,library/'tools/on1x-build.py',source,'--on1xc',compiler,
               '--cc',t.cc,'--use-ppx','GrammarEmbed','--stdppx-path',relocated/'libexec/on1x',
               '-o',exe,*[f'--cflag={flag}' for flag in t.flags]],env=env,cwd=base)
        assert p.returncode==0,p.stderr
        p=run([exe],cwd=base,env=env)
        assert p.returncode==0 and p.stdout=='true\n',p.stderr
    source.write_text('from std import mulib;fn main(){assert(mulib.fits_size(42));println(mulib.abi_version());}')
    p=run([compiler,'--deps',source],cwd=base,env=env);assert p.returncode==0,p.stderr
    assert str(library/'stdlib/mulib/__init__.on1x') in p.stdout and str(t.root/'stdlib/mulib') not in p.stdout
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
    if sys.platform.startswith('linux'):
        project=base/'relocated-project'
        p=run([compiler,'init','--bin',project],cwd=base,env=env)
        assert p.returncode==0,p.stderr
        p=run([compiler,'exec','build','--cc',t.cc],cwd=project,env=env)
        assert p.returncode==0,p.stderr
        generated=[item for item in (project/'target/dev').iterdir()
                   if item.is_file() and os.access(item,os.X_OK)]
        assert len(generated)==1,generated
        p=run([generated[0]],cwd=base,env=env)
        assert p.returncode==0,p.stderr
print('Relocated compiler, prelude, stdlib, stdppx, formatter and project driver: PASS')
