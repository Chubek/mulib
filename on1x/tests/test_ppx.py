from support import NativeTests, run
from pathlib import Path
import json
import os
import sys
import time

t=NativeTests()
root=t.root.resolve()
sys.path.insert(0,str(root/'stdlib'))
from sexpr import Atom,loads,dumps
from ppx import Schema
from visitor import rewrite,tag

fixture=root/'tests/fixtures/ppx_transformer.py'
def options(*labels):
    return [item for label in labels for item in ['--ppx',label,fixture]]

def native(name,expected,*,text=None,source=None,flags=(),status=0,diagnostic=None):
    source=t.source(name,text) if text is not None else source
    output=t.work/(name+'.c')
    p=run([t.compiler,'--emit-c',source,'-o',output,*flags])
    assert p.returncode==0,(name,p.returncode,p.stderr)
    exe=t.work/name
    p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I',root/'runtime',output,root/'runtime/on1x_runtime.c',*t.flags,'-o',exe])
    assert p.returncode==0,(name,p.stderr)
    p=run([exe])
    assert p.returncode==status and p.stdout==expected,(name,p.returncode,p.stdout,p.stderr)
    if diagnostic:
        assert diagnostic in p.stderr,p.stderr
    return exe

native('stdppx','42\ntrue\ntrue\n2\nfn(i64, i64) -> i64\n',source=root/'examples/ppx.on1x',flags=['--allow-source-ppx'])
simple='fn main(){println(20);}'
native('ordered_chain','42\n',text=simple,flags=options('increment','double'))
native('cli_label','',text='fn main(){@my_checks("assert_eq",42,42);}',flags=['--ppx','my_checks',root/'stdppx/checks.py'])
native('checks_shadow','',text='fn main(){let assert=fn(x:bool){};@c("assert_eq",1,2);}',flags=['--ppx','c',root/'stdppx/checks.py'],status=70,diagnostic='assertion failed')
native('wire_unicode','snow: 雪\n',text='fn main(){println("snow: 雪");}',flags=['--ppx','identity',root/'stdppx/identity.py'])
native('unroll_scopes','1\n1\n',text='fn main(){@u("repeat",2,{let x=1;println(x);});}',flags=['--ppx','u',root/'stdppx/unroll.py'])
native('fold_negative','-1\n-2\n',text='fn main(){println((0-7)%3);println((0-7)/3);}',flags=['--ppx','fold',root/'stdppx/fold.py'])
native('fold_overflow','',text='fn main(){println(9223372036854775807+1);}',flags=['--ppx','fold',root/'stdppx/fold.py'],status=70,diagnostic='addition overflow')
native('fold_zero_division','',text='fn main(){println(1/0);}',flags=['--ppx','fold',root/'stdppx/fold.py'],status=70,diagnostic='invalid division')

source=t.source('protocol_input',simple)
for mode,diagnostic in [
    ('malformed','S-expression'),('wrong_version','$.version'),('wrong_field','$.functions'),
    ('internal_node','unknown constructor'),('duplicate','duplicate function'),('unknown','unknown constructor'),
    ('unbound_type','unbound quantified'),('trailing','trailing input'),('bad_semantics','expected i64'),
    ('empty','empty stdout'),('exit_error','exit status 23'),('flood','stdout byte limit'),
    ('stderr_flood','stderr byte limit'),('deep','depth limit'),('timeout','timeout'),
    ('huge_binders','binder count limit'),
]:
    p=run([t.compiler,'--emit-c',source,*options(mode),'--ppx-timeout-ms','200' if mode=='timeout' else '5000'])
    assert p.returncode==1 and diagnostic in p.stderr and not p.stdout,(mode,p.returncode,p.stderr)

for name, text, flags, diagnostic in [
    ('source_label_limit','@ppx '+'x'*129+' "program"\nfn main(){}',[], 'label exceeds 128'),
    ('extension_label_limit','fn main(){@'+'x'*129+'("tag");}',[], 'label exceeds 128'),
    ('source_program_limit',''.join(f'@ppx p{i} "program"\n' for i in range(33))+'fn main(){}',[], 'program count limit'),
    ('cli_label_limit',simple,options('x'*129), 'label exceeds 128'),
]:
    p=run([t.compiler,'--check',t.source(name,text),*flags])
    assert p.returncode==1 and diagnostic in p.stderr and not p.stdout,(name,p.returncode,p.stderr)
p=run([t.compiler,'--check',source,*options('x'*128)])
assert p.returncode==0,p.stderr
preserved=t.work/'preserved-output.c'
preserved.write_text('unchanged\n')
p=run([t.compiler,'--emit-c',source,*options('malformed'),'-o',preserved])
assert p.returncode==1 and preserved.read_text()=='unchanged\n'

# Descendants retaining protocol descriptors cannot keep the compiler stuck.
if Path('/proc/self/stat').exists():
    pidfile=t.work/'descendant.pid'
    pidfile.unlink(missing_ok=True)
    p=run([t.compiler,'--check',source,*options('forkhang'),'--ppx-timeout-ms','1000'],
          env=dict(os.environ,ON1X_TEST_PIDFILE=str(pidfile)))
    assert p.returncode==1 and 'timeout' in p.stderr,p.stderr
    assert pidfile.exists(),'descendant fixture did not start'
    stat=Path('/proc')/pidfile.read_text()/'stat'
    for _ in range(100):
        try:state=stat.read_text().split(') ')[1][0]
        except FileNotFoundError:break
        if state=='Z':break
        time.sleep(0.01)
    else:raise AssertionError('PPX process group was not terminated')

# Source registrations cannot execute without permission, including through imports.
marker=t.work/'must-not-exist'
marker.unlink(missing_ok=True)
script=t.work/'script with spaces.py'
script.write_text('#!/usr/bin/env python3\nimport sys\nfrom pathlib import Path\nPath('+repr(str(marker))+').write_text("ran")\nsys.stdout.write(sys.stdin.read())\n')
registered=t.source('registered','@ppx echo "script with spaces.py"\nfn main(){}')
p=run([t.compiler,'--check',registered])
assert p.returncode==1 and '--allow-source-ppx' in p.stderr and not marker.exists(),p.stderr
p=run([t.compiler,'--emit-ast',registered])
assert p.returncode==0 and not marker.exists(),p.stderr
p=run([t.compiler,'--check','--allow-source-ppx',registered])
assert p.returncode==0 and marker.read_text()=='ran',p.stderr
marker.unlink()
importer=t.source('registered_import','import "registered.on1x";')
p=run([t.compiler,'--check',importer])
assert p.returncode==1 and '--allow-source-ppx' in p.stderr and not marker.exists(),p.stderr
p=run([t.compiler,'--check','--allow-source-ppx',importer])
assert p.returncode==0 and marker.exists(),p.stderr
p=run([t.compiler,'--check',source,*options('identity','identity')])
assert p.returncode==1 and 'duplicate PPX label' in p.stderr,p.stderr

# No implicit shell fallback for a text file without a shebang.
shell=t.work/'not a shell program'
shell.write_text('touch '+str(t.work/'shell-was-run'))
shell.chmod(0o700)
p=run([t.compiler,'--check',source,'--ppx','bad_exec',shell])
assert p.returncode==1 and 'exec failed' in p.stderr and not (t.work/'shell-was-run').exists(),p.stderr

# Native host executable transport, independent of shebang handling.
c=t.work/'native.c'
c.write_text('#include <stdio.h>\nint main(void){int c;while((c=getchar())!=EOF){if(putchar(c)==EOF)return 1;}return ferror(stdin)?1:0;}\n')
exe=t.work/'native-transformer'
p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',c,*t.flags,'-o',exe])
assert p.returncode==0,p.stderr
p=run([t.compiler,'--check',source,'--ppx','native',exe])
assert p.returncode==0,p.stderr

# An early-stdin-close transformer must not deliver SIGPIPE to the compiler.
large=t.source('large_input','fn main(){println("'+'x'*100000+'");}')
p=run([t.compiler,'--check',large,*options('ignore_input')])
assert p.returncode==0,(p.returncode,p.stderr)

# Namespaced schema extensions can exist between stages, but cannot reach HIR.
schema_path=t.work/'extension.json'
schema_path.write_text(json.dumps({'version':1,'nodes':{'introduce:Box':{
    'category':'Expression','fields':[['value','Expression'],['location','Location']]}}}))
native('schema_extension','20\n',text=simple,flags=[*options('introduce','consume'),'--ppx-schema','introduce',schema_path])
p=run([t.compiler,'--check',source,*options('introduce'),'--ppx-schema','introduce',schema_path])
assert p.returncode==1 and 'unhandled PPX schema extension' in p.stderr,p.stderr
schema_path.write_text('{"version":1,"nodes":{"Identifier":{"category":"Expression","fields":[]}}}')
p=run([t.compiler,'--check',source,*options('introduce'),'--ppx-schema','introduce',schema_path])
assert p.returncode==1 and 'prefix constructors' in p.stderr,p.stderr

# Canonical cross-language round trips for every source example without a PPX registration.
for example in sorted((root/'examples').glob('*.on1x')):
    if example.name=='ppx.on1x':
        continue
    plain=run([t.compiler,'--emit-c',example])
    echoed=run([t.compiler,'--emit-c',example,'--ppx','identity',root/'stdppx/identity.py'])
    assert plain.returncode==echoed.returncode and plain.stdout==echoed.stdout,(example,plain.stderr,echoed.stderr)
    if plain.returncode:
        assert plain.returncode==1 and 'missing main function' in plain.stderr and 'missing main function' in echoed.stderr,(example,plain.stderr,echoed.stderr)
    ast=run([t.compiler,'--emit-ast',example])
    assert ast.returncode==0,ast.stderr
    tree=loads(ast.stdout);Schema().validate(tree)
    assert dumps(tree)==ast.stdout,example

# Schema/codec errors stay failures rather than being accepted by the bootstrap tools.
for text in ['() ()','(broken','"bad\\q"','('*300+'x'+')'*300]:
    try:loads(text)
    except (ValueError,RecursionError):pass
    else:raise AssertionError(text[:50])
assert loads(dumps(['a\x00\xff',Atom('Symbol')]))==['a\x00\xff',Atom('Symbol')]
print('External PPX, standard processors, schema extensions and bootstrap tools: PASS')
