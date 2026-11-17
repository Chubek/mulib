#!/usr/bin/env python3
"""Real compiled On1x cooperative-fiber/channel profile and closed boundaries."""
import os
import time
from pathlib import Path
from support import NativeTests, run

t = NativeTests()
helper = t.root / 'tools/on1x-build.py'


def build(name, source, *flags):
    path = source if isinstance(source, Path) else t.source(name, source)
    exe = t.work / name
    p = run(['python3', helper, path, '--on1xc', t.compiler,
             '--cc', t.cc, '-o', exe, *[f'--cflag={flag}' for flag in t.flags], *flags])
    assert p.returncode == 0, (name, p.returncode, p.stderr)
    return exe


def execute(name, source, output, *flags):
    exe = build(name, source, *flags)
    p = run([exe])
    asan_note = "WARNING: ASan doesn't fully support makecontext/swapcontext functions and may produce false positives in some cases!"
    assert p.returncode == 0 and p.stdout == output and (not p.stderr or asan_note in p.stderr and
           p.stderr.count('\n') == 1), (name, p)


def reject(name, source, needle, *flags):
    path = t.source(name, source)
    preserved = t.work / (name + '.c')
    preserved.write_text('do not replace')
    p = run([t.compiler, '--emit-ir-c' if '--via-mir' in flags else '--emit-c', path, '-o', preserved])
    assert p.returncode != 0 and needle in p.stderr and preserved.read_text() == 'do not replace', (name, p)


example = t.root / 'examples/cooperative_channels.on1x'
execute('cooperative_example', example, '1\n10\n20\n2\n3\n30\n0\n7\n')
execute('cooperative_example_o2', example, '1\n10\n20\n2\n3\n30\n0\n7\n', '--cflag=-O2')
execute('poll_example', t.root / 'examples/cooperative_poll.on1x', '42\n1\n')
execute('external_example', t.root / 'examples/cooperative_external.on1x', '0\ntrue\n1\n')
execute('process_stream_example', t.root / 'examples/cooperative_process_streams.on1x', '79\n110\n49\n120\n0\n')
execute('external_literal_argv', '''fn main(){taskgroup{
 let p:xfiber=launch exec("/bin/echo","$(echo NOT_EXPANDED)");
 println(join p);
 }}''', '$(echo NOT_EXPANDED)\n0\n')
execute('shell_literal_argv', '''fn main(){taskgroup{
 let p:xfiber=launch `/bin/echo "two words" 'three four' escaped\\ word`;
 println(join p);
 }}''', 'two words three four escaped word\n0\n')
execute('shell_literal_no_expansion', '''fn main(){taskgroup{
 let p:xfiber=launch `/bin/echo '$HOME' \\$HOME '|' ""`;
 println(join p);
 }}''', '$HOME $HOME | \n0\n')
execute('external_concurrent', '''fn main(){taskgroup{
 let worker=launch{var count=0;for i in 0..24{count=count+1;} count};
 let process:xfiber=launch exec("/bin/true");
 println(join process);println(join worker);
 }}''', '0\n24\n')

# A completed process is reaped once; the registry is bounded even when every
# earlier process has already finished and been joined.
execute('external_repeated', '''fn main(){for i in 0..32{taskgroup{
 let p:xfiber=launch exec("/bin/true");assert(join p==0);
 }}}''', '')
execute('external_nested_owner', '''fn main(){taskgroup{
 let f=launch{let p:xfiber=launch exec("/bin/false");join p};
 println(join f);
 }}''', '1\n')
execute('ready_internal_cancel', '''fn main(){for i in 0..32{taskgroup{
 let f=launch{println(999);77};cancel f;println(i);
 }}}''', ''.join(f'{i}\n' for i in range(32)))

child = t.work / 'external_child'
p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Werror',
         t.root / 'tests/fixtures/external_child.c', '-o', child])
assert p.returncode == 0, p
stream_child = t.work / 'process_stream_child'
p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Werror',
         t.root / 'tests/fixtures/process_stream_child.c', '-o', stream_child])
assert p.returncode == 0, p

# The compiler parses a bounded command literal itself and emits direct argv;
# no host shell sees quotes, escapes, comments or metacharacters. Captured
# streams are affine byte-channel endpoints and preserve NUL bytes.
execute('captured_process_streams', f'''fn main(){{taskgroup{{
 let (process,input,output,error)=launch capture ```{stream_child}
 stream "two words" 'four' escaped\\ word '#literal'
 ```;
 for i in 0..512{{
  let outcome=input <- u8_from_bits(i);
  match outcome with{{Ok(_)=>assert(true),Err(_)=>assert(false)}};
 }}
 assert(close input);assert(!close input);
 var count=0;var total=0;var output_open=true;
 while output_open{{match <-output with{{
  Some(byte)=>{{count=count+1;total=total+u8_to_bits(byte);}},
  None(_)=>{{output_open=false;}}
 }};}}
 var errors=0;var error_total=0;var error_open=true;
 while error_open{{match <-error with{{
  Some(byte)=>{{errors=errors+1;error_total=error_total+u8_to_bits(byte);}},
  None(_)=>{{error_open=false;}}
 }};}}
 println(count);println(total);println(errors);println(error_total);println(join process);
}}}}''', '512\n65280\n3\n233\n0\n', '--cflag=-DON1X_CSP_STREAM_CAPACITY=7')

execute('captured_stream_workers', f'''fn main(){{taskgroup{{
 let (process,input,output,error)=launch capture `{stream_child} stream "two words" four escaped\\ word '#literal'`;
 let output_reader=launch{{var total=0;var open=true;while open{{match <-output with{{Some(byte)=>{{total=total+u8_to_bits(byte);}},None(_)=>{{open=false;}}}};}} total}};
 let error_reader=launch{{var total=0;var open=true;while open{{match <-error with{{Some(byte)=>{{total=total+u8_to_bits(byte);}},None(_)=>{{open=false;}}}};}} total}};
 let outcome=input <- 90u8;match outcome with{{Ok(_)=>assert(true),Err(_)=>assert(false)}};close input;
 println(join process);println(join output_reader);println(join error_reader);
 }}}}''', '0\n90\n233\n', '--cflag=-DON1X_CSP_STREAM_CAPACITY=0')

execute('captured_join_closes_stdin', '''fn main(){taskgroup{
 let (process,input,output,error)=launch capture `/bin/cat`;
 println(join process);
 let after=input <- 1u8;match after with{Ok(_)=>println(-1),Err(code)=>println(code)};
 match <-output with{Some(_)=>println(-2),None(_)=>println(2)};
 match <-error with{Some(_)=>println(-3),None(_)=>println(3)};
 }}''', '0\n1\n2\n3\n')

# A closed child stdin must become a checked channel close rather than deliver
# SIGPIPE to the generated parent process.
execute('captured_stdin_epipe', f'''fn main(){{taskgroup{{
 let (process,input,output,error)=launch capture `{stream_child} noinput`;
 var rejected=false;
 for i in 0..300{{let outcome=input <- 65u8;match outcome with{{Ok(_)=>assert(true),Err(_)=>{{rejected=true;}}}};}}
 assert(rejected);close input;
 match <-output with{{Some(byte)=>println(u8_to_bits(byte)),None(_)=>println(-1)}};
 match <-output with{{Some(_)=>println(-2),None(_)=>println(0)}};
 println(join process);
 }}}}''', '88\n0\n0\n', '--cflag=-DON1X_CSP_STREAM_CAPACITY=1')
ready = t.work / 'external_child.ready'
ready.unlink(missing_ok=True)
execute('external_sigkill_escalation', f'''fn main(){{taskgroup{{
 let target:xfiber=launch exec("{child}","ignore","{ready}");
 let witness:xfiber=launch exec("{child}","wait","{ready}");
 assert(join witness==0);
 assert(cancel target);
 }}}}''', '')

captured_ready = t.work / 'captured_child.ready'
captured_ready.unlink(missing_ok=True)
execute('captured_cancel_cleanup', f'''fn main(){{taskgroup{{
 let (target,input,output,error)=launch capture `{child} ignore {captured_ready}`;
 let witness:xfiber=launch exec("{child}","wait","{captured_ready}");
 assert(join witness==0);assert(cancel target);
 let after=input <- 65u8;match after with{{Ok(_)=>assert(false),Err(code)=>assert(code==1)}};
 match <-output with{{Some(_)=>assert(false),None(_)=>assert(true)}};
 match <-error with{{Some(_)=>assert(false),None(_)=>assert(true)}};
 }}}}''', '')

execute('buffered_close', '''fn main(){taskgroup{
 let ch=->(1,1){payload:i64,capacity:2,mode:uni};let (tx,rx)=split ch;
 let sent=tx <- 4;let sent2=tx <- 9;close tx;close tx;
 match sent with{Ok(_)=>println(1),Err(_)=>println(-1)};
 match sent2 with{Ok(_)=>println(2),Err(_)=>println(-2)};
 match <-rx with{Some(n)=>println(n),None(_)=>println(-3)};
 match <-rx with{Some(n)=>println(n),None(_)=>println(-4)};
 match <-rx with{Some(_)=>println(-5),None(_)=>println(0)};
 let failed=tx <- 5;match failed with{Ok(_)=>println(-6),Err(code)=>println(code)};
}}''', '1\n2\n4\n9\n0\n1\n')

execute('nested_scopes', '''fn main(){taskgroup{
 let f=launch{taskgroup{let nested=launch{3};println(join nested);} 6};
 println(join f);
}}''', '3\n6\n')

execute('return_cleanup', '''fn work()->i64{taskgroup{
 let ch=->(1,1){payload:i64,capacity:1,mode:uni};let (tx,rx)=split ch;
 let child=launch{defer println(8);let sent=tx <- 5; 9};
 match <-rx with{Some(n)=>println(n),None(_)=>println(-1)};
 return join child;
}}fn main(){println(work());}''', '8\n5\n9\n')

# Recreate and destroy rendezvous storage repeatedly, including moved scalar
# captures and source-level checked outcomes. A lost wakeup stalls this fixture.
execute('repeated_rendezvous', '''fn main(){for n in 0..64{taskgroup{
 let c=->(1,1){payload:i64,capacity:0,mode:uni};let (tx,rx)=split c;
 let child=launch{let outcome=tx <- n;match outcome with{Ok(_)=>n,Err(_)=>-1}};
 match <-rx with{Some(value)=>assert(value==n),None(_)=>assert(false)};
 assert(join child==n);
}}}''', '')

# A single atomic MuLIB selection commits exactly one offer. Check both
# immediate/default and suspended/rendezvous paths, plus closure dispatch.
execute('poll_choice', '''fn main(){taskgroup{
 let a=->(1,1){payload:i64,capacity:1,mode:uni};let (tx,rx)=split a;
 let b=->(1,1){payload:i64,capacity:0,mode:uni};let (other,out)=split b;
 let first=poll{recv rx as n=>n,closed rx=>-1,default=>99};println(first);
 let sent=poll{send tx <- 41=>1,closed tx=>-2,default=>-3};println(sent);
 let available=poll{recv rx as n=>n,closed rx=>-4,recv out as n=>n+100,closed out=>-5};println(available);
 let worker=launch{let result=other <- 7;match result with{Ok(_)=>3,Err(_)=>-3}};
 let rendezvous=poll{recv out as n=>n,closed out=>-6};println(rendezvous);
 println(join worker);close tx;
 poll{recv rx as n=>println(-7),closed rx=>println(0)};
 let rejected=poll{send tx <- 8=>1,closed tx=>2};println(rejected);
 }}''', '99\n1\n41\n7\n3\n0\n2\n')

execute('poll_order', '''fn payload()->i64{println(10);return 3;}fn main(){taskgroup{
 let a=->(1,1){payload:i64,capacity:1,mode:uni};let (tx,rx)=split a;
 let value=poll{send tx <- payload()=>1,closed tx=>-1,default=>0};
 println(value);
 poll{recv rx as n=>println(n),closed rx=>println(-2)};
 }}''', '10\n1\n3\n')

execute('poll_losing_payload', '''fn payload()->i64{println(10);return 3;}fn main(){taskgroup{
 let a=->(1,1){payload:i64,capacity:0,mode:uni};let (tx,rx)=split a;
 let choice=poll{send tx <- payload()=>1,closed tx=>-1,default=>0};println(choice);
 close tx;poll{recv rx as n=>println(-2),closed rx=>println(2)};
 }}''', '10\n0\n2\n')

for name, text, diagnostic in [
    ('unjoined', 'fn main(){taskgroup{let f=launch{42};}}', 'fiber must be joined'),
    ('double_join', 'fn main(){taskgroup{let f=launch{42};println(join f);println(join f);}}', 'moved'),
    ('drop_branch', 'fn main(){taskgroup{let f=launch{42};if true{println(join f);}}}', 'fiber must be joined'),
    ('escape', 'fn main()->ifiber<i64>{taskgroup{let f=launch{42};return f;}}', 'cannot escape'),
    ('mutable_capture', 'fn main(){taskgroup{var x=1;let f=launch{x};println(join f);}}', 'mutable'),
    ('outside_group', 'fn main(){let f=launch{42};println(join f);}', 'taskgroup'),
    ('bad_cardinality', 'fn main(){taskgroup{let c=->(2,1){payload:i64,capacity:0,mode:uni};}}', 'cardinality'),
    ('bad_capacity', 'fn main(){taskgroup{let c=->(1,1){payload:i64,capacity:257,mode:uni};}}', 'capacity'),
    ('bad_payload', 'fn main(){taskgroup{let c=->(1,1){payload:string,capacity:0,mode:uni};}}', 'i64'),
    ('double_split', 'fn main(){taskgroup{let c=->(1,1){payload:i64,capacity:0,mode:uni};let (tx,rx)=split c;let (a,b)=split c;}}', 'moved'),
    ('endpoint_alias', 'fn main(){taskgroup{let c=->(1,1){payload:i64,capacity:0,mode:uni};let (tx,rx)=split c;let copy=tx;}}', 'affine'),
    ('lost_send', 'fn main(){taskgroup{let c=->(1,1){payload:i64,capacity:1,mode:uni};let (tx,rx)=split c;tx <- 1;}}', 'send outcome'),
    ('duplex', 'fn main(){taskgroup{let c=->(1,1){request:i64,response:i64,capacity:0,mode:duplex};}}', 'duplex'),
    ('poll', 'fn main(){taskgroup{poll{default=>println(1)};}}', 'poll'),
    ('poll_missing_close', 'fn main(){taskgroup{let c=->(1,1){payload:i64,capacity:0,mode:uni};let (tx,rx)=split c;poll{recv rx as n=>println(n)};}}', 'closed arm'),
    ('poll_bad_close', 'fn main(){taskgroup{let c=->(1,1){payload:i64,capacity:0,mode:uni};let (tx,rx)=split c;poll{recv rx as n=>println(n),closed tx=>println(1)};}}', 'no channel offer'),
    ('poll_duplicate', 'fn main(){taskgroup{let c=->(1,1){payload:i64,capacity:0,mode:uni};let (tx,rx)=split c;poll{recv rx as n=>println(n),recv rx as m=>println(m),closed rx=>println(1)};}}', 'duplicate poll endpoint'),
    ('poll_duplicate_default', 'fn main(){taskgroup{let c=->(1,1){payload:i64,capacity:0,mode:uni};let (tx,rx)=split c;poll{recv rx as n=>println(n),closed rx=>println(1),default=>println(2),default=>println(3)};}}', 'duplicate poll default arm'),
    ('poll_payload_moves_endpoint', '''fn main(){taskgroup{
     let c=->(1,1){payload:i64,capacity:1,mode:uni};let (tx,rx)=split c;
     let result=poll{send tx <- match true with{true=>{let f=launch{let sent=tx <- 8;1};let x=join f;7},rest=>7}=>1,closed tx=>-1};
     println(result);
    }}''', 'poll payload moved an offered endpoint'),
    ('send_payload_moves_endpoint', '''fn main(){taskgroup{
     let c=->(1,1){payload:i64,capacity:1,mode:uni};let (tx,rx)=split c;
     let result=tx <- match true with{true=>{let f=launch{let sent=tx <- 8;1};let x=join f;7},rest=>7};
    }}''', 'send payload moved its endpoint'),
    ('external_relative', 'fn main(){taskgroup{let p:xfiber=launch exec("bin/true");println(join p);}}', 'path must be absolute'),
    ('external_reserved_nominal', 'record xfiber{value:i64}fn main(){}', 'built-in type name cannot be redeclared'),
    ('external_nondeterministic_argv', 'fn main(){taskgroup{let name="/bin/true";let p:xfiber=launch exec(name);println(join p);}}', 'constant string literals'),
    ('external_empty_argv', 'fn main(){taskgroup{let p:xfiber=launch exec();println(join p);}}', 'absolute executable path'),
    ('external_unjoined', 'fn main(){taskgroup{let p:xfiber=launch exec("/bin/true");}}', 'fiber must be joined or cancelled'),
    ('external_double_join', 'fn main(){taskgroup{let p:xfiber=launch exec("/bin/true");println(join p);println(join p);}}', 'moved'),
    ('external_double_cancel', 'fn main(){taskgroup{let p:xfiber=launch exec("/bin/true");cancel p;cancel p;}}', 'moved'),
    ('external_join_after_cancel', 'fn main(){taskgroup{let p:xfiber=launch exec("/bin/sleep","10");cancel p;println(join p);}}', 'moved'),
    ('external_escape', 'fn run()->xfiber{taskgroup{let p:xfiber=launch exec("/bin/true");return p;}}fn main(){}', 'cannot escape'),
    ('external_capture', 'fn main(){taskgroup{let p:xfiber=launch exec("/bin/true");let f=launch{println(join p);1};println(join f);}}', 'concurrency capabilities'),
    ('shell_relative', 'fn main(){taskgroup{let p:xfiber=launch `bin/true`;println(join p);}}', 'path must be absolute'),
    ('shell_pipeline', 'fn main(){taskgroup{let p:xfiber=launch `/bin/echo one | /bin/cat`;println(join p);}}', 'pipelines'),
    ('shell_expansion', 'fn main(){taskgroup{let p:xfiber=launch `/bin/echo $HOME`;println(join p);}}', 'environment expansion'),
    ('shell_assignment', 'fn main(){taskgroup{let p:xfiber=launch `K=12 /bin/true`;println(join p);}}', 'assignment/export'),
    ('shell_export', 'fn main(){taskgroup{let p:xfiber=launch `export K=12`;println(join p);}}', 'assignment/export'),
    ('shell_unclosed_quote', 'fn main(){taskgroup{let p:xfiber=launch `/bin/echo "bad`;println(join p);}}', 'unterminated quote'),
    ('shell_escaped_newline', 'fn main(){taskgroup{let p:xfiber=launch ```/bin/echo bad\\' + '\n' + 'word```;println(join p);}}', 'cannot quote a newline'),
    ('shell_empty', 'fn main(){taskgroup{let p:xfiber=launch `   `;println(join p);}}', 'requires a command'),
    ('shell_arg_limit', 'fn main(){taskgroup{let p:xfiber=launch `/bin/true a b c d e f g h i`;println(join p);}}', 'argv limit'),
    ('shell_word_limit', 'fn main(){taskgroup{let p:xfiber=launch `/bin/echo ' + 'x' * 4097 + '`;println(join p);}}', '4096 bytes'),
    ('shell_command_limit', 'fn main(){taskgroup{let p:xfiber=launch `' + '/bin/echo ' + ' '.join(['x' * 2048] * 8) + '`;println(join p);}}', '16384 bytes'),
    ('external_command_limit', 'fn main(){taskgroup{let p:xfiber=launch exec("/bin/true",' + ','.join(['"' + 'x' * 2048 + '"'] * 8) + ');println(join p);}}', '16384 decoded bytes'),
    ('capture_outside_group', 'fn main(){let (p,input,output,error)=launch capture `/bin/cat`;}', 'taskgroup'),
    ('capture_bad_pattern', 'fn main(){taskgroup{let (p,input,output)=launch capture `/bin/cat`;}}', 'tuple arity'),
    ('capture_unjoined', 'fn main(){taskgroup{let (p,input,output,error)=launch capture `/bin/cat`;close input;}}', 'fiber must be joined or cancelled'),
    ('capture_stdin_receive', 'fn main(){taskgroup{let (p,input,output,error)=launch capture `/bin/cat`;match <-input with{Some(_)=>println(1),None(_)=>println(0)};cancel p;}}', 'receive expects'),
    ('capture_stdout_send', 'fn main(){taskgroup{let (p,input,output,error)=launch capture `/bin/cat`;let sent=output <- 1u8;cancel p;}}', 'expected sender'),
    ('capture_stdout_close', 'fn main(){taskgroup{let (p,input,output,error)=launch capture `/bin/cat`;close output;cancel p;}}', 'close expects'),
    ('capture_alias', 'fn main(){taskgroup{let (p,input,output,error)=launch capture `/bin/cat`;let copy=output;cancel p;}}', 'affine'),
    ('capture_poll', 'fn main(){taskgroup{let (p,input,output,error)=launch capture `/bin/cat`;poll{recv output as n=>println(n),closed output=>println(0)};cancel p;}}', 'receiver<i64>'),
    ('poll_after', 'fn main(){taskgroup{let c=->(1,1){payload:i64,capacity:0,mode:uni};let (tx,rx)=split c;poll{recv rx as n=>println(n),closed rx=>println(1),after 1s=>println(2)};}}', 'monotonic poll deadlines'),
    ('cancel_running', 'fn main(){taskgroup{let f=launch{1};let pause=1;cancel f;}}', 'immediately preceding launch'),
    ('cancel_in_expression', 'fn main(){taskgroup{let f=launch{1};let stopped=cancel f;}}', 'immediately preceding launch'),
    ('cancel_after_join', 'fn main(){taskgroup{let f=launch{1};println(join f);cancel f;}}', 'immediately preceding launch'),
]:
    reject(name, text, diagnostic)

reject('mir_unsupported', example.read_text(), 'concurrency requires effectful/ownership MIR', '--via-mir')

deadlock = build('deadlock', '''fn main(){taskgroup{
 let ch=->(1,1){payload:i64,capacity:0,mode:uni};let (tx,rx)=split ch;
 let sent=tx <- 1;match sent with{Ok(_)=>println(1),Err(_)=>println(2)};
}}''')
p = run([deadlock], env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
assert p.returncode == 73 and 'quiescent fiber/channel deadlock' in p.stderr, p

poll_deadlock = build('poll_deadlock', '''fn main(){taskgroup{
 let c=->(1,1){payload:i64,capacity:0,mode:uni};let (tx,rx)=split c;
 poll{recv rx as n=>println(n),closed rx=>println(-1)};
 }}''')
p = run([poll_deadlock], env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
assert p.returncode == 73 and 'quiescent fiber/channel deadlock' in p.stderr, p

limited = build('task_budget', '''fn main(){for i in 0..4{taskgroup{
 let f=launch{i};println(join f);
}}}''', '--cflag=-DON1X_CSP_MAX_TASKS=4')
p = run([limited], env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
assert p.returncode == 73 and p.stdout == '0\n1\n2\n' and 'fiber budget exhausted' in p.stderr, p

missing_process = build('external_spawn_failure', '''fn main(){taskgroup{
 let p:xfiber=launch exec("/definitely/nonexistent/on1x-executable");println(join p);
 }}''')
p = run([missing_process], env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
assert p.returncode == 73 and 'external posix_spawn failure' in p.stderr, p

missing_capture = build('captured_spawn_failure', '''fn main(){taskgroup{
 let (p,input,output,error)=launch capture `/definitely/nonexistent/on1x-executable`;
 cancel p;
 }}''')
p = run([missing_capture], env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
assert p.returncode == 73 and 'captured posix_spawn failure' in p.stderr, p

limited_capture = build('captured_channel_budget', '''fn main(){taskgroup{
 let (p,input,output,error)=launch capture `/bin/true`;
 cancel p;
 }}''', '--cflag=-DON1X_CSP_MAX_CHANNELS=2')
p = run([limited_capture], env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
assert p.returncode == 73 and 'captured stream channel budget exhausted' in p.stderr, p

limited_process = build('external_budget', '''fn main(){for i in 0..3{taskgroup{
 let p:xfiber=launch exec("/bin/true");println(join p);
 }}}''', '--cflag=-DON1X_CSP_MAX_PROCESSES=2')
p = run([limited_process], env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
assert p.returncode == 73 and p.stdout == '0\n0\n' and 'external process budget exhausted' in p.stderr, p

# A terminal runtime failure cannot unwind C frames, but it must not leave an
# already-running tracked subprocess executing indefinitely after the parent
# exits. A zombie awaiting init's reaping is terminated, not a running child.
failure_ready = t.work / 'external_failure.ready'
failure_ready.unlink(missing_ok=True)
terminal_process = build('external_terminal_cleanup', f'''fn main(){{taskgroup{{
 let target:xfiber=launch exec("{child}","ignore","{failure_ready}");
 let witness:xfiber=launch exec("{child}","wait","{failure_ready}");
 assert(join witness==0);
 let c=->(1,1){{payload:i64,capacity:0,mode:uni}};
 let d=->(1,1){{payload:i64,capacity:0,mode:uni}};
 cancel target;
 }}}}''', '--cflag=-DON1X_CSP_MAX_CHANNELS=1')
p = run([terminal_process], env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
assert p.returncode == 73 and 'channel budget exhausted' in p.stderr, p
pid = int(failure_ready.read_text().strip())
for attempt in range(100):
    status = Path(f'/proc/{pid}/stat')
    try:
        state = status.read_text().split(') ', 1)[1][0]
    except (FileNotFoundError, ProcessLookupError):
        break
    if state == 'Z':
        break
    time.sleep(.01)
else:
    raise AssertionError(f'terminal runtime failure left child {pid} running')

print('cooperative fibers/channels, checked failures and unsupported profiles PASS')
