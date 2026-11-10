# mulib-parallel-sh

[Manuals](README.md) · [Library index](index.md)

`mulib-parallel-sh.h` runs shell commands in bounded POSIX child processes,
captures grouped output, retries failed jobs, and provides named thread-safe
string channels. An optional [Mune](mulib-mune.md) adapter programs the scheduler
and runs isolated Lua jobs. This module is distinct from the thread-pool-based
[`mulib-parallel`](mulib-parallel.md).

## Build

Define `MULIB_IMPL_PARALLEL_SH` in exactly one C11 translation unit. POSIX builds
require `_POSIX_C_SOURCE=200809L` before any system header and `-pthread`.
For shell-only use, define `MULIB_PARALLEL_SH_NO_MUNE` consistently in all
translation units:

```c
#define _POSIX_C_SOURCE 200809L
#define MULIB_PARALLEL_SH_NO_MUNE
#define MULIB_IMPL_PARALLEL_SH
#include "mulib-parallel-sh.h"

int main(void) {
    mulib_parallel_sh_t jobs;
    int rc = mulib_parallel_sh_init(&jobs, 2);
    if (rc) return 1;
    jobs.keep_order = 1;
    rc = mulib_parallel_sh_add_input(&jobs, "printf '%s\\n' {}", "hello world");
    if (!rc) rc = mulib_parallel_sh_run(&jobs);
    mulib_parallel_sh_free(&jobs);
    return rc ? 1 : 0;
}
```

Without `NO_MUNE`, link the Mune implementation and its dependencies as described
in its manual. [examples/parallel.c](../examples/parallel.c) supplies the complete
implementation switches and CLI entry point:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  examples/parallel.c -pthread -lm -ldl -o parallel-sh
```

## Scheduler lifecycle and ownership

`mulib_parallel_sh_init(&p, max_jobs)` initializes fresh storage; zero jobs selects
the online CPU count, falling back to one. Free an existing scheduler before
initializing it again. `add`, `add_input`, and `add_lua` copy their strings.
Commands and inputs must be NUL-terminated text. An empty shell command is valid.

Configure public fields before the synchronous, one-shot `run`:

| Field | Default and meaning |
| --- | --- |
| `max_jobs` | Positive maximum number of simultaneously running children |
| `shell` | Borrowed `/bin/sh`; invoked with `-c COMMAND`, searched through PATH |
| `keep_order` | Zero: emit when completion is observed; nonzero: submission order |
| `retries` | Zero; nonnegative number of **additional** attempts after failure |
| `halt_on_error` | Zero; nonzero stops submitting jobs after a final job failure |
| `dry_run` | Zero; nonzero prints expanded commands without spawning children |
| `stdout_file`, `stderr_file` | Borrowed `stdout`, `stderr`; NULL discards that stream |
| `joblog` | NULL; optional borrowed writable log stream |

`run` refuses a second run, zero `max_jobs`, or negative retries. After a run has
started, additions are refused. Dry-run also consumes the scheduler, but leaves
job attempt/completion counters at zero. `free` releases jobs, captures and
channels, clears the structure, and permits repeated calls; it does not close
caller-owned output streams.

Each attempt receives `/dev/null` as stdin and seekable temporary stdout/stderr
files. Only the final attempt's output is emitted. Output is copied in chunks;
large output can consume disk. With ordered output, completed jobs may retain
their capture files while waiting for earlier jobs. There is no total task,
output-size or execution-time limit. Stdout and stderr are grouped separately,
not merged into a cross-stream chronology.

`completed` counts finished jobs, `failed` counts final nonzero job results,
and `active` returns to zero before `run` returns. `last_status` retains the run's
result. Each job exposes `attempts`, `finished`, `exit_code`, and `term_signal`.
Signal exits use `128 + signal`; shell exec failure uses 127 and child descriptor
setup failure uses 126. Jobs skipped by halt or a system failure remain unfinished
with zero attempts. System-error cleanup may mark interrupted children as failed
with code 127. Only scheduler-owned children are waited for or terminated.

On halt, already-running jobs finish, including their configured retries. System
failures stop further queued submissions. Capture, fork, wait, output, and log
errors return `SYSTEM`, which takes precedence over ordinary job failures.

Scheduler mutation and execution require exclusive caller ownership. Run from
a single-threaded process: the forked Lua path creates a runtime before exec,
and inherited library locks must not be held by other threads. Join channel
users before freeing the scheduler. The API does not install signal handlers
or provide process-group cancellation.

## Expansion

`mulib_parallel_sh_expand(pattern, input, index)` returns a malloc-owned string,
or NULL on invalid arguments/allocation failure. Free it with `free`.
`add_input` expands and queues it, retaining a copy of the input; its index is
the one-based submission number.

| Token | Replacement for `/dir/report.txt` |
| --- | --- |
| `{}` | `/dir/report.txt` |
| `{.}` | `/dir/report` |
| `{/}` | `report.txt` |
| `{/.}` | `report` |
| `{//}` | `/dir` (`.` for a slashless name) |
| `{#}` | Decimal index |

Only the last extension in the basename is removed. A leading dot alone is not
an extension: `.profile` remains `.profile`. Paths are processed lexically;
there is no filesystem lookup or path normalization. Unknown token text is
preserved. If no recognized token occurs, the quoted input is appended.

Input-derived replacements are POSIX single-quoted, including embedded quotes,
spaces and newlines. **Put tokens outside existing shell quotes.** The pattern
itself is shell source; it is not escaped or parsed as an argv array.

## Channels

`channel_create(p, name, &channel)` copies a nonempty name; creating the same name
returns the same channel. `channel_find` returns a borrowed pointer or NULL.
Serialize registry creation/lookups against registry mutation.

On POSIX, send, receive, wait, close and limit changes on an existing channel
are mutex-protected and can be called from multiple threads:

- `channel_send(c, text)` copies a NUL-terminated string into a FIFO queue.
- `channel_set_limit(c, n)` bounds queued messages; zero restores the default
  unlimited queue. A limit below current queue size is rejected. A send at the
  limit returns `FULL`; it never blocks or drops an existing message.
- `channel_receive(c, &text)` transfers a malloc-owned string, or returns `EMPTY`.
- `channel_receive_wait(c, &text, ms)` polls for zero, waits indefinitely for
  `UINT64_MAX`, or uses a finite realtime-clock deadline. A timeout returns
  `EMPTY`. Unrepresentable deadlines return `INVALID`.
- `channel_close(c)` is idempotent and wakes all waiters. Queued messages remain
  receivable; once drained, receives return `CLOSED`. Sends return `CLOSED`.

Receive outputs are NULL on failure. Empty strings are valid messages; binary
NULs are not representable. Channels are in-process thread queues, not IPC
between forked jobs. Stop/join all users before scheduler destruction.

## Lua programming

`mulib_parallel_sh_lua(p, source, length, error, capacity)` creates a fresh Mune
state for each call. `lua_file` reads at most 1 MiB from a seekable file.
The optional error buffer receives a bounded diagnostic. Scripts execute on the
caller thread, with these functions:

| Function | Meaning |
| --- | --- |
| `task(command [, input])` | Queue shell source, optionally with input expansion |
| `lua_task(source)` | Queue Lua source in a fresh isolated child Mune state |
| `channel(name)` | Create/find a channel |
| `send(name, text)` | Send a string to an existing channel |
| `recv(name)` | Receive a string, or nil if empty/closed |
| `run()` | Run once and return boolean success |

Bindings reject embedded NULs. Script side effects persist on error. A successful
configuration script is not itself proof of job success: inspect `last_status`
when the script called `run`. The CLI does this automatically. Child Lua jobs do
not inherit the configuration state's globals or channel bindings.

## Command line

`mulib_parallel_sh_main(argc, argv)` returns 0 for success, 1 for execution/input
failure, or 2 for invalid options/usage. It accepts:

```text
-j N, -jN, --jobs N, --jobs=N       concurrency; 0 selects CPUs
-k, --keep-order                   emit in submission order
-0, --null                         NUL-delimited input
-a FILE, --arg-file FILE           input file; - means stdin
--shell PROGRAM                   shell executable
--dry-run                         print expanded shell source
--retries N, --retries=N           additional attempts
--halt soon,fail=1                 stop new submissions on final failure
--joblog FILE, --joblog=FILE        truncate/write escaped TSV log
--lua FILE, --lua-code SOURCE      program the scheduler
--help                            print usage
--                                end option parsing
```

Pass a command template followed by `::: INPUT...`, `:::: FILE`, or input on
stdin. Repeated input groups concatenate; they do not form a Cartesian product.
`-a` cannot be combined with input markers. Lua mode cannot be combined with a
command/input source. Without Lua, a nonempty command is required. CLI command
tokens are joined with spaces as shell source: pass the complete template in
one argv element when its quoting matters.

```sh
parallel-sh -j 2 -k 'printf "%s\n" {}' ::: 'red rose' blue
parallel-sh -0 -a paths.bin 'wc -c < {}'
parallel-sh --lua examples/parallel.lua
```

Newline input drops empty records and removes CR immediately before LF; a final
unterminated nonempty record is accepted. NUL input preserves empty records.
Each record is limited to 1 MiB; NULs in newline input are rejected. Joblogs have
`Seq`, `Exit`, `Attempts`, `Command` columns. Backslash, tab, CR and LF in commands
are escaped, so each job occupies one record. The C API writes records only;
the CLI writes the header. This is not GNU Parallel's joblog format. Unsupported
options, including `--halt now,fail=1`, fail explicitly. SSH/distributed execution,
GNU replacement expressions, and full GNU CLI compatibility are not provided.

## Status codes and verification

`OK=0`, `INVALID=-1`, `NOMEM=-2`, `SYSTEM=-3`, `UNSUPPORTED=-4`, `CLOSED=-5`,
`EMPTY=-6`, `FAILED=-7`, `FULL=-8`. `FAILED` means at least one final job or Lua
configuration failure; `SYSTEM` means an operating-system or stream failure.
Mune calls return `UNSUPPORTED` in `NO_MUNE` builds. Non-POSIX builds support the
text helpers and dry-run; actual job execution returns `UNSUPPORTED`.

Run `sh tests/run_tests.sh` or `make check-shell`. The runner covers the original
tests, allocation/fork/wait/output failures, child ownership, channel closure,
quoting, CLI behavior, kernel shebangs, and real Mune execution. The focused C
tests and CLI regressions are also registered with CMake/CTest.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-parallel-sh.h`](../include/mulib-parallel-sh.h) |
| Implementation switch | `MULIB_IMPL_PARALLEL_SH` |
| Repository-local includes | [mulib-mune](mulib-mune.md) |
| Complete example | [`examples/parallel.c`](../examples/parallel.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/parallel.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
