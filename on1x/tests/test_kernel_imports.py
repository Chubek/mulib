#!/usr/bin/env python3
"""The intrinsic source-request path is useful, but cannot authorize compilation."""
import hashlib
import json
import os
import sys

from support import NativeTests, run

t = NativeTests()

def inspect(path):
    p = run([t.compiler, '--kernel-imports', '--no-prelude', path])
    assert p.returncode == 0, p.stderr
    return json.loads(p.stdout)

def request(path, line, offset, module, alias, unsafe=False):
    return dict(file_hex=os.fsencode(path.resolve()).hex(), line=line, column=1, offset=offset,
                module=module, alias=alias, unsafe=unsafe)

# Ordinary `kernel`, `foreign` and `unsafe` identifiers still compile and execute.
t.source('kernel', 'fn value()->i64{return 42;}')
t.source('foreign', 'fn value()->i64{return 42;}')
ordinary = t.source('ordinary', '''import kernel as Kernel;
 import foreign as F;
 fn unsafe()->i64 {return Kernel.value()+F.value()-42;}
fn main(){let kernel=unsafe();println(kernel);}
''')
assert inspect(ordinary)['requests'] == []
t.valid('ordinary', '42\n', source=ordinary)

sub = t.source('child', 'import kernel mulib.histprice as History;\nfn value()->i64{return 42;}')
entry = t.source('imports', '''import child as Child;
unsafe import kernel mulib.json;
import kernel mulib.histprice as FromEntry;
import kernel mulib.histprice as AlsoFromEntry;
fn main(){println(Child.value());}
''')
data = inspect(entry)
lines = entry.read_bytes().splitlines(keepends=True)
offset = lambda line: sum(map(len, lines[:line-1]))
assert data['schema'] == 'on1x.kernel-requests/v1'
assert data['requests'] == [
    request(sub, 1, 0, 'mulib.histprice', 'History'),
    request(entry, 2, offset(2), 'mulib.json', 'json', True),
    request(entry, 3, offset(3), 'mulib.histprice', 'FromEntry'),
    request(entry, 4, offset(4), 'mulib.histprice', 'AlsoFromEntry'),
], data
assert data == inspect(entry)
assert {s['file_hex']: s['sha256'] for s in data['sources']} == {
    os.fsencode(path.resolve()).hex(): hashlib.sha256(path.read_bytes()).hexdigest() for path in (sub, entry)}

# An unsafe alias in the same unit cannot make resources visible via a safe
# alias, including as types, even in review staging.
raw = 'unsafe import kernel mulib.ipc_framed as Raw;\n'
path = t.source('safe_resource', raw+'''import kernel mulib.ipc_framed as Safe;
fn use_type(s:own<Safe.Session>){drop(s);}
fn main(){unsafe { Safe.open_session(); }}
''')
for flags in ([], ['--kernel-c11'], ['--kernel-c11', '--kernel-stage']):
    p = run([t.compiler, '--no-prelude', '--emit-c', *flags, path])
    assert p.returncode == 1 and 'ON1X-KERNEL-NOT-ADMITTED' in p.stderr and 'unsafe-only' in p.stderr and not p.stdout, p
path = t.source('safe_resource', 'import kernel mulib.ipc_framed as Safe;\nfn main(){}')
p = run([t.compiler, '--no-prelude', '--emit-c', '--kernel-c11', path])
assert p.returncode == 1 and 'ON1X-KERNEL-NOT-ADMITTED' in p.stderr and not p.stdout, p

for name, text, flags, diagnostic in (
    ('raw_available', raw+'''fn main(){
      let result=unsafe { Raw.open_session() };
      let session=match result with { Ok(s)=>move(s), Err(e)=>{println(e); return;} };
      drop(session);
    }''',
     [], 'on1x_kernel_ipc_v1_open_session'),
    ('raw_context', raw+'fn main(){ Raw.open_session(); }', [], 'ON1X-KERNEL-CONTEXT'),
    ('raw_member', raw+'fn main(){ unsafe { Raw.init(); } }', [], 'no ABI-verified export'),
    ('raw_dynlib', 'unsafe import kernel[dynlib] mulib.ipc_framed;\nfn main(){}', [], 'requires the static KernelBind adapter'),
    ('raw_json', 'unsafe import kernel mulib.json;\nfn main(){}', [], 'no ABI-verified unsafe import support'),
):
    path = t.source(name, text)
    p = run([t.compiler, '--no-prelude', '--emit-c', '--kernel-c11', *flags, path])
    if name == 'raw_available':
        assert p.returncode == 0 and diagnostic in p.stdout, p
    else:
        assert p.returncode == 1 and diagnostic in p.stderr and not p.stdout, p

for mode in ('--check', '--emit-c', '--emit-ast', '--emit-ir', '--deps'):
    output = t.work/'unpublished'
    p = run([t.compiler, mode, entry, '-o', output])
    assert p.returncode == 1 and f'{entry.resolve()}:2:1:' in p.stderr and 'ON1X-KERNEL-UNSAFE-UNSUPPORTED' in p.stderr, p
    assert not output.exists() and not p.stdout, p

transitive = t.source('transitive', 'import child;fn main(){}')
p = run([t.compiler, '--emit-c', transitive])
assert p.returncode == 1 and f'{sub.resolve()}:1:1:' in p.stderr and 'ON1X-KERNEL-NOT-ADMITTED' in p.stderr, p
safe = t.source('safe', 'import kernel mulib.histprice as H;fn main(){println(H.holt(0));}')
for flags in ([], ['--emit-only'], ['--via-mir']):
    output = t.work/'unpublished'
    p = run([sys.executable, t.root/'tools/on1x-build.py', safe, '--on1xc', t.compiler, '-o', output, *flags])
    assert p.returncode == 1 and 'ON1X-KERNEL-NOT-ADMITTED' in p.stderr and not output.exists(), p

for name, text, diagnostic in (
    ('bad_linkage', 'import kernel[dynamic] mulib.clamp;', "expected 'dynlib'"),
    ('unclosed_linkage', 'unsafe import kernel[dynlib mulib.clamp;', "expected ']'"),
    ('missing_semi', 'import kernel mulib.json as J\nfn main(){}', "expected ';'"),
    ('missing_alias', 'import kernel mulib.json as;', 'expected identifier'),
    ('quoted_kernel', 'import kernel "../quote";', 'expected identifier'),
    ('traversal_safe', 'import kernel ../quote;', 'expected identifier'),
    ('traversal', 'unsafe import kernel ../quote;', 'expected identifier'),
    ('unsafe_relative', 'unsafe import kernel mulib../quote;', "expected ';'"),
    ('duplicate', 'import kernel mulib.json as J; import kernel mulib.json as J;', 'conflicting kernel import alias: J'),
    ('ordinary_alias', 'import child as J; import kernel mulib.json as J;', 'conflicting kernel import alias: J'),
    ('function_alias', 'import kernel mulib.json as J; fn J(){}', 'conflicting kernel import alias: J'),
    ('limitations', ''.join(f'import kernel mulib.json as J{i};\n' for i in range(1025)), 'import count limit'),
):
    path = t.source(name, text)
    p = run([t.compiler, '--kernel-imports', path])
    assert p.returncode == 1 and str(path.resolve())+':' in p.stderr and diagnostic in p.stderr and not p.stdout, p

# The request stage must not run processors or accept these nodes off the wire.
marker = t.work/'processor-ran'
marker.unlink(missing_ok=True)
processor = t.work/'processor.sh'
processor.write_text(f'#!/bin/sh\ntouch "{marker}"\nprintf \'(Unit 1 ((KernelImport "mulib.json")))\\n\'\n')
processor.chmod(0o755)
scanner_only = t.source('source_ppx', '@ppx tripwire "processor.sh";\nimport kernel mulib.json;\nfn main(){}')
assert len(inspect(scanner_only)['requests']) == 1
assert not marker.exists()
p = run([t.compiler, '--check', '--allow-source-ppx', scanner_only])
assert p.returncode == 1 and 'ON1X-KERNEL-NOT-ADMITTED' in p.stderr and not marker.exists(), p
p = run([t.compiler, '--check', ordinary, '--ppx', 'tripwire', processor])
assert p.returncode == 1 and 'KernelImport' in p.stderr and marker.exists(), p

# Staged ports have real source requests, but none is a working program
# until its projections are admitted. Change this gate when promoting a port.
migrations = t.root/'tests/fixtures/kernel_migrations'
for relative, expected in {
    'data/ini.on1x': [('mulib.ini', 'Ini')],
    'data/json.on1x': [('mulib.json', 'Json')],
    'data/json_owner.on1x': [('mulib.json', 'Json')],
    'data/toml.on1x': [('mulib.toml', 'Toml'), ('mulib.json', 'Json')],
    'data/yaml.on1x': [('mulib.yaml', 'Yaml'), ('mulib.json', 'Json')],
    'prettyprint/render.on1x': [('mulib.prettysexpr', 'PrettyKernel')],
    'runtime/channel.on1x': [('mulib.channel', 'Channel')],
    'runtime/crypto.on1x': [('mulib.cryptohash', 'Hash')],
    'runtime/socket.on1x': [('mulib.socket', 'Socket')],
}.items():
    path = migrations/relative
    requests = inspect(path)['requests']
    assert [(r['module'], r['alias']) for r in requests] == expected, requests
    assert all(not r['unsafe'] for r in requests), requests
    p = run([t.compiler, '--no-prelude', '--check', path])
    assert p.returncode == 1 and 'ON1X-KERNEL-NOT-ADMITTED' in p.stderr and not p.stdout, p

clamp = t.root/'examples/kernel/clamp.on1x'
assert [(item['module'], item['unsafe']) for item in inspect(clamp)['requests']] == [('mulib.clamp', True)]
p = run([t.compiler, '--emit-c', clamp])
assert p.returncode == 1 and 'ON1X-KERNEL-UNSAFE-UNSUPPORTED' in p.stderr and not p.stdout, p
p = run([t.compiler, '--emit-c', '--kernel-c11', clamp])
assert p.returncode == 0 and 'unsafe_foreign: mulib_clamp_add_i64' in p.stdout, p
assert '#include <mulib-clamp.h>' in p.stdout and '_Generic(&mulib_clamp_add_i64' in p.stdout
assert '_Generic(&mulib_clamp_i64, int64_t (*)(int64_t, int64_t, int64_t)' in p.stdout
assert 'extern int64_t mulib_clamp' not in p.stdout

for name, source, diagnostic in (
    ('outside', 'fn main(){println(C.add_i64(1,2));}', 'ON1X-KERNEL-CONTEXT'),
    ('nested_lambda', 'fn main(){unsafe { let f=fn(){println(C.add_i64(1,2));}; f(); }}', 'ON1X-KERNEL-CONTEXT'),
    ('value', 'fn main(){unsafe { let f=C.add_i64; }}', 'ON1X-KERNEL-DIRECT'),
    ('unbound_member', 'fn main(){unsafe { println(C.unknown(1,2,3)); }}', 'ON1X-KERNEL-MEMBER'),
    ('interval_arity', 'fn main(){unsafe { println(C.i64(1,2)); }}', 'arity'),
    ('interval_type', 'fn main(){unsafe { println(C.i64(1,2,true)); }}', 'type'),
    ('interval_outside', 'fn main(){println(C.i64(1,2,3));}', 'ON1X-KERNEL-CONTEXT'),
    ('interval_extern', 'extern fn mulib_clamp_i64(v:i64,lo:i64,hi:i64)->i64; fn main(){}', 'ON1X-KERNEL-EXTERN'),
    ('type', 'fn main(){unsafe { println(C.add_i64("1",2)); }}', 'type'),
    ('arity', 'fn main(){unsafe { println(C.add_i64(1)); }}', 'arity'),
    ('extern', 'extern fn mulib_clamp_add_i64(a:i64,b:i64)->i64; fn main(){}', 'ON1X-KERNEL-EXTERN'),
    ('defer', 'fn main(){unsafe { defer println(C.add_i64(1,2)); }}', 'ON1X-KERNEL-PROFILE'),
):
    path = t.source('clamp_'+name, 'unsafe import kernel mulib.clamp as C;\n'+source)
    p = run([t.compiler, '--no-prelude', '--emit-c', '--kernel-c11', path])
    assert p.returncode == 1 and diagnostic in p.stderr and not p.stdout, p
p = run([t.compiler, '--emit-c', '--kernel-c11', clamp, '--ppx', 'tripwire', processor])
assert p.returncode != 0 and 'without PPX' in p.stderr, p

# Both forms are contextual; expression tails evaluate inside the block and
# a closure body must opt in even when construction occurs inside the block.
expr = t.source('unsafe_expression', '''unsafe import kernel mulib.clamp as C;
fn answer() -> i64 {
  return unsafe {
    let x = 40;
    C.add_i64(x, unsafe { C.sub_i64(4, 2) })
  };
}
fn main(){
  var total = 0;
  let value = unsafe { total = answer(); total };
  let thunk = unsafe { fn()->i64 { return unsafe { C.add_i64(40,2) }; } };
  unsafe { println(thunk()); }
  println(value);
}
''')
assert inspect(expr)['requests'][0]['unsafe']
p = run([t.compiler, '--no-prelude', '--emit-c', '--kernel-c11', expr])
assert p.returncode == 0 and 'unsafe_foreign: mulib_clamp_add_i64' in p.stdout, p
for name, source, diagnostic in (
    ('outside_arg', 'fn main(){println(C.add_i64(unsafe { 1 }, 2));}', 'ON1X-KERNEL-CONTEXT'),
    ('following_call', 'fn main(){let x=unsafe { C.add_i64(40,2) }; println(C.add_i64(x,2));}', 'ON1X-KERNEL-CONTEXT'),
    ('tail_lambda', 'fn main(){let f=unsafe { fn(){println(C.add_i64(1,2));} };}', 'ON1X-KERNEL-CONTEXT'),
    ('escaping_local', 'fn main(){let r=unsafe { let private=42; private }; println(private);}', 'undefined'),
    ('void_tail', 'fn main(){let r=unsafe { C.add_i64(40,2); };}', 'cannot bind a void expression'),
    ('unsafe_value', 'fn main(){let f=unsafe { C.add_i64 };}', 'ON1X-KERNEL-DIRECT'),
    ('unterminated', 'fn main(){let r=unsafe { 1;', 'unterminated value block'),
    ('depth', 'fn main(){let r='+('unsafe {'*300)+'42'+('}'*300)+';}', 'limit'),
):
    path = t.source('unsafe_expr_'+name, 'unsafe import kernel mulib.clamp as C;\n'+source)
    p = run([t.compiler, '--no-prelude', '--emit-c', '--kernel-c11', path])
    assert p.returncode == 1 and diagnostic in p.stderr and not p.stdout, p
no_import = t.source('unsafe_expr_no_profile', 'fn main(){println(unsafe { 42 });}')
p = run([t.compiler, '--check', no_import])
assert p.returncode == 1 and 'ON1X-KERNEL-PROFILE' in p.stderr, p
