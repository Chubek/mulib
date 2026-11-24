#!/usr/bin/env python3
"""Private C11 stackless-frame ABI; no On1x source async is admitted."""
import os
import re
from support import NativeTests, run


def no_ucontext_advisory(stderr):
    # Existing MuLIB stackful root and supervisor may trigger this single
    # ASan ucontext advisory; don't filter sanitizer diagnostics.
    return re.sub(r'(?m)^(?:==\d+==)?WARNING: ASan doesn\'t fully support '
                  r'makecontext/swapcontext functions and may produce false positives in some cases!\n',
                  '', stderr, count=1)


def check_async_frames(t):
    exe = t.work / 'async_frame'
    p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-pthread',
             '-I', t.root / 'runtime', '-I', t.root.parent / 'include',
             t.root / 'tests/fixtures/async_frame.c', t.root / 'runtime/on1x_runtime.c',
             t.root / 'runtime/on1x_concurrency.c',
             '-Wl,--wrap=mulib_fiber_spawn', '-Wl,--wrap=clock_gettime', '-Wl,--wrap=nanosleep',
             *t.flags, '-o', exe])
    assert p.returncode == 0, p

    def invoke(case, *, fault=None, output='', at=r'.*async_frame\.c:\d+:1', no_unwind=False):
        env = os.environ.copy()
        if no_unwind:
            # Protocol faults retain CSP's documented immediate process exit.
            # Address/UB checks stay on; all cooperative exits check leaks.
            env['ASAN_OPTIONS'] = ':'.join(filter(None, [
                env.get('ASAN_OPTIONS'), 'detect_leaks=0']))
        p = run([exe, case], env=env)
        assert p.returncode == (73 if fault else 0) and p.stdout == output, (case, p)
        stderr = no_ucontext_advisory(p.stderr)
        if fault:
            assert re.fullmatch(at + ': ON1X-CSP-RUNTIME: ' + re.escape(fault) + '\n',
                                stderr), (case, p)
        else:
            assert not stderr, (case, p)

    for case in ('mixed', 'repeated', 'real', 'nested-cancel', 'completed-cancel'):
        invoke(case)
    for case in ('deadline', 'deadline-return'):
        invoke(case, fault='monotonic taskgroup deadline expired',
               output='dropped\ndrained\n', at=r'deadline:123:4')
    invoke('shielded', fault='monotonic taskgroup deadline expired',
           output='dropped\nshield drained\n', at=r'deadline:123:4')
    for case, message in [
        ('malformed', 'invalid stackless frame action'),
        ('negative', 'negative stackless frame delay'),
        ('overflow', 'stackless frame deadline overflow'),
        ('blocking', 'operation outside a compiled On1x fiber'),
        ('unjoined', 'taskgroup exited with an unjoined child'),
        ('wrong-kind', 'join does not match task kind'),
        ('twice', 'invalid, repeated or non-owning join'),
        ('nonowner', 'invalid, repeated or non-owning join'),
    ]:
        invoke(case, fault=message, no_unwind=True)

    for name, text in [
        ('async_definition', 'async fn work()->i64{42} fn main(){}'),
        ('await_expression', 'fn main(){ let n = await 1; println(n); }'),
    ]:
        source = t.source(name, text)
        for mode in ('--emit-c', '--emit-ir-c'):
            out = t.work / (name + '.c')
            out.write_text('keep previous artifact')
            p = run([t.compiler, mode, source, '-o', out])
            assert p.returncode != 0 and f'{source}:' in p.stderr and \
                out.read_text() == 'keep previous artifact', (mode, p)
    check_async_source(t)


def check_async_source(t):
    probe = t.root / 'tests/fixtures/async_source_probe.c'
    wrappers = tuple('-Wl,--wrap=' + name for name in ('clock_gettime', 'nanosleep',
                     'mulib_fiber_spawn', 'on1x_async_frame_alloc_v1', 'on1x_async_frame_free_v1'))
    example = t.root / 'examples/async_timer.on1x'

    def execute(name, source, output, *, fault=None, extra=()):
        exe = t.work / name
        p = run(['python3', t.root / 'tools/on1x-build.py', source, '--on1xc', t.compiler,
                 '--cc', t.cc, '-o', exe, '--extra-c', probe,
                 *[f'--cflag={flag}' for flag in [*wrappers, *t.flags]], *extra])
        assert p.returncode == 0, (name, p)
        p = run([exe])
        assert p.returncode == (73 if fault else 0) and p.stdout == output, (name, p)
        stderr = no_ucontext_advisory(p.stderr)
        if fault:
            assert re.fullmatch(r'.*:\d+:\d+: ON1X-CSP-RUNTIME: ' + re.escape(fault) + '\n',
                                stderr), (name, p)
        else:
            assert not stderr, (name, p)

    execute('source_timers', example, '7\n42\n6\nprobe: 8 3 1 1\n')
    execute('source_timers_ppx', example, '7\n42\n6\nprobe: 8 3 1 1\n',
            extra=('--ppx', 'identity', t.root / 'stdppx/identity.py'))
    immediate = t.source('source_cancel_ready', '''fn main(){taskgroup{
      let f=launch async { let impossible=1/0; await timer(5); impossible };
      println(cancel f);
    }}''')
    execute('source_cancel_ready', immediate, 'true\nprobe: 0 2 1 1\n')
    waiting = t.source('source_cancel_wait', '''fn main(){taskgroup{
      let f=launch async { await timer(90); 1/0 };
      poll { after 2ms => println(7) };
      println(cancel f);
    }}''')
    execute('source_cancel_wait', waiting, '7\ntrue\nprobe: 2 2 1 1\n')
    deadline = t.source('source_async_deadline', '''fn main(){
      defer println(99); taskgroup within 3ms {
        let f=launch async { await timer(90); 1/0 };
        println(join f);
      }
    }''')
    execute('source_async_deadline', deadline, '99\nprobe: 3 2 1 1\n',
            fault='monotonic taskgroup deadline expired')

    def reject(name, text, diagnostic, mode='--emit-c'):
        source = t.source(name, text)
        output = t.work / (name + '.c')
        output.write_text('keep previous artifact')
        p = run([t.compiler, mode, source, '-o', output])
        assert p.returncode != 0 and diagnostic in p.stderr and \
            f'{source}:' in p.stderr and output.read_text() == 'keep previous artifact', (name, p)

    cases = [
        ('no_group', 'let f=launch async { await timer(1); 3 };', 'enclosing taskgroup'),
        ('outside_frame', 'await timer(1);', 'only permitted as a statement in launch async'),
        ('inside_fiber', 'taskgroup{let f=launch{await timer(1); 3}; cancel f;}',
         'only permitted as a statement in launch async'),
        ('no_suspend', 'taskgroup{let f=launch async { 3 }; cancel f;}',
         'requires an await timer'),
        ('wrong_duration', 'taskgroup{let f=launch async { await timer(true); 3 }; cancel f;}',
         'i64'),
        ('captured', 'let n=3; taskgroup{let f=launch async{await timer(1); n}; cancel f;}',
         'cannot capture outer bindings'),
        ('borrow_capture', 'let n=3; let r=borrow n; taskgroup{let f=launch async{await timer(1); r}; cancel f;}',
         'cannot capture borrowed place'),
        ('borrow_slot', 'taskgroup{let f=launch async{let n=3; let r=borrow n; await timer(1); r}; cancel f;}',
         'requires pure i64 arithmetic'),
        ('call', 'taskgroup{let f=launch async{println(3); await timer(1); 3}; cancel f;}',
         'allows only immutable i64 let, await timer, and return'),
        ('affine', 'taskgroup{let f=launch async{await timer(1); 3};}',
         'joined or cancelled on every normal exit'),
    ]
    for name, body, diagnostic in cases:
        reject('frame_' + name, 'fn main(){' + body + '}', diagnostic)
    reject('frame_no_mir', example.read_text(), 'requires effectful/ownership MIR', '--emit-ir-c')


if __name__ == '__main__':
    check_async_frames(NativeTests())
    print('private CSP frames and checked C11 source timer profile PASS')
