#!/usr/bin/env python3
"""Exercise HIR→C11 taskgroup deadlines through the existing MuLIB scheduler."""
import os
import re
from support import NativeTests, run

t = NativeTests()
clock = t.root / 'tests/fixtures/taskgroup_within_clock.c'
clock_flags = ['-Wl,--wrap=clock_gettime', '-Wl,--wrap=nanosleep']


def build(name, text=None, *, fake=True, flags=(), extra=()):
    src = (t.source(name, text) if text is not None else
           t.root / 'tests/fixtures/taskgroup_within.on1x')
    exe = t.work / name
    companions = ((clock,) if fake else ()) + extra
    p = run(['python3', t.root / 'tools/on1x-build.py', src, '--on1xc', t.compiler,
             '--cc', t.cc, '-o', exe,
             *[arg for path in companions for arg in ('--extra-c', path)],
             *[f'--cflag={f}' for f in [*t.flags, *flags, *(clock_flags if fake else [])]]])
    assert p.returncode == 0, (name, p)
    return exe


def check(exe, output='', *, fault=None, line=None, mode='idle', no_leak_check=False):
    env = {**os.environ, 'ON1X_TEST_WITHIN_CLOCK': mode}
    if no_leak_check:
        env['ASAN_OPTIONS'] = 'detect_leaks=0'
    p = run([exe], env=env)
    assert p.returncode == (73 if fault else 0), (exe, p)
    if isinstance(output, list):
        assert sorted(p.stdout.splitlines()) == output, (exe, p)
    else:
        assert p.stdout == output, (exe, p)
    stderr = re.sub(r'(?m)^(?:==\d+==)?WARNING: ASan doesn\'t fully support '
                    r'makecontext/swapcontext functions and may produce false positives in some cases!\n',
                    '', p.stderr)
    if fault:
        at = rf':{line}:\d+' if line else r':\d+:\d+'
        assert re.fullmatch(r'.*' + at + ': ON1X-CSP-RUNTIME: ' + re.escape(fault) + '\n',
                            stderr), (exe, p)
    else:
        assert not stderr, (exe, p)


expired = 'monotonic taskgroup deadline expired'
check(build('success'), '10\n42\n7\n8\n')
check(build('success_O2', flags=('-O2',)), '10\n42\n7\n8\n')

check(build('zero', '''fn main() {
    defer println(1);
    taskgroup within 0ms { assert(false); }
    assert(false);
}'''), '1\n', fault=expired, line=3)

# The outer deadline governs both a waiting parent and its nested child. The
# shorter deadline's source takes precedence, and expired wait registrations
# must be withdrawn before their channels and the child's scope can be freed.
for outer, inner, line in [(20, 2000, 6), (2000, 20, 8), (20, 20, 6)]:
    check(build(f'nested_{outer}_{inner}', f'''fn later()->i64{{assert(false);return 0;}}
fn main(){{
    defer println(10);
    taskgroup {{
    let c=->(1,1){{payload:i64,capacity:0,mode:uni}};let (tx,rx)=split c;
    taskgroup within {outer}ms {{
        let child=launch{{
            taskgroup within {inner}ms {{
                defer println(20);
                poll{{recv rx as n=>assert(false),closed rx=>assert(false),after 5s=>assert(false)}};
            }}
            9
        }};
        println((join child)+later());
    }}
    close tx;
    }}
}}'''), ['10', '20'], fault=expired, line=line)

check(build('expired_receive', '''fn main(){
    taskgroup within 5ms {
        let c=->(1,1){payload:i64,capacity:0,mode:uni};let (tx,rx)=split c;
        let received=<-rx;
        assert(false);
    }
}'''), fault=expired, line=2)

check(build('loop_point', '''fn main(){
    taskgroup within 60 {
        defer println(3);
        loop { }
    }
}'''), '3\n', fault=expired, line=2, mode='busy')

check(build('cancel_retires_deadline', '''fn cleanup(){poll{after 10ms=>println(6)};}
fn main(){taskgroup{
    let c=->(1,1){payload:i64,capacity:0,mode:uni};let(tx,rx)=split c;
    let child=launch{taskgroup within 2ms {
        defer cleanup();let ready=tx <- 1;
        poll{after 100ms=>assert(false)};
    } 9};
    match <-rx with {Some(_)=>assert(true),None(_)=>assert(false)};
    assert(cancel child);println(7);
}}'''), '6\n7\n')

check(build('shielded_cleanup', '''fn cleanup(){taskgroup{
    let child=launch{poll{after 3ms=>println(4)};7};
    println(join child);
}}
fn main(){
    defer cleanup();
    taskgroup within 0ms {}
}'''), '4\n7\n', fault=expired, line=7)

check(build('shielded_deadline_rejected', '''fn cleanup(){
    taskgroup within 1s {assert(false);}
}
fn main(){defer cleanup();}'''),
      fault='taskgroup within is unavailable during shielded cleanup',
      line=2, no_leak_check=True)

# Real POSIX child: cat cannot finish while the still-owned stdin is open.
# The deadline interrupts the buffered-byte wait, stops/reaps the child, and
# cleans up all three streams before the runtime reports terminal expiry.
check(build('process_stream', '''fn main(){
    defer println(12);
    taskgroup within 150ms {
        let (p,input,output,error)=launch capture `/bin/cat`;
        let byte=<-output;
        println(join p);
    }
}''', fake=False), '12\n', fault=expired, line=3)

for name, duration, fault, mode in [
    ('negative', '-1', 'negative taskgroup duration', 'idle'),
    ('overflow', '2000', 'taskgroup deadline overflow', 'overflow'),
    ('clock_failed', '100', 'monotonic clock unavailable', 'failure'),
]:
    check(build(name, f'fn main(){{taskgroup within {duration}{{assert(false);}}}}'),
          fault=fault, line=1, mode=mode, no_leak_check=True)

exe = build('allocator', 'fn main(){taskgroup within 7ms {println(5);}}',
            flags=('-Wl,--wrap=calloc',), extra=(t.root / 'tests/fixtures/poll_allocator.c',))
failures = []
for point in range(1, 40):
    p = run([exe], env={**os.environ, 'ON1X_POLL_FAIL_AT': str(point),
                        'ASAN_OPTIONS': 'detect_leaks=0'})
    if p.returncode == 0:
        assert p.stdout == '5\n', p
        break
    assert p.returncode == 73 and 'ON1X-CSP-RUNTIME:' in p.stderr, (point, p)
    assert 'ERROR: AddressSanitizer' not in p.stderr and 'runtime error:' not in p.stderr, p
    failures.append(p.stderr)
else:
    raise AssertionError('taskgroup allocation enumeration did not reach success')
assert any('taskgroup allocation failure' in text for text in failures), failures

for name, text, diagnostic, flag in [
    ('wrong_type', 'fn main(){taskgroup within "seconds" {}}', 'expected i64', '--emit-c'),
    ('missing', 'fn main(){taskgroup within {}}', 'expected', '--emit-c'),
    ('bad_units', 'fn main(){taskgroup within 1min {}}', 'expected', '--emit-c'),
    ('live_handle', 'fn main(){taskgroup within 1s{let f=launch{1};}}',
     'fiber must be joined or cancelled', '--emit-c'),
    ('unsupported_mir', 'fn main(){taskgroup within 1s{}}',
     'requires effectful/ownership MIR', '--emit-ir-c'),
    ('raw_foreign', 'extern fn native(); fn main(){taskgroup within 1s{native();}}',
     'raw foreign effects are unavailable', '--emit-c'),
]:
    out = t.work / f'{name}.c'
    out.write_text('preserve')
    p = run([t.compiler, flag, t.source(name, text), '-o', out])
    assert p.returncode != 0 and diagnostic in p.stderr and out.read_text() == 'preserve', (name, p)

print('taskgroup within, cancellation propagation, disarm and fail-closed C11 PASS')
