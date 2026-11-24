#!/usr/bin/env python3
"""Assemble, link and execute native timer frames against the C11 reference."""
import argparse
import os
import re
import subprocess
from pathlib import Path


def run(*args, check=True, env=None):
    result = subprocess.run([str(arg) for arg in args], text=True, capture_output=True, timeout=30, env=env)
    if check and result.returncode:
        raise AssertionError(result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ('tool', 'on1xc', 'cc', 'work', 'source', 'timer-abi', 'runtime'):
        parser.add_argument('--' + arg, type=Path, required=True)
    parser.add_argument('--cflag', action='append', default=[])
    args = parser.parse_args()
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    source_root = args.source
    probe = source_root / 'tests/fixtures/async_source_probe.c'
    trace = Path(__file__).with_name('TimerTrace.c')
    observation = ('clock_gettime', 'nanosleep', 'mulib_fiber_spawn',
                   'on1x_async_frame_alloc_v1', 'on1x_async_frame_free_v1',
                   'on1x_async_launch_v1')
    wraps = ['-Wl,--wrap=' + name for name in observation]
    probe_args = ('-I', source_root / 'runtime', '-I', source_root.parent / 'include',
                  probe, trace, *wraps)
    libs = (args.timer_abi, args.runtime, '-pthread', '-lm')

    def fixture(name, text):
        path = work / (name + '.on1x')
        path.write_text(text, encoding='utf-8')
        return path

    def normalize_stderr(stderr):
        # The existing stackful owner/supervisor/peer still use ucontext; remove
        # only its known advisory, not any sanitizer diagnostic.
        return re.sub(r"(?m)^==\d+==WARNING: ASan doesn't fully support makecontext/swapcontext "
                      r"functions and may produce false positives in some cases!\n", "", stderr, count=1)

    def compare(path, expected, *, status=0, message=None, pic=False, fatal=False):
        asm = work / 'native.s'
        obj = work / 'native.o'
        native = work / 'native'
        refc = work / 'reference.c'
        reference = work / 'reference'
        options = ['--pic'] if pic else []
        run(args.tool, '--timer-source', path, '-o', asm, *options)
        assembly = asm.read_bytes()
        assert b'on1x_archon_timer_resume_0' in assembly and b'on1x_archon_timer_launch_v1' in assembly
        run(args.tool, '--timer-source', path, '-o', asm, *options)
        assert assembly == asm.read_bytes(), 'native timer output is nondeterministic'
        run(args.cc, '-c', asm, '-o', obj)
        assert obj.read_bytes()[:6] == b'\x7fELF\x02\x01', 'expected a real ELF64 object'
        run(args.cc, '-pie' if pic else '-no-pie', *args.cflag, obj, *probe_args, *libs, '-o', native)
        run(args.on1xc, '--no-prelude', '--emit-c', path, '-o', refc)
        run(args.cc, '-std=c11', '-pie' if pic else '-no-pie',
            *args.cflag, refc, *probe_args, *libs, '-o', reference)
        env = os.environ.copy()
        if fatal:
            # A terminal protocol fault does not unwind. Keep address/UB
            # checks enabled and check leaks on ordinary and drained exits.
            env['ASAN_OPTIONS'] = ':'.join(filter(None, [env.get('ASAN_OPTIONS'), 'detect_leaks=0']))
        a = run(native, check=False, env=env)
        b = run(reference, check=False, env=env)
        assert a.returncode == b.returncode == status, (a, b)
        assert a.stdout == b.stdout and re.fullmatch(expected, a.stdout), (a, b)
        a_err, b_err = map(normalize_stderr, (a.stderr, b.stderr))
        assert a_err == b_err and (message in a_err if message else not a_err), (a, b)
        if message and status == 73:
            assert f'{path}:' in a_err, (a, b)

    example = source_root / 'examples/async_timer.on1x'
    for pic in (False, True):
        compare(example,
                r'(?:7\nframe 0 sleep 5\n|frame 0 sleep 5\n7\n)'
                'frame 0 sleep 3\nframe 0 return 42\nframe 0 drop\n42\n6\nprobe: 8 3 1 1\n',
                pic=pic)

    compare(fixture('cancel_ready', '''fn main(){taskgroup{
      let f = launch async { let never = 1/0; await timer(5); never };
      println(cancel f);
    }}'''), 'frame 0 drop\ntrue\nprobe: 0 2 1 1\n')
    compare(fixture('cancel_sleeping', '''fn main(){taskgroup{
      let f = launch async { await timer(90); 1/0 };
      let trigger = launch async { await timer(0); await timer(2); 7 };
      let result = join trigger;
      println(result);
      println(cancel f);
    }}'''), 'frame 1 sleep 0\nframe 0 sleep 90\nframe 1 sleep 2\nframe 1 return 7\n'
             'frame 1 drop\n7\nframe 0 drop\ntrue\nprobe: 2 2 2 2\n')
    compare(fixture('deadline', '''fn main(){taskgroup within 3ms {
      let f = launch async { await timer(90); 1/0 };
      println(join f);
    }}'''), 'frame 0 sleep 90\nframe 0 drop\nprobe: 3 2 1 1\n',
            status=73, message='monotonic taskgroup deadline expired')
    compare(fixture('duration', '''fn main(){taskgroup{
      let f = launch async { let d = -2; await timer(d % 3); 42 };
      println(join f);
    }}'''), 'frame 0 sleep -2\nprobe: 0 2 1 0\n', status=73,
            message='negative stackless frame delay', fatal=True)
    compare(fixture('arithmetic', '''fn main(){taskgroup{
      let f = launch async { let z = 0; await timer(2); 1/z };
      println(join f);
    }}'''), 'frame 0 sleep 2\nprobe: 2 2 1 0\n', status=70,
            message='On1x arithmetic error: invalid division', fatal=True)

    sentinel = work / 'untouched.s'
    for name, text, message in (
        ('async_fn', 'async fn f()->i64{42} fn main(){}', ''),
        ('task_await', 'fn main(){taskgroup{let f=launch async{await timer(1); 42}; await f;}}', ''),
        ('channel', '''fn main(){taskgroup{
          let ch = ->(1,1){payload:i64,capacity:0,mode:uni};
          let (tx,rx)=split ch; close tx; let observed = try_recv(rx);
          let f=launch async{await timer(1);42}; println(join f);
        }}''', 'ON1X-ARCHON-TIMER-UNSUPPORTED'),
        ('nested_group', '''fn main(){taskgroup{taskgroup{
          let f=launch async{await timer(1);42}; println(join f);
        }}}''', 'ON1X-ARCHON-TIMER-UNSUPPORTED'),
        ('unjoined', 'fn main(){taskgroup{let f=launch async{await timer(1);42};}}', 'joined or cancelled'),
    ):
        path = fixture(name, text)
        if name in ('channel', 'nested_group'):
            run(args.on1xc, '--check', '--no-prelude', path)
        sentinel.write_text('previous assembly\n', encoding='utf-8')
        result = run(args.tool, '--timer-source', path, '-o', sentinel, check=False)
        assert result.returncode and f'{path}:' in result.stderr and message in result.stderr, result
        assert sentinel.read_text() == 'previous assembly\n'
    result = run(args.on1xc, '--emit-ir', '--no-prelude', example, '-o', sentinel, check=False)
    assert result.returncode and 'ON1X-MIR-UNSUPPORTED' in result.stderr
    assert sentinel.read_text() == 'previous assembly\n'


if __name__ == '__main__':
    main()
