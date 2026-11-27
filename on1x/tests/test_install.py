"""A relocated installed compiler finds its own libraries without checkout paths."""
import json
import os
import re
import sys
import tempfile
from hashlib import sha256
from pathlib import Path
from support import NativeTests, run

t=NativeTests()
with tempfile.TemporaryDirectory(dir=t.work,prefix='install-') as temporary:
    base=Path(temporary);prefix=base/'original';relocated=base/'relocated'
    p=run(['cmake','--install',t.compiler.parent,'--prefix',prefix]);assert p.returncode == 0,p.stderr
    prefix.rename(relocated)
    compiler=relocated/'bin/on1xc';library=relocated/'share/on1x'
    source=base/'client.on1x'
    source.write_text('''import std.Int as I;
import std.collections.ListPair as P;
fn main(){
 let items=sx_cons(sx_string("installed"),sx_nil());
 let mapped=P.basis_listpair_map_eq(items,items,fn(a:sexpr,b:sexpr)->sexpr{return b;});
 assert(sx_text(sx_head(mapped))=="installed");
 println(@pipe("apply",identity(-7),I.abs));
}''')
    env={k:v for k,v in os.environ.items() if k not in (
        'ON1X_STDLIB','ON1X_PATH','ON1X_PPX_SCHEMA','ON1X_LIB_PATH','ON1X_STDLIB_PATH',
        'ON1X_PPX_PATH','ON1X_STDPPX_PATH','ON1X_MULIB_PATH','ON1X_MULIB_ROOT',
        'MULIB_FOREIGNLIB_PATH')}
    assert (library/'stdlib/On1xfile').is_file()
    # The selector library is embedded; it must not depend on the external
    # LibStone checkout, current directory or an installed Lua search path.
    unsupported=run([compiler,'--print-stdlib','--diagnostic-disable','all'],cwd=base,env=env)
    if unsupported.returncode==0:
        invalid=base/'invalid.on1x';artifact=base/'invalid.c'
        invalid.write_text('fn main()->i64 { return true; }\n', encoding='utf-8')
        p=run([sys.executable,library/'tools/on1x-build.py',invalid,'--on1xc',compiler,
               '--cc',t.cc,'--emit-only','-o',artifact,'--diagnostic-disable','all'],cwd=base,env=env)
        assert p.returncode==1 and not p.stderr and not artifact.exists(),p
    else:
        assert unsupported.returncode==2 and 'require the LibStone ASDL frontend' in unsupported.stderr,unsupported
    for installed_recipe in sorted((library/'stdffi/mulib').glob('*.ffi')):
        if not re.fullmatch(r'[a-z][a-z0-9]*', installed_recipe.stem):
            continue
        portable=base/'portable.on1x'
        portable.write_text(f'import foreign stdffi.{installed_recipe.stem}; fn main(){{}}')
        for foreign_env in (env,{**env,'MULIB_FOREIGNLIB_PATH':''}):
            p=run([compiler,'--check',portable],cwd=base,env=foreign_env)
            assert p.returncode==0,p
        # The compiler must read this install, not silently use the checkout.
        original_recipe=installed_recipe.read_bytes()
        try:
            installed_recipe.write_text('(ffispec 1 (kernel broken))')
            p=run([compiler,'--check',portable],cwd=base,env=env)
            assert p.returncode==1 and 'ON1X-STDFFI-SPEC' in p.stderr and str(installed_recipe) in p.stderr,p
        finally:
            installed_recipe.write_bytes(original_recipe)
        break
    sdk_tool=relocated/'bin/on1x-sdk'
    if sdk_tool.is_file() and os.name=='posix':
        direct=run([sdk_tool,'info'],cwd=base,env=env)
        via_driver=run([compiler,'sdk','info'],cwd=base,env=env)
        assert direct.returncode==via_driver.returncode==0,(direct,via_driver)
        assert via_driver.stdout==direct.stdout,via_driver
        assert json.loads(via_driver.stdout)['sdk_version']=='2.0.0'
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
    assert str(library/'stdlib/collections/ListPair.on1x') in p.stdout,p.stdout
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
        p=run([sys.executable,'-B',t.root/'tests/test_http_probe.py',
               '--on1xc',compiler,'--cc',t.cc,'--stdlib',library/'stdlib',
               '--runtime',library/'runtime','--example',library/'examples/http-probe',
               '--scratch',base,*[f'--cflag={flag}' for flag in t.flags]],
              cwd=base,env=env)
        assert p.returncode==0,p.stderr
        source.write_text('''import std.container.Box as B;
import std.container.Tls as T;
fn main() {
  let answer = B.basis_box_new(42);
  assert(T.basis_tls_i64_set(B.basis_box_into_inner(move(answer))));
  println(match T.basis_tls_i64_current() with { Some(n) => n, None() => 0 });
}
''', encoding='ascii')
        plan=base/'container-link.json'
        p=run([sys.executable,library/'tools/on1x-build.py',source,'--on1xc',compiler,
               '--mulib-root',t.root.parent,'--cc',t.cc,'-o',exe,'--mulib-plan',plan,
               *[f'--cflag={flag}' for flag in t.flags]],env=env,cwd=base)
        assert p.returncode==0,p.stderr
        pins=json.loads(plan.read_text())['inputs']
        assert str(library/'stdlib/container/runtime.c') in pins
        assert str(library/'stdlib/container/runtime.h') in pins
        p=run([exe],cwd=base,env=env);assert p.returncode==0 and p.stdout=='42\n',p
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
        (project/'src/main.on1x').write_text('fn main(){println(42);}\n')
        for backend, tool in [('mir-archon', 'on1x-archon-mir'), ('mir-vm', 'on1x-vm')]:
            shipped = (relocated/'bin'/tool).is_file()
            built = (t.compiler.parent/('backends/mir-archon' if backend=='mir-archon' else '.')/tool).is_file()
            assert shipped == built, (backend, 'runner missing from install')
            if shipped:
                p=run([compiler,'exec','build','--backend',backend,'--cc',t.cc],cwd=project,env=env)
                if backend=='mir-archon' and run([relocated/'bin'/tool,'--print-target']).stdout.strip()!='x86_64-unknown-linux-gnu':
                    assert p.returncode!=0 and 'mir-archon unavailable' in p.stderr,p
                    continue
                assert p.returncode==0,(backend,p.stderr)
                out=Path(p.stdout.strip())
                report=json.loads((out/'.on1x-build.json').read_text())
                assert report['backend']['name']==backend
                key='archon' if backend=='mir-archon' else 'vm_runner'
                assert report['tools'][key]==sha256((relocated/'bin'/tool).read_bytes()).hexdigest()
                if backend=='mir-archon':
                    manifest=library/'backends/mir-archon/BackendManifest.json'
                    assert report['tools']['archon_manifest']==sha256(manifest.read_bytes()).hexdigest()
                    assert report['tools']['runtime']==sha256(
                        (relocated/'lib/libon1x_runtime.a').read_bytes()).hexdigest()
                p=run([out/'relocated-project'],cwd=base,env=env)
                assert (p.returncode,p.stdout)==(0,'42\n'),(backend,p)
print('Relocated compiler, prelude, stdlib, stdppx, formatter and project driver: PASS')
