# `std.daemon`

The package has two independent surfaces: the On1x single-byte IPC dispatcher
and real-process supervision through an explicit native companion. Neither
uses the other, and using the dispatcher does not launch a process.

## On1x: single-byte local IPC dispatch

This API invokes one handler **in the calling process**. It does not connect to
a supervised process.

```on1x
import std.daemon as Daemon;

fn main() {
  let reply = Daemon.basis_daemon_ipc_dispatch(0u8, 1000, fn(byte: i64) -> i64 {
    return byte + 42;
  });
  match reply with {
    Ok(value) => println(value),
    Err(status) => println(status)
  };
}
```

`basis_daemon_ipc_dispatch(request: u8, timeout_ms: i64, handler: fn(i64) -> i64)`
returns the framed adapter's nominal `Result<i64, i64>`. An invalid timeout
outside `1..1000` ms returns `Err(-1)` before opening a socket. Otherwise the
function opens a private `AF_UNIX` stream pair, sends one binary byte (including
NUL), and receives it through MuLIB framing under one monotonic deadline.
Only on success does it invoke the handler, exactly once on the caller's thread,
with the received byte widened to `i64` (`0..255`). Its result is an `Ok`, even
if it is negative. Opening and transport failures return the unchanged
[adapter status](../../docs/kernel-framed.md#one-shot-pair) and skip the handler.

Only the dispatcher owns the pair. The adapter consumes the owner and closes
both descriptors before calling the handler, including when the handler then
aborts the program. It retains no handler, and no socket or owner escapes.
There is no retry. The deadline bounds the framed transfer, not socket setup
or handler execution.

### IPC admission and limits

The implementation explicitly uses `unsafe import kernel mulib.ipc_framed`
with local `unsafe` blocks and the existing native Linux x86-64 GNU C11
KernelBind profile. Importing the wrapper does not turn the raw profile into a
reviewed safe kernel import. Safe `import kernel mulib.ipc_framed` still fails.
Use the static KernelBind build route below; this package requires neither
dynamic kernel compilation nor an additional C shim. It exposes no reusable
session, external peer, path, listener, or network connection.

The CMake installation includes these IPC sources. The restricted
`on1x-project.py` standard-library source package excludes them: it does not
support KernelBind admission or linking.

MuLIB's [`daemon`](../../../manual/mulib-daemon.md) and
[`process`](../../../manual/mulib-process.md) kernels exist in
[`MANIFEST.yaml`](../../../MANIFEST.yaml), but On1x KernelBind has **no callable
profile** for either. The frontend rejects safe requests with
`ON1X-KERNEL-NOT-ADMITTED` and unsafe requests with
`ON1X-KERNEL-UNSAFE-UNSUPPORTED`. C declaration discovery alone cannot admit
them. The companion below is called by trusted C host code, not by a foreign
declaration from On1x. It does not pass raw pointers, process IDs, ownership
tokens, or callbacks across an unreviewed ABI.

### IPC build and test

Use `on1xc`, [Lua-enabled Exotract](../../docs/kernel-bind.md#inspect-requests-and-discover-declarations),
Clang resource headers, and a native C compiler. From the repository root,
point `sdk` at an existing SDK build, and choose fresh output paths:

```sh
sdk=build/kernel-shared-general
exo="$sdk/libstone-moonstone/exotract/exotract"
python3 on1x/tools/on1x-build.py on1x/stdlib/daemon/fixtures/dispatch.on1x \
  --kernel-bind --no-prelude --on1xc "$sdk/on1xc" --exotract "$exo" \
  --cc cc --resource-dir "$(clang -print-resource-dir)" \
  --mulib-plan "$sdk/daemon-ipc-link.json" -o "$sdk/daemon-ipc"
"$sdk/daemon-ipc"

python3 on1x/stdlib/daemon/test_daemon_ipc.py \
  --on1xc "$sdk/on1xc" --exotract "$exo" --cc cc
```

The fixture prints `0`, `255`, and `260` on separate lines. The test also
checks invalid timeouts, descriptor exhaustion, repeated cleanup under a
descriptor limit, the one-translation-unit MuLIB link plan, and rejection
without the required KernelBind mode. The existing `on1x.kernel_ipc_sdk` test
covers injected allocation and transport failures in the underlying adapter.

## Native companion: owned process supervision

Include [`supervisor.h`](supervisor.h) from trusted native host code and link
[`supervisor.c`](supervisor.c) with the MuLIB daemon and process kernels. This
profile implements real lifecycle operations on Linux; other platforms and
builds explicitly disabling POSIX process support return `BASIS_DAEMON_UNSUPPORTED`.
No service manager, Mune script, network, framed IPC, or On1x closure participates.

1. Configure a trusted `basis_daemon_runner` table (1–64 entries) mapping stable
   names to absolute executable and working directory paths. Create an owner
   with `basis_daemon_create_supervisor(&owner)` in a `NULL` slot.
2. A `basis_daemon_job` chooses a name and supplies up to 4,096 bytes of
   length-delimited context. `basis_daemon_validate_job` validates the whole
   table (including duplicate names), the name and its authorization, and the
   context bound without performing I/O. `basis_daemon_validate_limits` checks
   the deadline bounds. The caller and runner must separately validate their
   application-specific data schema and any I/O policy.
3. Use `basis_daemon_start` and `basis_daemon_poll`/`basis_daemon_wait` to drive
   an owned child, or `basis_daemon_run` for one bounded attempt. A successful
   start reports either `RUNNING` or an already observed terminal state; the
   exec handshake is **not** a service readiness notification.
4. `basis_daemon_stop` requests `SIGTERM`, waits up to the grace period, then
   uses `SIGKILL` and waits up to the reap budget. Zero grace requests only
   `SIGKILL`. `basis_daemon_restart` prepares the next command before stopping
   the current child and launches it only after the old child is reaped. There
   is no automatic restart for an individually managed child, and no
   rollback if the replacement fails to exec.
5. When `event.child_owned` is true, retain the owner and continue polling or
   stopping, including after any error. `basis_daemon_destroy_supervisor`
   rejects a still-owned child without altering the slot, and clears the slot
   on success.

`basis_daemon_event` distinguishes running, normal exit (including a nonzero
code), signal exit, stop and timeouts. `system_error` supplies an applicable
POSIX error, for example `ENOENT` for a missing executable. A `wait` budget
expiring returns `OK` with `RUNNING` and does not signal. A `run` budget
expiring returns `TIMEOUT` with `RUN_TIMEOUT` even after successful cleanup;
a cleanup system error is returned as such and may retain the child. Poll and
wait preserve the last terminal event after reaping. Stopping an idle owner
is an `IDLE` no-op and keeps that event available via poll.

The positive millisecond budgets are `1..60000` and measured independently
with a monotonic clock. The startup budget limits the wait for the exec
handshake; failure or expiry triggers forced cleanup under the reap budget.
Deadlines do not guarantee real-time bounds on fork, scheduling, or a child
in an uninterruptible kernel operation. If cleanup misses its deadline, the
caller must keep supervising the same owner; no child is silently detached.

### Background queue and restart policy

Use `basis_daemon_queue_create(capacity, &queue)` for one independently
running child and a FIFO of up to 16 waiting jobs. `basis_daemon_queue_submit`
copies the prepared command, context and policy and returns a queue-local ID.
Submitting when full returns `LIMIT`. There is no queue thread, persistent
state, or implicit event loop: call `basis_daemon_queue_step` from the host
to start the next job, poll the child, enforce its run budget, and advance
backoff or cleanup. A child runs between calls; budget expiration is only
observed on the next step, and an already reaped exit takes precedence over a
timeout on that step. Starting a child or cleaning up in a step can wait up
to the configured phase budgets.

Select `RESTART_NEVER`, `RESTART_ON_FAILURE`, or `RESTART_ALWAYS` in
`basis_daemon_policy`. A restart policy requires `1..8` **additional**
attempts and a `1..60000` ms monotonic backoff. `ON_FAILURE` covers launch
failure, abnormal exit and run timeout; `ALWAYS` additionally restarts a
successful exit. Neither retries ownership errors or canceled jobs. A job
holds the active slot during cleanup and backoff, so the queue cannot start
another child until the previous one is reaped. The `basis_daemon_queue_event`
reports queue state, ID, attempt number, waiting count, and the nested
process event/status. `queue_step` returning `OK` only means the step was
handled; inspect the nested result for the actual attempt outcome. The queue
does not retain a history after reporting a job's completion.

`basis_daemon_queue_cancel` removes a waiting job without spawning it, or
stops an active child and suppresses its retries. `basis_daemon_queue_shutdown`
closes admission, discards waiting jobs (reporting the count), and cancels
active work. If cleanup is incomplete, repeat shutdown or step; destruction
rejects any remaining active or waiting job instead of silently abandoning it.

### Typed On1x application values

Import `std.daemon.lifecycle` directly for `DaemonSpecification<C>`,
`DaemonMessage<C>` (submit, cancel, status, shutdown), `DaemonPolicy`, and
`DaemonJobId`. The generic context is typed in application code; these pure
values neither serialize it nor grant OS authority. The native companion
accepts only the separately encoded, length-delimited context, and never
uses an On1x job ID as a process handle. Standalone use needs no Probe, Elvi,
or Kron imports. [`fixtures/lifecycle_types.on1x`](fixtures/lifecycle_types.on1x)
exercises a generic specification, command matching, and policy validation
without enabling KernelBind.

### Context and trust boundary

An approved runner is started directly with `execve`, without PATH lookup,
shell interpolation, or ambient environment. It starts a new session with
the configured cwd, umask 027 and `/dev/null` on stdin/stdout/stderr.
`argv[1]` is the runner name; `argv[2..]` hold a canonical sequence of lowercase
hex chunks (one empty argument for empty context). The runner should call
`basis_daemon_decode_context` with its deployment's expected name to recover
the bounded bytes, including binary NUL. Decoding confirms envelope shape, not
authority. Arguments are visible to OS process inspection; do not use this
transport for secrets. Never place an executable path, function pointer,
closure, owner, or runtime handle in the persisted job descriptor.

The executable and cwd **must** be chosen by trusted deployment code, stored
where untrusted principals cannot replace them, and not derived from the
descriptor. This is not a sandbox. The runner runs under the current process
credentials and opens its own resources. Nonstandard descriptors can be
inherited unless marked close-on-exec by the host. Signal masks and inherited
ignored dispositions also remain the host's responsibility.

Initialize/launch in a single-threaded host. Serialize owner operations and
use an owner only in its creating process. No global signal handlers are
installed: the caller must leave `SIGCHLD` at its default disposition without
`SA_NOCLDWAIT`, and must not let another component reap the child (including
via `waitpid(-1, ...)`). Only the direct child is supervised, not descendants;
use foreground runners that do not fork off unmanaged work. The companion does
not install units, maintain PID files, or leave a background manager. An
unexpected supervisor process exit is not an orderly shutdown and may leave
the independently running child alive.

### Native build and test

The companion uses the daemon setters and additive exec-only
`mulib_daemon_launch_begin` / `mulib_daemon_launch_status` handshake, and
`mulib_process_wait` for nonblocking reaping. It never calls the legacy blocking
launch or terminate APIs. `lifecycle_kernels.c` is the standalone C11 kernel
implementation translation unit. If a combined host already implements any
of those kernels, merge the required switches into that host's **sole** kernel
translation unit instead of linking both.

```sh
out="$(mktemp -d "$PWD/build/daemon-supervisor-XXXXXX")"
mkdir "$out/work"
TMPDIR="$out" cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels \
  on1x/stdlib/daemon/supervisor.c on1x/stdlib/daemon/lifecycle_kernels.c \
  on1x/stdlib/daemon/test_supervisor.c -pthread -lm -ldl \
  -Wl,--wrap=execve,--wrap=malloc,--wrap=calloc,--wrap=waitpid \
  -o "$out/test-supervisor"
"$out/test-supervisor" "$out/work"
```

The test runs real child processes, with targeted injections for allocation
failure, a stalled exec handshake, and delayed reaping. The `--wrap` options
are test-only; production hosts do not use them. The IPC test above remains
independent and uses its existing KernelBind build route.
