# `mulib-process-stream.h`

**Macro:** `MULIB_IMPL_PROCESS_STREAM` in one C11 translation unit. Depends on
static-inline `mulib-pty.h`. Linux/POSIX; Linux requires `_GNU_SOURCE`.

`mulib_ps_start(&process, &spec)` creates a child process group, an optional
stdin socketpair and independent stdout/stderr pipes, or a PTY with controlling
terminal. The child runs `file` with explicit `argv`; no shell expansion occurs
unless the caller intentionally invokes a shell. `env=NULL` inherits host
environment. A non-NULL environment replaces it and is used for explicit PATH
lookup. Explicit working directory may be supplied. The child reports pre-exec
failure via a close-on-exec handshake pipe. The same deadline applies while
waiting for this handshake; on timeout, start cancels and reaps the child.

`mulib_ps_pump(&process, wait_ms, on_chunk, user)` polls, incrementally reads
binary stdout/stderr data and invokes the callback with stream id 1 or 2. For
PTY mode both streams share id 1. Callback must return 1 to continue. The
process output limit is **cumulative**, not per chunk. The deadline is absolute
from process start, based on `CLOCK_MONOTONIC`. An output limit, callback
failure or deadline triggers cancellation. `mulib_ps_wait` pumps until all
output descriptors hit EOF and the direct child has been reaped.

`mulib_ps_write` performs a nonblocking stdin write. Non-PTY mode uses an
AF_UNIX socketpair and `MSG_NOSIGNAL` to avoid killing the caller with
`SIGPIPE`; `mulib_ps_close_input` closes that half to deliver EOF. PTY close
input alone is unsupported because input/output share a descriptor.
`mulib_ps_cancel` sends SIGTERM to the child process group and escalates to
SIGKILL if necessary. Always call `mulib_ps_destroy` to close descriptors and
reap a live child.

## Limits

- Not a sandbox, an executable allowlist, or an authorization layer.
- Unrelated descriptors inherited from the host must already be close-on-exec
  if the executed program must not receive them.
- The caller must provide strictly positive deadline/output bounds.
- Forking/configuring a child is designed for a **single-threaded supervisor**;
  it does not promise async-signal-safe behavior after fork in a multithreaded
  host. For multithreaded supervisors use a dedicated launcher worker or extend
  to a verified `posix_spawn` profile.
- Descendant process supervision is best-effort process-group based. After the
  direct child is reaped, an orphaned grandchild may still hold inherited
  output fds; the deadline prevents indefinite pumping, but the runner does
  not kill a process-group ID that may have been reused. Use cgroup-v2 and an
  OS sandbox/launcher policy for stricter containment and cleanup.
- PTY support depends on a host `devpts` setup; it may be unavailable in
  restricted containers. The PTY is byte-oriented and offers resize only at
  creation in this kernel; use `mulib_pty_resize` for later resize.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-process-stream.h`](../kernels/mulib-process-stream.h) |
| Implementation switch | `MULIB_IMPL_PROCESS_STREAM` |
| Repository-local includes | [mulib-pty](notebook-desktop.md) |
| Complete example | [`examples/process_stream.c`](../examples/process_stream.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/process_stream.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
