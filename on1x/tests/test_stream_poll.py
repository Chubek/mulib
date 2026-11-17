#!/usr/bin/env python3
"""Exact process-byte/i64 atomic selection through real compiled On1x."""
import os
import random
import re
import shutil
from support import NativeTests, run

t = NativeTests()


def build(name, source, *flags):
    path = t.source(name, source)
    exe = t.work / name
    p = run(['python3', t.root / 'tools/on1x-build.py', path,
             '--on1xc', t.compiler, '--cc', t.cc, '-o', exe,
             *[f'--cflag={flag}' for flag in [*t.flags, *flags]]])
    assert p.returncode == 0, (name, p)
    return exe


def check_run(name, executable, output):
    p = run([executable])
    note = "WARNING: ASan doesn't fully support makecontext/swapcontext functions and may produce false positives in some cases!"
    assert p.returncode == 0 and p.stdout == output, (name, p)
    assert not p.stderr or re.fullmatch(r'(?:==\d+==)?' + re.escape(note) + r'\n', p.stderr), (name, p)


def execute(name, source, output='', *flags):
    check_run(name, build(name, source, *flags), output)


example = (t.root / 'examples/cooperative_stream_poll.on1x').read_text()
for capacity in (0, 1, 7):
    execute(f'example_{capacity}', example, '1\n213\n0\n',
            f'-DON1X_CSP_STREAM_CAPACITY={capacity}', '-O2')

# Full byte domain twice, including NUL/high bit/255, in a seeded order with an
# independent Python oracle. Capacity zero forces registration/park/wake and
# proves that the byte buffers outlive the whole select rather than a probe.
values = list(range(256))
random.Random(2026111801).shuffle(values)
body = 'let values=array[' + ','.join(f'{v}u8' for v in values) + '''];
 for repeat in 0..2{for i in 0..256{
 poll{send input <- values[i]=>assert(true),closed input=>assert(false)};
 poll{recv output as byte=>println(u8_to_bits(byte)),closed output=>assert(false)};
 }}
'''
binary = '''fn main(){taskgroup{
 let (process,input,output,error)=launch capture `/bin/cat`;
''' + body + '''
 close input;
 poll{recv output as byte=>assert(false),closed output=>assert(true)};
 poll{recv error as byte=>assert(false),closed error=>assert(true)};
 assert(join process==0);
}}'''
for capacity in (0, 1, 7):
    execute(f'binary_{capacity}', binary, ''.join(f'{v}\n' for v in values * 2),
            f'-DON1X_CSP_STREAM_CAPACITY={capacity}')

# Two simultaneously ready sends commit only one. The losing byte never enters
# stdin; both payloads still evaluate once, in source order. After select, reuse
# both endpoints: no losing registration may refer to expired stack storage.
execute('mixed_losing_send', '''
fn byte_value()->u8{println(10);return 255u8;}
fn wide_value()->i64{println(20);return -9223372036854775807;}
fn main(){taskgroup{
 let (p,input,output,error)=launch capture `/bin/cat`;
 let c=->(1,1){payload:i64,capacity:1,mode:uni};let (tx,rx)=split c;
 let which=poll{send input <- byte_value()=>1,closed input=>-1,
                send tx <- wide_value()=>2,closed tx=>-2};
 if which==1{
  poll{recv rx as n=>assert(false),closed rx=>assert(false),default=>assert(true)};
  poll{recv output as byte=>assert(byte==255u8),closed output=>assert(false)};
 }else{
  assert(which==2);
  poll{recv rx as n=>assert(n == -9223372036854775807),closed rx=>assert(false)};
 }
 poll{send input <- 0u8=>assert(true),closed input=>assert(false)};
 close input;
 poll{recv output as byte=>assert(byte==0u8),closed output=>assert(false)};
 poll{recv output as byte=>assert(false),closed output=>assert(true)};
 poll{recv error as byte=>assert(false),closed error=>assert(true)};
 assert(join p==0);
}}''', '10\n20\n', '-O2', '-DON1X_CSP_STREAM_CAPACITY=1')

# Default cannot commit a rendezvous send before any supervisor has received it.
execute('byte_default_and_close', '''
fn payload()->u8{println(9);return 128u8;}
fn main(){taskgroup{
 let (p,input,output,error)=launch capture `/bin/cat`;
 let choice=poll{send input <- payload()=>1,closed input=>-1,default=>2};
 assert(choice==2);close input;
 poll{send input <- 255u8=>assert(false),closed input=>assert(true)};
 poll{recv output as n=>assert(false),closed output=>assert(true)};
 poll{recv error as n=>assert(false),closed error=>assert(true)};
 assert(join p==0);
}}''', '9\n', '-DON1X_CSP_STREAM_CAPACITY=0')

child = t.work / 'process_stream_child'
p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
         t.root / 'tests/fixtures/process_stream_child.c', '-o', child])
assert p.returncode == 0, p

# Concurrent pipe output, FIFO contents and separate EOF outcomes. Never retain
# an already-closed stream offer in the choice: closed is permanently ready.
execute('both_streams', f'''fn main(){{taskgroup{{
 let (p,input,output,error)=launch capture `{child} stream "two words" four escaped\\ word '#literal'`;
 for i in 0..64{{poll{{send input <- u8_from_bits(i)=>assert(true),closed input=>assert(false)}};}}
 close input;var stdout_open=true;var stderr_open=true;var count=0;var errors=0;var total=0;
 while stdout_open || stderr_open{{
  if stdout_open && stderr_open{{
   poll{{recv output as byte=>{{assert(u8_to_bits(byte)==count);count=count+1;}},
         closed output=>{{stdout_open=false;}},
         recv error as byte=>{{errors=errors+1;total=total+u8_to_bits(byte);}},
         closed error=>{{stderr_open=false;}}}};
  }}else{{if stdout_open{{
   poll{{recv output as byte=>{{assert(u8_to_bits(byte)==count);count=count+1;}},closed output=>{{stdout_open=false;}}}};
  }}else{{
   poll{{recv error as byte=>{{errors=errors+1;total=total+u8_to_bits(byte);}},closed error=>{{stderr_open=false;}}}};
  }}}}
 }}
 assert(count==64);assert(errors==3);assert(total==233);assert(join p==0);
}}}}''', '', '-DON1X_CSP_STREAM_CAPACITY=7', '-O2')

# Ready byte send selected by a fiber, then two receive offers register and park.
# This also exercises moved stream capabilities and function-return cleanup.
execute('fiber_select', '''fn main(){taskgroup{
 let (p,input,output,error)=launch capture `/bin/cat`;
 let reader=launch{
  let byte=poll{recv output as n=>n,closed output=>0u8};
  poll{recv output as n=>assert(false),closed output=>assert(true)};
  poll{recv error as n=>assert(false),closed error=>assert(true)};
  u8_to_bits(byte)
 };
 poll{send input <- 200u8=>assert(true),closed input=>assert(false)};close input;
 assert(join reader==200);assert(join p==0);
}}''', '', '-DON1X_CSP_STREAM_CAPACITY=0')

execute('cancel_then_poll', '''fn main(){taskgroup{
 let (p,input,output,error)=launch capture `/bin/cat`;
 cancel p;
 poll{send input <- 99u8=>assert(false),closed input=>assert(true)};
 poll{recv output as n=>assert(false),closed output=>assert(true)};
 poll{recv error as n=>assert(false),closed error=>assert(true)};
}}''')

# Source wire round-trip with ordinary Python identity PPX.
identity = t.root / 'stdppx/identity.py'
path = t.source('identity', example)
identity_exe = t.work / 'identity'
p = run(['python3', t.root / 'tools/on1x-build.py', path,
         '--on1xc', t.compiler, '--cc', t.cc, '-o', identity_exe,
         '--ppx', 'identity', identity, *[f'--cflag={flag}' for flag in t.flags]])
assert p.returncode == 0, p
check_run('identity', identity_exe, '1\n213\n0\n')


def reject(name, source, needle, mode='--emit-c'):
    path = t.source(name, source)
    out = t.work / (name + '.c')
    out.write_text('preserve existing output')
    p = run([t.compiler, mode, path, '-o', out])
    assert p.returncode != 0 and needle in p.stderr, (name, p)
    assert str(path) + ':' in p.stderr, (name, p)
    assert out.read_text() == 'preserve existing output'


prefix = 'fn main(){taskgroup{let (p,input,output,error)=launch capture `/bin/cat`;'
suffix = 'cancel p;}}'
for name, text, needle in [
    ('recv_stdin', 'poll{recv input as n=>0,closed input=>0};', 'poll receive expects'),
    ('send_stdout', 'poll{send output <- 0u8=>0,closed output=>0};', 'poll send expects'),
    ('send_stderr', 'poll{send error <- 0u8=>0,closed error=>0};', 'poll send expects'),
    ('wrong_send_payload', 'poll{send input <- 256=>0,closed input=>0};', 'expected u8'),
    ('wrong_recv_binder', 'poll{recv output as n=>n+1,closed output=>0};', 'expected i64, got u8'),
    ('different_results', 'let n=poll{recv output as n=>n,closed output=>0};', 'expected u8'),
    ('missing_close', 'poll{recv output as n=>n};', 'closed arm'),
    ('duplicate_byte', 'poll{recv output as n=>0,recv output as m=>0,closed output=>0};', 'duplicate poll endpoint'),
    ('moved_byte', 'let f=launch{match <-output with{Some(_)=>0,None(_)=>0}};println(join f);poll{recv output as n=>0,closed output=>0};', 'moved'),
]:
    reject(name, prefix + text + suffix, needle)
reject('scalar_mir', example, 'concurrency requires effectful/ownership MIR', '--emit-ir-c')

# Separate C11 runtime/kernel/consumer TUs, and an optional C++ consumer using
# the actual private header. Malformed boundary values must reject before any
# memory copy or pipe access. One test TU owns all MuLIB implementation switches.
driver = t.work / 'boundary_driver.c'
driver.write_text('''#include "on1x_concurrency.h"
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
extern int on1x_test_poll_mode;
int64_t on1x_test_poll(on1x_env *);
#ifdef __cplusplus
}
#endif
int main(int argc,char **argv){
 on1x_test_poll_mode=argc>1?atoi(argv[1]):0;
 return (int)on1x_csp_run(on1x_test_poll,NULL,__FILE__,1,1);
}
''')
objects = []
for index, source in enumerate([t.root / 'tests/fixtures/stream_poll_boundary.c',
                                t.root / 'runtime/on1x_runtime.c',
                                t.root / 'runtime/on1x_concurrency.c']):
    obj = t.work / f'boundary_{index}.o'
    p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-pthread', '-I', t.root / 'runtime', '-I', t.root.parent / 'include',
             *t.flags, '-c', source, '-o', obj])
    assert p.returncode == 0, p
    objects.append(obj)
consumers = [(t.cc, '-std=c11', driver)]
cxx = shutil.which('clang++' if 'clang' in t.cc else 'g++')
if cxx:
    cpp = t.work / 'boundary_driver.cpp'
    cpp.write_text(driver.read_text())
    consumers.append((cxx, '-std=c++20', cpp))
for index, (cc, standard, source) in enumerate(consumers):
    exe = t.work / f'boundary_{index}'
    p = run([cc, standard, '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-pthread',
             '-I', t.root / 'runtime', source, *objects, *t.flags, '-o', exe])
    assert p.returncode == 0, p
    p = run([exe, '0'])
    assert p.returncode == 0 and p.stdout == '', p
    for mode, needle in [(1, 'byte payload outside u8 range'),
                         (2, 'byte payload outside u8 range'),
                         (3, 'byte send requires captured stdin'),
                         (4, 'byte receive requires captured stdout/stderr'),
                         (5, 'poll allocation/protocol failure')]:
        p = run([exe, str(mode)], env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
        assert p.returncode == 73 and needle in p.stderr, (mode, p)

print('mixed i64/u8 atomic stream poll, exact payloads and boundary rejections PASS')
