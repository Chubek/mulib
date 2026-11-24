#!/usr/bin/env python3
"""Private C11 stackless-frame ABI; no On1x source async is admitted."""
import os
import re
from support import NativeTests, run


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
        # Existing MuLIB stackful root and supervisor may trigger this single
        # ASan ucontext advisory; don't filter sanitizer diagnostics.
        stderr = re.sub(r'(?m)^(?:==\d+==)?WARNING: ASan doesn\'t fully support '
                        r'makecontext/swapcontext functions and may produce false positives in some cases!\n',
                        '', p.stderr, count=1)
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


if __name__ == '__main__':
    check_async_frames(NativeTests())
    print('private CSP stackless frames and source fail-closed boundary PASS')
