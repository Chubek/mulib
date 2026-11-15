#!/usr/bin/env python3
"""Chisel HIR admission and actual MuLIB C11 execution."""
import sys
from support import NativeTests, run
t = NativeTests()
source = t.root/'examples/memory.on1x'
report = run([t.compiler, '--check-memory', source])
assert report.returncode == 0 and 'on1x.memory/v1' in report.stdout, report.stderr
assert report.stdout == run([t.compiler, '--check-memory', source]).stdout
code = t.work/'checked.c'
p = run([t.compiler, '--emit-c', '--memory-check', source, '-o', code])
assert p.returncode == 0, p.stderr
assert code.read_text() == run([t.compiler, '--emit-c', source]).stdout
exe = t.work/'memory'
p = run([sys.executable, t.root/'tools/on1x-build.py', source, '--memory-check',
         '--on1xc', t.compiler, '--cc', t.cc, '-o', exe, *['--cflag='+f for f in t.flags]])
assert p.returncode == 0, p.stderr
p = run([exe]);assert p.returncode == 0 and p.stdout == '2\n1\n3\n3\n3\n', p.stderr
for name, text, diagnostic in [
    ('extern', 'extern fn opaque(); fn main(){opaque();}', 'raw external'),
    ('closure', 'fn main(){let f=fn()->i64{return 1;};println(f());}', 'memory model unavailable'),
    ('indirect', 'fn f()->i64{return 1;}fn main(){let g=f;println(g());}', 'first-class callable'),
    ('temporary', 'fn main(){println(json_size(json_parse_or_die("{}")));}', 'named live owner'),
    ('buffer', 'fn main(){let b=buffer_new();}', 'intrinsic memory model unavailable'),
]:
    path=t.source(name,text);code.write_text('keep')
    p=run([t.compiler,'--emit-c','--memory-check',path,'-o',code])
    assert p.returncode == 1 and diagnostic in p.stderr and code.read_text() == 'keep', (name,p.stderr)
for options in [['--tokens'],['--emit-ast'],['--ir-input','--emit-ir']]:
    p=run([t.compiler,*options,'--memory-check',source]);assert p.returncode==2,p.stderr
print('Chisel checked HIR, deterministic admission, output preservation and real MuLIB execution PASS')
