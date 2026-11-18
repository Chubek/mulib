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

# Ordinary `kernel` and `unsafe` identifiers still compile and execute.
t.source('kernel', 'fn value()->i64{return 42;}')
ordinary = t.source('ordinary', '''import kernel as Kernel;
fn unsafe()->i64 {return Kernel.value();}
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
    ('missing_semi', 'import kernel mulib.json as J\nfn main(){}', "expected ';'"),
    ('missing_alias', 'import kernel mulib.json as;', 'expected identifier'),
    ('quoted_kernel', 'import kernel "../quote";', 'expected identifier'),
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
assert 'extern int64_t mulib_clamp' not in p.stdout

for name, source, diagnostic in (
    ('outside', 'fn main(){println(C.add_i64(1,2));}', 'ON1X-KERNEL-CONTEXT'),
    ('nested_lambda', 'fn main(){unsafe { let f=fn(){println(C.add_i64(1,2));}; f(); }}', 'ON1X-KERNEL-CONTEXT'),
    ('value', 'fn main(){unsafe { let f=C.add_i64; }}', 'ON1X-KERNEL-DIRECT'),
    ('unbound_member', 'fn main(){unsafe { println(C.i64(1,2,3)); }}', 'ON1X-KERNEL-MEMBER'),
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
