#!/usr/bin/env python3
"""Compiled cooperative cancellation, cleanup and monotonic select deadlines."""
import os
import re
import time
from support import NativeTests, run

t = NativeTests()


def build(name, source, *flags, extra=()):
    path = t.source(name, source)
    exe = t.work / name
    p = run(['python3', t.root / 'tools/on1x-build.py', path,
             '--on1xc', t.compiler, '--cc', t.cc, '-o', exe,
             *[arg for path in extra for arg in ('--extra-c', path)],
             *[f'--cflag={f}' for f in [*t.flags, *flags]]])
    assert p.returncode == 0, (name, p)
    return exe


def execute(name, source, output='', *flags):
    p = run([build(name, source, *flags)])
    note = "WARNING: ASan doesn't fully support makecontext/swapcontext functions and may produce false positives in some cases!"
    assert p.returncode == 0 and p.stdout == output, (name, p)
    assert not p.stderr or re.fullmatch(r'(?:==\d+==)?' + re.escape(note) + r'\n', p.stderr), (name, p)


example = (t.root / 'examples/cancellation_deadlines.on1x').read_text()
execute('example', example, '90\n7\n8\n')
execute('example_o2', example, '90\n7\n8\n', '-O2')

execute('timer_only', '''fn main(){taskgroup{
 for i in 0..32{assert(poll{after 1ms=>i}==i);}
 assert(poll{after 0ms=>1,default=>2}==1);
 assert(poll{after 1s=>1,default=>2}==2);
}}''')

execute('timer_operand_order', '''fn duration()->i64{println(2);return 10;}
fn payload()->i64{println(1);return 8;}
fn main(){taskgroup{
 let c=->(1,1){payload:i64,capacity:0,mode:uni};let(tx,rx)=split c;
 assert(poll{send tx <- payload()=>0,closed tx=>-1,after duration() ms=>1}==1);
 close tx;
 poll{recv rx as n=>assert(false),closed rx=>assert(true),after 1s=>assert(false)};
}}''', '1\n2\n')

# Interrupted waiters must be removed before group destruction or reuse.
# Child sends a rendezvous notification before blocking on the tested wait.
for operation in ('recv', 'send', 'poll'):
    action = {'recv': 'let value=<-rx;',
              'send': 'let value=tx <- 9;',
              'poll': 'poll{recv rx as n=>assert(false),closed rx=>assert(false),after 1s=>assert(false)};'}[operation]
    endpoint = 'tx' if operation == 'send' else 'rx'
    after = ('poll{recv rx as n=>assert(false),closed rx=>assert(false),default=>assert(true)};'
             if operation == 'send' else
             'assert(poll{send tx <- 7=>1,closed tx=>-1,default=>0}==0);close tx;')
    execute(f'blocked_{operation}', f'''fn main(){{for repeat in 0..16{{taskgroup{{
     let c=->(1,1){{payload:i64,capacity:0,mode:uni}};let(tx,rx)=split c;
     let ready=->(1,1){{payload:i64,capacity:0,mode:uni}};let(notify,observed)=split ready;
     let child=launch{{defer println(3);let started=notify <- 1;{action} 99}};
     match <-observed with{{Some(_)=>assert(true),None(_)=>assert(false)}};
     assert(cancel child);{after}
    }}}}}}''', '3\n' * 16)

# Cancellation crosses ordinary function calls and stops later operands.
execute('nested_call_cleanup', '''fn busy()->i64{
 defer println(2);for i in 0..100000{assert(i>=0);}return 42;
}fn later()->i64{println(999);return 1;}
fn main(){taskgroup{
 let ready=->(1,1){payload:i64,capacity:0,mode:uni};let(tx,rx)=split ready;
 let child=launch{defer println(1);let started=tx <- 0;busy()+later()};
 match <-rx with{Some(_)=>assert(true),None(_)=>assert(false)};
 assert(cancel child);
}}''', '2\n1\n')

execute('managed_call_cleanup', '''fn hold()->fn()->i64{
 let k=5;let f=fn()->i64{println(k);return k;};
 defer f();for i in 0..100000{assert(i>=0);}return f;
}fn main(){taskgroup{
 let ready=->(1,1){payload:i64,capacity:0,mode:uni};let(tx,rx)=split ready;
 let child=launch{defer println(1);let started=tx <- 0;hold()()};
 match <-rx with{Some(_)=>assert(true),None(_)=>assert(false)};
 assert(cancel child);
}}''', '5\n1\n')

execute('nested_child_cleanup', '''fn main(){taskgroup{
 let ready=->(1,1){payload:i64,capacity:0,mode:uni};let(tx,rx)=split ready;
 let child=launch{
  defer println(1);
  let nested=launch{defer println(2);for i in 0..100000{assert(i>=0);} 4};
  let started=tx <- 1;
  join nested
 };
 match <-rx with{Some(_)=>assert(true),None(_)=>assert(false)};
 assert(cancel child);
}}''', '1\n2\n')

execute('completed_cancel', '''fn main(){taskgroup{
 let c=->(1,1){payload:i64,capacity:0,mode:uni};let(tx,rx)=split c;
 let child=launch{let done=tx <- 1;42};
 match <-rx with{Some(_)=>assert(true),None(_)=>assert(false)};
 assert(!cancel child);
}}''')

# A cancelled parent must terminate/reap external children rather than leave
# process or pipe supervision alive after its source cleanup edge returns.
start = time.monotonic()
execute('external_child_cleanup', '''fn main(){taskgroup{
 let ready=->(1,1){payload:i64,capacity:0,mode:uni};let(tx,rx)=split ready;
 let child=launch{
  let p=launch exec("/bin/sleep","10");let started=tx <- 1;
  join p
 };
 match <-rx with{Some(_)=>assert(true),None(_)=>assert(false)};
 assert(cancel child);
}}''')
assert time.monotonic() - start < 8

execute('byte_wait_cleanup', '''fn main(){taskgroup{
 let ready=->(1,1){payload:i64,capacity:0,mode:uni};let(tx,rx)=split ready;
 let child=launch{
  let(p,input,output,error)=launch capture `/bin/cat`;
  defer println(4);let started=tx <- 1;
  let byte=<-output;
  join p
 };
 match <-rx with{Some(_)=>assert(true),None(_)=>assert(false)};
 assert(cancel child);
}}''', '4\n')

# Deadline must not reset on scheduler progress; real communications before the
# deadline still win, and timed-out sends leave no ghost payload behind.
execute('channel_deadlines', '''fn main(){taskgroup{
 let c=->(1,1){payload:i64,capacity:0,mode:uni};let(tx,rx)=split c;
 let child=launch{let value=poll{recv rx as n=>n,closed rx=>-1,after 1s=>-2};value};
 assert(poll{send tx <- 255=>1,closed tx=>-1,after 1s=>-2}==1);
 assert(join child==255);
}}''', '', '-O2')

for name, text, diagnostic in [
    ('duplicate_timer', 'fn main(){taskgroup{poll{after 1ms=>0,after 2ms=>1};}}', 'duplicate poll deadline'),
    ('duration_type', 'fn main(){taskgroup{poll{after "no"=>0};}}', 'expected i64'),
    ('cancel_alias', 'fn main(){taskgroup{let f=launch{1};let copy=f;cancel f;}}', 'affine'),
    ('cancel_twice', 'fn main(){taskgroup{let f=launch{1};let pause=1;cancel f;cancel f;}}', 'moved'),
    ('deadline_mir', example, 'concurrency requires effectful/ownership MIR'),
    ('async_unimplemented', 'async fn main()->i64{return 1;}', 'expected'),
    ('cleanup_expansion_budget', 'fn main(){' +
     ''.join(f'let f{i}=fn()->i64{{return {i};}};' for i in range(600)) +
     'println(0);' * 600 + 'taskgroup{poll{after 1ms=>println(1)};}}',
     'cancellation cleanup expansion budget exhausted'),
]:
    source = t.source(name, text)
    out = t.work / (name + '.c')
    out.write_text('preserve')
    p = run([t.compiler, '--emit-ir-c' if name.endswith('_mir') else '--emit-c', source, '-o', out])
    assert p.returncode != 0 and diagnostic in p.stderr and out.read_text() == 'preserve', (name, p)

# Runtime ABI mismatch is diagnosed before linking, not a missing-function crash.
generated = t.work / 'abi.c'
p = run([t.compiler, '--emit-c', t.root / 'examples/cancellation_deadlines.on1x', '-o', generated])
assert p.returncode == 0, p
bad_abi = t.work / 'bad-abi'
bad_abi.mkdir(exist_ok=True)
(bad_abi / 'on1x_concurrency.h').write_text(
    (t.root / 'runtime/on1x_concurrency.h').read_text().replace('ON1X_CSP_ABI_VERSION 3', 'ON1X_CSP_ABI_VERSION 2'))
p = run([t.cc, '-std=c11', '-I', bad_abi, '-I', t.root / 'runtime', '-fsyntax-only', generated])
assert p.returncode != 0 and 'runtime ABI' in p.stderr, p

for name, duration, diagnostic in [('negative', '-1', 'negative poll duration')]:
    exe = build(name, 'fn main(){taskgroup{poll{after ' + duration + '=>assert(false)};}}')
    p = run([exe], env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
    assert p.returncode == 73 and diagnostic in p.stderr, p

# Wall-clock discontinuities must not change monotonic expiry. The actual
# channel/fiber kernels still run; only their UTC timespec_get observations jump.
clock = t.root / 'tests/fixtures/poll_wallclock.c'
exe = build('wallclock', 'fn main(){taskgroup{poll{after 25ms=>println(3)};}}',
            '-Wl,--wrap=timespec_get', extra=(clock,))
start = time.monotonic()
p = run([exe])
elapsed = time.monotonic() - start
assert p.returncode == 0 and p.stdout == '3\n' and .020 <= elapsed < 5, (elapsed, p)

# Enumerate every calloc failure of this bounded timer program. Ordinary success
# retains leak checking; intentional terminal failures retain ASan/UBSan but do
# not promise unwinding. In particular, new signal/timer allocations reject.
allocator = t.root / 'tests/fixtures/poll_allocator.c'
exe = build('allocation', 'fn main(){taskgroup{poll{after 1ms=>println(3)};}}',
            '-Wl,--wrap=calloc', extra=(allocator,))
failures = []
for point in range(1, 64):
    p = run([exe], env={**os.environ, 'ON1X_POLL_FAIL_AT': str(point),
                       'ASAN_OPTIONS': 'detect_leaks=0'})
    assert 'ERROR: AddressSanitizer' not in p.stderr and 'runtime error:' not in p.stderr, p
    if p.returncode == 0:
        assert p.stdout == '3\n', p
        break
    assert p.returncode == 73 and 'ON1X-CSP-RUNTIME:' in p.stderr, (point, p)
    failures.append(p.stderr)
else:
    raise AssertionError('timer allocation enumeration did not reach success')
assert any('timer allocation failure' in text for text in failures), failures
assert any('cancellation signal allocation failure' in text for text in failures), failures
assert any('poll allocation/protocol failure' in text for text in failures), failures
p = run([exe])
assert p.returncode == 0 and p.stdout == '3\n', p

print('running/blocked cancellation, explicit cleanup and monotonic deadlines PASS')
