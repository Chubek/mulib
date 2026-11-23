# `std.environ` — POSIX process environment

```on1x
import std.environ as Env;

fn main() {
  println(Env.argc());
  let (found, value) = Env.getenv("HOME");
  if found { println(value); }
  let (ok, sysname) = Env.os();
  if ok { println(sysname); }
}
```

| API | Meaning |
| --- | --- |
| `argc() -> i64` | Argument count passed to C `main`, including `argv[0]` when present. |
| `argv() -> sexpr` | Snapshot of the startup argument vector as a proper list of string atoms, in order. |
| `environ() -> sexpr` | Snapshot of the current environment as a proper list of raw `NAME=value` string atoms, in host order. Entries are not split, sorted, or deduplicated. |
| `getenv(name) -> (bool, string)` | Presence and a copied value. A set but empty variable is `(true, "")`; absent or invalid names (empty or containing `=`) return `(false, "")`. |
| `os() -> (bool, string)` | Success and the host's `uname().sysname`, not the compiler target. |
| `uname() -> (bool, string, string, string, string, string)` | Success, system name, node name, release, version, and machine. Failure returns `false` and empty strings. Both OS queries use the same first successful snapshot. |
| `exec(path, args) -> i64` | Replace the current process with `path`, inheriting the current environment. Pass a proper list of string atoms for the arguments after `argv[0]`, which is set to `path`. There is no `PATH` search or shell fallback. On success it does not return, run On1x defers, or flush C stdio buffers. On failure it returns negative host `errno`. |
| `system(command) -> i64` | Run a trusted command through the host's POSIX shell and wait. Returns the shell's exit status, `128 + signal` if the shell was killed by a signal, or negative host `errno` if `system` itself fails. |

Use `exec` with separate arguments when you don't need shell syntax. **Do not
interpolate untrusted input into `system` commands.** Both APIs run with the
program's privileges, and neither sanitizes the inherited environment or open
descriptors. `system` blocks the calling native thread; `exec` replaces the
entire process, not just an On1x task.

This module requires the direct C11 backend and a POSIX host libc. The build
helper automatically links `environ/runtime.c` from the selected stdlib. When
linking emitted C by hand, compile that file with the usual runtime include
path and link it alongside `on1x_runtime.c`. The generated entry point passes
the real `argc` and `argv` to the companion before executing On1x code.

Arguments and environment values are byte-oriented NUL-free strings.
List snapshots reject more than 4096 entries. `exec` rejects more than 4095
supplied arguments; the OS can impose smaller limits. The normal runtime
text, snapshot, and allocation limits also apply. Reading the environment
while native code concurrently mutates it is unsupported.

### Raw C boundary

The implementation uses handwritten scalar externs, not a reviewed safe
FFI contract. `on1x_environ_argc`, `on1x_environ_arg`, `on1x_environ_count`,
and `on1x_environ_entry` read the startup vector or the libc environment.
The internal string accessors borrow host storage only long enough for the
On1x wrapper to copy it. `on1x_environ_has` and `on1x_environ_getenv` query
libc; the latter returns an On1x text snapshot. `on1x_environ_uname_ready`
and `on1x_environ_uname_field` cache and read the POSIX host fields.
`on1x_environ_exec` validates a length-framed string to construct a C
argument vector for `execv`; `on1x_environ_system` wraps POSIX `system`.
Neither retains inputs on return. These symbols are internal to the
companion; the `std.environ` facade exports only the On1x wrappers.
