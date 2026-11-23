"""Run the std.environ facade through the real direct C11 build path."""
import errno
import os
import shutil
import signal
import sys

from support import NativeTests, run


t = NativeTests()
work = t.work.resolve()
stdlib = work / 'relocated stdlib'
shutil.copytree(t.root / 'stdlib/environ', stdlib / 'environ', dirs_exist_ok=True)
flags = ['--cflag=' + flag for flag in t.flags]


def build(name, text):
    source = t.source(name, text)
    executable = work / name
    result = run([sys.executable, t.root / 'tools/on1x-build.py', source,
                  '--on1xc', t.compiler, '--cc', t.cc, '--no-prelude',
                  '--stdlib', stdlib, '-o', executable, *flags])
    assert result.returncode == 0, result.stderr
    return executable


program = build('process info', r'''
import std.environ as E;
fn main() {
  let args = E.argv();
  assert(E.argc() == 3);
  println(sx_text(sx_head(sx_tail(args))));
  assert(sx_text(sx_head(sx_tail(sx_tail(args)))) == "");
  assert(sx_kind(sx_tail(sx_tail(sx_tail(args)))) == 0);
  let (present, value) = E.getenv("ON1X_TEST_VALUE");
  let (empty, blank) = E.getenv("ON1X_TEST_EMPTY");
  let (missing, absent) = E.getenv("ON1X_TEST_MISSING");
  let (bad, invalid) = E.getenv("ON1X_TEST_VALUE=");
  assert(present && value == "a=b\nc" && empty && blank == "");
  assert(!missing && absent == "" && !bad && invalid == "");
  var entries = E.environ();
  while sx_kind(entries) == 3 {
    println(sx_text(sx_head(entries)));
    entries = sx_tail(entries);
  }
  let (ok, sysname, nodename, release, version, machine) = E.uname();
  let (os_ok, os_name) = E.os();
  assert(ok && os_ok && os_name == sysname);
  println(sysname); println(nodename); println(release); println(version); println(machine);
  println(E.system("exit 7"));
  println(E.system("kill -TERM $$$$"));
}
''')
env = {'ON1X_TEST_VALUE': 'a=b\nc', 'ON1X_TEST_EMPTY': ''}
result = run([program, 'two words', ''], env=env)
expected = ['two words', *(f'{k}={v}' for k, v in env.items()), *os.uname(),
            '7', str(128 + signal.SIGTERM)]
assert result.returncode == 0, result.stderr
assert result.stdout == '\n'.join(expected) + '\n', result.stdout
result = run([program, *([''] * 4096)], env=env)
assert result.returncode == 73 and 'argv snapshot entry limit' in result.stderr, result.stderr
many_entries = {**env, **{f'VAR_{i}': '' for i in range(4096)}}
result = run([program, 'two words', ''], env=many_entries)
assert result.returncode == 73 and 'environ snapshot entry limit' in result.stderr, result.stderr

(work / 'not-an-executable-format').write_text('exit 23\n')
(work / 'not-an-executable-format').chmod(0o700)
replace = build('exec test', r'''
import std.environ as E;
fn main() -> i64 {
  let args = E.argv();
  if E.argc() == 4 {
    let received = sx_tail(args);
    assert(sx_text(sx_head(received)) == "a:b ; $$HOME\n\"q\"");
    assert(sx_text(sx_head(sx_tail(received))) == "");
    assert(text_byte(sx_text(sx_head(sx_tail(sx_tail(received)))), 0) == 1);
    let (present, value) = E.getenv("ON1X_TEST_VALUE");
    assert(present && value == "a=b\nc");
    return 42;
  }
  if E.argc() == 2 {
    return E.exec(sx_text(sx_head(args)), sx_cons(sx_string("a:b ; $$HOME\n\"q\""),
      sx_cons(sx_string(""), sx_tail(args))));
  }
  let n = data_live_nodes();
  println(E.exec("./not-an-executable-format", sx_nil()));
  println(E.exec("./does-not-exist", sx_nil()));
  println(E.exec("", sx_nil()));
  println(E.exec("/bin/sh", sx_cons(sx_symbol("invalid"), sx_nil())));
  println(E.exec("/bin/sh", sx_string("not a list")));
  assert(data_live_nodes() == n);
  var list = sx_nil();
  var i = 0;
  while i < 4096 {
    list = sx_cons(sx_string(""), list);
    i = i + 1;
  }
  println(E.exec("/bin/sh", list));
  return 0;
}
''')
result = run([replace, '\x01'], cwd=work, env=env)
assert result.returncode == 42, (result.returncode, result.stdout, result.stderr)
result = run([replace], cwd=work, env=env)
assert result.returncode == 0, result.stderr
expected = [errno.ENOEXEC, errno.ENOENT, errno.EINVAL, errno.EINVAL, errno.EINVAL,
            errno.E2BIG]
assert result.stdout == ''.join(f'{-code}\n' for code in expected), result.stdout

bad = t.source('environ_nul', r'''
import std.environ as E;
fn main() { println(E.exec("/bin/echo", sx_cons(sx_string("x\0y"), sx_nil()))); }
''')
result = run([t.compiler, '--check', bad])
assert result.returncode != 0 and 'NUL escape' in result.stderr, result.stderr

harness = work / 'environ_native.c'
harness.write_text(r'''
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
static void *test_malloc(size_t size);
#define ON1X_ENVIRON_MALLOC test_malloc
#include "runtime.c"
#include <assert.h>
static bool inject_failure;
static void *test_malloc(size_t size) {
  return inject_failure ? NULL : malloc(size);
}
int main(void) {
  assert(on1x_environ_exec("/bin/sh", 1, "3:a") == -EINVAL);
  assert(on1x_environ_exec("/bin/sh", 0, "0:") == -EINVAL);
  assert(on1x_environ_exec("/bin/sh", 1, "999999999999999999999999999999:x") == -EINVAL);
  assert(on1x_environ_exec("/bin/sh", INT64_MAX, "") == -E2BIG);
  inject_failure = true;
  assert(on1x_environ_exec("/bin/sh", 0, "") == -ENOMEM);
  return 0;
}
''')
native = work / 'environ_native'
result = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
              '-I', t.root / 'runtime', '-I', stdlib / 'environ', harness,
              t.root / 'runtime/on1x_runtime.c', *t.flags, '-o', native])
assert result.returncode == 0, result.stderr
result = run([native])
assert result.returncode == 0, result.stderr
