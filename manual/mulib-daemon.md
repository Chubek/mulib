# MuLIB daemon

**Header:** `include/mulib-daemon.h` · **Switch:** `MULIB_IMPL_DAEMON`

Build an owned daemon specification with C setters or a Mune configuration;
compile it through a Mune profile, or launch it as an owned POSIX child session.
The profile libraries live in **`library/ldaemon`**.

## Integration

Define these switches in exactly one C11 translation unit, before including the
header (no dependency switches are enabled automatically):

```
MULIB_IMPL_DAEMON MULIB_IMPL_PROCESS MULIB_IMPL_MUNE MULIB_IMPL_DYNLOAD
MULIB_IMPL_MIR MULIB_IMPL_VMWEAVE MULIB_IMPL_EGRAPH MULIB_IMPL_REWRITE
MULIB_IMPL_DYNCALL MULIB_IMPL_JIT MULIB_IMPL_PARSEC MULIB_IMPL_PARSER
MULIB_IMPL_PEG MULIB_IMPL_AST MULIB_IMPL_STRBUF MULIB_IMPL_GLOB
```

Link with `-pthread -lm -ldl` on Linux. Configuration/profile compilation is
portable wherever Mune runs. Native launching uses POSIX fork, setsid and execve;
other systems return UNSUPPORTED. `MULIB_DAEMON_NO_POSIX` explicitly disables it.

## Specification API

`mulib_daemon_create(name)` owns a copy of a 1..127-byte ASCII service name
(letters, digits, `_`, `-`, `.`, except `.` and `..`). Defaults are description =
name, working directory `/`, restart `on-failure`, no command and an empty explicit
environment. There is no ambient PATH lookup.

* `description`, `directory`, `restart` replace copied strings. Directories must
  be absolute; policies are `no`, `on-failure`, `always`.
* `command` replaces argv including an absolute executable in argv[0]. It accepts
  1..256 arguments, including empty non-executable arguments.
* `environment` adds/replaces a copied key/value, up to 256 keys. Keys are ASCII
  identifiers with a nonnumeric first character; updating a key keeps its position.

Text fields are limited to 4095 bytes and reject ASCII controls, including newlines
and DEL. Setters validate/allocate before replacing existing storage. `destroy`
frees a spec, never a live process; process ownership is independent.

## Mune configurations

`mulib_daemon_load(source,length,error)` evaluates a configuration in a fresh Mune
state. It exposes these native functions:

```lua
daemon_name('source-watch')
daemon_description('Watch source changes')
daemon_directory('/srv/project')
daemon_command('/usr/local/bin/watch-runner', '--watch-program', '/etc/watch.fsw')
daemon_environment('LANG', 'C')
daemon_restart('on-failure')
```

The command is required. Parsing/runtime/validation errors destroy the partial
specification and return NULL; errors include an owned diagnostic and byte offset.
Mune retains its normal standard libraries: configuration scripts are application
code, and their explicit I/O side effects are not rolled back.

`library/ldaemon/service.lua` returns a table-based configuration helper:

```lua
local service = dofile('library/ldaemon/service.lua')
service {
  name = 'source-watch',
  argv = { '/usr/local/bin/watch-runner', '--watch-program', '/etc/watch.fsw' },
  environment = { LANG = 'C' },
}
```

It rejects unknown fields and sorts environment keys for reproducible compilation.
`library/ldaemon/fswatch.lua` provides a smaller helper accepting name, foreground
runner executable and watch-program path. These helpers can also be bundled using
Mune's module-bundle API. Paths used by `dofile` are resolved from the compiling
application's current working directory, not from the configuration file.

## Profile compilation and systemd

`compile` executes a profile with a global `daemon` table containing `name`,
`description`, `directory`, `restart`, `argv` (1-based array), and `environment`
(array of `KEY=value` strings). A profile must return exactly one NUL-free string.
The string is appended to an initialized `mulib_strbuf_t` only after successful
evaluation. `compile_file` and `load_file` bound file input to 1 MiB; the Mune
runtime also applies its documented step, memory and call-depth limits.

`library/ldaemon/systemd.lua` compiles a `.service` unit with Unit, Service and
Install sections, `Type=simple`, ExecStart, WorkingDirectory, Environment, Restart,
UMask and `WantedBy=multi-user.target`. Arguments are quoted individually;
backslashes, quotes, systemd `%` specifiers and ExecStart `$` expansion are escaped.
Description and WorkingDirectory use systemd's unquoted scalar syntax with a
trailing-space sentinel to prevent backslash line continuation. The profile rejects
boundary spaces in these two fields, which systemd would otherwise strip.
All emitted text fields must be valid UTF-8; invalid strings fail profile evaluation.
It emits argv directly, never a shell command. Run the application **in foreground**
under systemd: systemd owns daemon supervision. The restart policy is implemented
by systemd; the native launcher itself does not restart processes. A systemd
manager may supply additional environment beyond emitted Environment assignments.

Compilation returns text; it does not write into `/etc`, install units or invoke
systemctl. The application chooses its output filename and publication mechanism.

## Native launch and lifecycle

Initialize an unused `mulib_process_t`, then call `mulib_daemon_launch` with a spec.
A NULL entry executes argv directly with the explicit environment. A non-NULL
entry runs application C code in the child and can operate without a command.
The launcher forks once, creates a new session, changes cwd, sets umask 027 and
connects stdin/stdout/stderr to `/dev/null`. It uses a close-on-exec error pipe so
setup/exec failures are reported to the parent and failed children are reaped.
Success means child setup/exec succeeded, not that the application has finished
initializing or will remain alive.

The child remains owned and waitable; there is no double-fork or global PID file.
Use `mulib_process_wait` to collect status. `mulib_daemon_stop` checks/reaps an
already-exited child or sends SIGTERM to a live one; then wait to reap a live
child. `mulib_process_terminate` provides forced termination/reaping if desired.
Only one owner may reap a child. Stop signals the daemon itself, not its descendants.

Launch before creating application threads. Fork copies callback data; parent and
child memory modifications are independent. Open backend resources in the entry
callback. Nonstandard descriptors and signal masks/dispositions are inherited;
applications own their descriptor/signal policy. Callback entries return 0..255;
other return values map to exit code 1. No global signal handlers are installed.
Daemon specs/runtime state require external synchronization.

## Example and verification

```sh
cc -std=c11 -Iinclude examples/modules/daemon.c -pthread -lm -ldl -o /tmp/opencode/daemon
/tmp/opencode/daemon library/ldaemon/systemd.lua > /tmp/opencode/example.service
```

`tests/test_daemon_fswatch.c` exercises Mune helpers, profile escaping and failed
compilation, copied environments, child callbacks, exec/setup failure reporting,
exit statuses, watcher launch, SIGTERM and reaping.
