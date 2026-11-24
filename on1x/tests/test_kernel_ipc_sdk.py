#!/usr/bin/env python3
"""Real KernelBind CDecl, compiled On1x, and instrumentable separate-TU tests."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

exo, on1xc, root, work = (Path(arg).resolve() for arg in sys.argv[1:5])
cc, cxx = sys.argv[5:7]
flags = [flag.removeprefix('--cflag=') for flag in sys.argv[7:]]
clang = shutil.which('clang')
if (not clang or sys.platform != 'linux' or not Path('/proc/self/fd').is_dir() or
        not re.fullmatch(r'x86_64(?:-[a-z0-9_]+)?-linux-gnu',
                         subprocess.check_output([cc, '-dumpmachine'], text=True).strip())):
    print('UNSUPPORTED: framed KernelBind test requires native Linux x86-64, procfs and Clang resource headers')
    sys.exit(77)


def run(*argv, error=None):
    proc = subprocess.run(argv, text=True, capture_output=True, timeout=90)
    if error is None:
        assert proc.returncode == 0, proc
    else:
        assert proc.returncode != 0 and error in proc.stderr, proc
    return proc.stdout


driver = [sys.executable, root/'tools/on1x-build.py', '--no-prelude', '--on1xc', on1xc,
          '--exotract', exo, '--cc', cc, '--resource-dir', run(clang, '-print-resource-dir').strip(),
          *(f'--cflag={flag}' for flag in flags)]
fixture = root/'examples/kernel/framed_pair.on1x'
session_fixture = fixture.with_name('framed_session.on1x')
work.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as tmp:
    tmp = Path(tmp)
    plan, program = tmp/'plan.json', tmp/'framed'
    run(*driver, '--kernel-bind', fixture, '-o', program, '--mulib-plan', plan)
    assert run(program) == '0\n42\n-1\n'
    data = json.loads(plan.read_text())
    interface = data['kernel_interface']
    assert data['safe_export'] is interface['safe_export'] is False
    assert interface['import_policy'] == 'unsafe_only'
    assert interface['profile'] == 'unsafe-ipc-framed-local-pair/v1'
    assert set(interface['exports']) == {'open_pair', 'roundtrip', 'open_session',
                                         'register_callback', 'revoke', 'send', 'poll', 'finish_send'}
    assert all(v['status'] == 'unsafe' for v in interface['exports'].values())
    assert len(interface['c_dependencies']) == 4
    assert data['plan']['implementation_translation_units'] == 1
    assert data['plan']['implementation_macros'] == ['MULIB_IMPL_SANDBOX_PROFILE', 'MULIB_IMPL_IPC_FRAMED']
    assert data['executable_sha256'] == hashlib.sha256(program.read_bytes()).hexdigest()
    assert all(str(root/'runtime'/f'on1x_kernel_ipc.{ext}') in data['inputs'] for ext in ('h', 'c'))

    session_program = tmp/'session'
    run(*driver, '--kernel-bind', session_fixture, '-o', session_program)
    assert run(session_program) == '42\n297\n'

    inspection = tmp/'inspection.json'
    run(*driver, '--inspect-kernels', fixture, '-o', inspection)
    report = json.loads(inspection.read_text())
    module = report['modules']['mulib.ipc_framed']
    assert module['callable'] is module['safe_export'] is False
    assert module['contract']['bundle_provenance']['review_status'] == 'unreviewed'
    spec = importlib.util.spec_from_file_location('kernelbind_test', root/'tools/on1x-exotract.py')
    bridge = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(bridge)
    assert bridge.ipc_raw_interface(report) == interface
    graph = module['metadata']['cdecl']
    send = next(d for d in graph['declarations'] if d['name'] == 'mulib_ipc_send')
    missing_provenance = copy.deepcopy(report)
    declarations = missing_provenance['modules']['mulib.ipc_framed']['metadata']['cdecl']['declarations']
    next(d for d in declarations if d['name'] == 'mulib_ipc_send')['source']['file'] = 'noncanonical.h'
    try:
        bridge.ipc_raw_interface(missing_provenance)
    except ValueError as error:
        assert 'ON1X-KERNEL-ABI' in str(error)
    else:
        raise AssertionError('admitted resource profile with missing declaration provenance')
    for id_, field, value in (
        (send['type'], 'calling_convention', 10),
        (send['type'], 'variadic', True),
        (next(t['id'] for t in graph['types'] if t['spelling'] == 'size_t'), 'alignment', 4),
        (next(t['id'] for t in graph['types'] if t['spelling'] == 'const void'), 'const', False),
        (next(t['id'] for t in graph['types'] if t['spelling'] == 'struct mulib_ipc_framed'),
         'size', 16),
    ):
        changed = copy.deepcopy(report)
        types = changed['modules']['mulib.ipc_framed']['metadata']['cdecl']['types']
        next(t for t in types if t['id'] == id_)[field] = value
        try:
            bridge.ipc_raw_interface(changed)
        except ValueError as error:
            assert 'ON1X-KERNEL-ABI' in str(error)
        else:
            raise AssertionError(f'admitted drifted CDecl: {field}')

    def denied(name, source, diagnostic, *, options=('--kernel-c11',)):
        path, output = tmp/(name+'.on1x'), tmp/(name+'.c')
        path.write_text(source, encoding='ascii')
        run(on1xc, '--emit-c', '--no-prelude', *options, path, '-o', output, error=diagnostic)
        assert not output.exists()
        return path

    prefix = 'unsafe import foreign stdffi.ipc_framed as I;\n'
    valid = 'fn f(x:i64)->i64 { return x; }\n'
    owner = 'let pair = match I.open_pair() with { Ok(p)=>move(p), Err(e)=>{println(e); return;} };\n'
    denied('context', prefix+'fn main(){ I.open_pair(); }', 'ON1X-KERNEL-CONTEXT')
    denied('first_class', prefix+'fn main(){ unsafe { let f=I.open_pair; } }', 'ON1X-KERNEL-DIRECT')
    denied('member', prefix+'fn main(){ unsafe { I.receive(); } }', 'ON1X-KERNEL-MEMBER')
    denied('owner_type', prefix+'fn f(x:own<I.Pair>){} fn main(){}', 'ON1X-KERNEL-MEMBER')
    denied('extern', prefix+'extern fn arbitrary() -> i64; fn main(){}', 'ON1X-KERNEL-EXTERN')
    denied('consumption', prefix+valid+'fn main(){ unsafe { '+owner+
           'I.roundtrip(move(pair),0u8,1000,f); drop(pair); } }', 'use after move')
    denied('copied', prefix+valid+'fn main(){ unsafe { '+owner+
           'I.roundtrip(pair,0u8,1000,f); } }', 'requires move(local)')
    denied('take', prefix+'fn main(){ unsafe { '+owner+'take(pair); } }', 'ON1X-KERNEL-OPAQUE')
    denied('borrow', prefix+'fn main(){ unsafe { '+owner+'let borrow_pair=borrow pair; } }', 'ON1X-KERNEL-OPAQUE')
    denied('bad_callback', prefix+'fn main(){ unsafe { '+owner+
           'I.roundtrip(move(pair),0u8,1000,fn(x:bool)->bool {return x;}); } }', 'i64')
    safe = denied('safe', 'import foreign stdffi.ipc_framed as I;\nfn main(){}',
                  'ON1X-KERNEL-NOT-ADMITTED', options=('--kernel-c11', '--kernel-stage'))
    denied('disabled', prefix+'fn main(){}', 'ON1X-KERNEL-UNSAFE-UNSUPPORTED', options=())
    for options in ('--kernel-bind', '--kernel-review-candidate'):
        output = tmp/'not-published'
        run(*driver, options, safe, '-o', output, error='ON1X-KERNEL-NOT-ADMITTED')
        assert not output.exists()

    session = '''let session = match I.open_session() with {
      Ok(s)=>move(s), Err(e)=>{println(e); return;}
    };
    '''
    denied('session_by_value', prefix+'fn f(s:I.Session){} fn main(){}', 'ON1X-KERNEL-OPAQUE')
    denied('session_nested_value', prefix+'record Fake { session:I.Session } fn main(){}', 'ON1X-KERNEL-OPAQUE')
    denied('session_outside_unsafe', prefix+'fn main(){ I.open_session(); }', 'ON1X-KERNEL-CONTEXT')
    denied('session_first_class', prefix+'fn main(){ unsafe { let poll=I.poll; } }', 'ON1X-KERNEL-DIRECT')
    denied('session_taken', prefix+'fn main(){ unsafe { '+session+'take(session); } }', 'ON1X-KERNEL-OPAQUE')
    denied('session_lexical_alias', prefix+'fn main(){ unsafe { '+session+
           'let alias=borrow mut session; } }', 'ON1X-KERNEL-OPAQUE')
    denied('session_copy', prefix+'fn main(){ unsafe { '+session+
           'let alias=session; } }', 'requires move(local)')
    denied('session_temporary', prefix+'fn main(){ unsafe { '+session+
           'I.poll(move(session),1000); } }', 'requires a named owner place')
    denied('session_moved', prefix+'fn main(){ unsafe { '+session+
           'drop(session); I.poll(session,1000); } }', 'borrow of moved')
    denied('session_later_argument', prefix+'fn main(){ unsafe { '+session+
           'I.poll(session, unsafe { drop(session); 1000 }); } }', 'during its exclusive borrow')
    denied('session_reentry_argument', prefix+'fn main(){ unsafe { '+session+
           'I.poll(session, unsafe { I.revoke(session); 1000 }); } }', 'overlapping exclusive borrow')
    denied('session_move_during_loan', prefix+'fn main(){ unsafe { '+session+
           'I.poll(session, unsafe { let moved=move(session); drop(moved); 1000 }); } }',
           'during its exclusive borrow')
    denied('session_callback_capture', prefix+'fn main(){ unsafe { '+session+
           'I.register_callback(session,fn(x:i64)->i64 { unsafe { I.poll(session,1000); } return x; }); } }',
           'capture')
    denied('session_deferred', prefix+'fn main(){ unsafe { '+session+
           'defer I.revoke(session); } }', 'ON1X-KERNEL-PROFILE')
    denied('session_with_csp', prefix+'fn main(){ unsafe { I.open_session(); } taskgroup {} }',
           'ON1X-KERNEL-PROFILE')
    denied('session_is_not_pair', prefix+valid+'fn main(){ unsafe { '+session+
           'I.roundtrip(move(session),0u8,1000,f); } }', 'Pair')

    (tmp/'service.on1x').write_text('''unsafe import kernel mulib.ipc_framed as A;
fn registered(n:i64) -> A.Result<own<A.Session>,i64> {
  let result=unsafe { A.open_session() };
  let resource=result?;
  let status=unsafe { A.register_callback(resource,fn(x:i64)->i64 { return x+n; }) };
  status?;
  return variant<A.Result<own<A.Session>,i64>>(Ok,move(resource));
}
''', encoding='ascii')
    client = tmp/'client.on1x'
    client.write_text('''import service as S;
unsafe import kernel mulib.ipc_framed as B;
fn visit(n:i64) -> B.Result<i64,i64> {
  let pending=S.registered(n);
  let resource:own<B.Session> = pending?;
  let sent=unsafe { B.send(resource,0u8,1000) };
  sent?;
  if n%2==0 { return unsafe { B.poll(resource,1000) }; }
  # Implicitly drop a still-registered closure and outstanding frame.
  return variant<B.Result<i64,i64>>(Ok,n);
}
fn fail_after_register(n:i64) -> B.Result<i64,i64> {
  let registered=S.registered(n);
  let resource:own<B.Session> = registered?;
  let queued=unsafe { B.send(resource,0u8,1000) };
  queued?;
  # An invalid timeout leaves the pending frame and registration intact.
  # The ? early return must drop both along with the session exactly once.
  let failed=unsafe { B.poll(resource,0) };
  let value=failed?;
  return variant<B.Result<i64,i64>>(Ok,value);
}
fn main() {
  var i=0;
  while i<260 {
    assert(match visit(i) with { Ok(n)=>n==i, Err(code)=>false });
    assert(match fail_after_register(i) with { Ok(n)=>false, Err(code)=>code == -1 });
    i=i+1;
  }
  println(i);
}
''', encoding='ascii')
    client_program = tmp/'client'
    run(*driver, '--kernel-bind', client, '-o', client_program)
    assert run(client_program) == '260\n'

    (tmp/'cleanup.on1x').write_text('''unsafe import kernel mulib.ipc_framed as Owned;
fn clean(explicit:bool) -> Owned.Result<i64,i64> {
  let pending=unsafe { Owned.open_pair() };
  let pair=pending?;
  if explicit {
    drop(pair);
    return variant<Owned.Result<i64,i64>>(Ok,1);
  }
  return variant<Owned.Result<i64,i64>>(Ok,2);
}
''', encoding='ascii')
    cleanup = tmp/'cleanup_main.on1x'
    cleanup.write_text('''import cleanup as Service;
unsafe import kernel mulib.ipc_framed as Other;
fn checked(explicit:bool) -> Other.Result<i64,i64> {
  return Service.clean(explicit);
}
fn main() {
  var i=0;
  while i<260 {
    assert(match checked(i%2==0) with { Ok(n)=>n==1+i%2, Err(code)=>false });
    i=i+1;
  }
  println(i);
}
''', encoding='ascii')
    cleanup_binary = tmp/'cleanup_program'
    run(*driver, '--kernel-bind', cleanup, '-o', cleanup_binary)
    assert run(cleanup_binary) == '260\n'

    # Reuse the MANIFEST planner from the measured link plan, and compile each
    # native source as C separately from the C++ ABI/fault-injection client.
    mulib = Path(interface['header']).parent
    shutil.copy(root/'tests/test_kernel_ipc_native.cpp', tmp/'client.cpp')
    impl = tmp/'mulib.c'
    impl.write_text(''.join(f'#define {name}\n' for name in data['plan']['implementation_macros']) +
                    '#include <mulib-sandbox-profile.h>\n#include <mulib-ipc-framed.h>\n', encoding='ascii')
    includes = ['-I', root/'runtime', '-I', mulib, '-DON1X_KERNEL_ABI_V1=1',
                *('-D'+define for define in data['plan']['compile_definitions'])]
    warnings = ['-Wall', '-Wextra', '-Wpedantic', '-Werror']
    objects = []
    for source in (impl, root/'runtime/on1x_runtime.c', root/'runtime/on1x_kernel_ipc.c'):
        obj = tmp/(source.stem+'.o')
        run(cc, '-std=c11', *warnings, *includes, *flags, '-c', source, '-o', obj)
        objects.append(obj)
    native = tmp/'native'
    run(cxx, '-std=c++17', *warnings, *includes, *flags, tmp/'client.cpp', *objects,
         *('-Wl,--wrap='+symbol for symbol in ('socketpair', 'close', 'shutdown', 'mulib_ipc_framed_init', 'mulib_ipc_send',
                                              'on1x_unique_try_box', 'clock_gettime')),
         '-pthread', '-lm', '-o', native)
    run(native)
