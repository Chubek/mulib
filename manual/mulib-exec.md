# exec — command execution and Mune workflows

`mulib-exec.h` provides POSIX fork/exec and `posix_spawn` execution, asynchronous
child ownership, synchronous communication, concurrent pipelines, file
redirection, binary capture, deadlines and an embedded Lua/Mune execution DSL.
It never implicitly expands arguments through a shell. To invoke a shell
deliberately, use `run_text` or an explicit `{"sh", "-c", script}` argv.

## Task execution helpers

`mulib_exec_run_file(path, settings, options, result)` executes a file directly,
using the kernel's shebang support for executable scripts. No ENOEXEC shell
fallback is attempted. Optional settings supply cwd, environment, redirects and
arguments (including argv[0]); otherwise argv is just the path. Relative paths
and PATH lookup follow the existing command rules.

`mulib_exec_run_text(text, settings, options, result)` explicitly invokes
`/bin/sh -c text`; settings' file/argv are ignored, other settings are retained.
An empty string is a valid no-op shell program. This is intentional code
execution, never an escaping API for untrusted text. Both helpers retain EXEC's
binary capture, deadline, output limit and result-ownership behavior, and report
UNSUPPORTED on non-POSIX backends. Defaults still inherit environment and stdio;
the higher-level TASK initializer deliberately chooses stricter defaults instead.

POSIX EXEC now links with `-pthread`. An internal launch mutex serializes EXEC's
descriptor creation and child launches, including portable pipe/fcntl and
temporary-input setup. Children execute, capture and wait concurrently after
launch. This prevents one EXEC task from inheriting another's not-yet-CLOEXEC
descriptors during construction. It does not coordinate unrelated process-launch
APIs or application descriptor mutations. Blocking launch/open calls can delay
other launches and deadline observation. Do not invoke EXEC from signal handlers
or a post-fork child of a multithreaded application before exec.

## Build and implementation placement

Define these switches in one C11 implementation translation unit before including
`mulib-exec.h` (or distribute their implementations across translation units):

```c
#define MULIB_IMPL_EXEC
#define MULIB_IMPL_PROCESS
#define MULIB_IMPL_PERF
#define MULIB_IMPL_MUNE
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GLOB
#include "mulib-exec.h"
```

Link with math, threads and the platform dynamic loader (`-lm -pthread -ldl` on
Linux). Dependencies are explicit; the header never enables their implementation
switches itself. [`examples/modules/exec.c`](../examples/modules/exec.c) uses the
shared example implementation-switch list in `examples/exec-common.h`.

## Command specifications

Initialize `mulib_exec_command_t` with `mulib_exec_command_init`, **not just `{0}`**:
inherited stdio descriptors are `-1`. All strings and arrays are borrowed through
`start` or the complete `run`/`pipeline` call.

| Field | Contract |
| --- | --- |
| `argv`, `argc` | Counted array including argv[0], 1–65536 non-NULL strings; no terminator required. Empty arguments are preserved. |
| `file` | Executable override; NULL uses argv[0]. The executable name must not be empty. |
| `env`, `envc` | NULL inherits `environ`; otherwise 0–65536 `NAME=value` strings replace the environment. A non-NULL empty array gives an empty environment. |
| `cwd` | Child-only working directory, or NULL to inherit. |
| `backend` | `AUTO`, `FORK`, or `SPAWN`. AUTO chooses fork with cwd, spawn otherwise. Explicit SPAWN with cwd returns UNSUPPORTED. |
| `io` | Borrowed stdin/stdout/stderr source descriptors: -1 inherits; >=3 redirects; 0/1/2 is accepted only for its own target. Parent retains ownership. |
| `stdin_path` | Open for reading in the child's directory; overrides stdin descriptor. |
| `stdout_path`, `stderr_path` | Open/create for writing, mode 0666 subject to inherited umask; truncate unless the corresponding append flag is set. Override descriptors. |
| `append_stdout`, `append_stderr` | Select append rather than truncate for file paths. |
| `merge_stderr` | Duplicate final stdout onto stderr after other redirections. Cannot combine with stderr_path. |
| `doc` | Optional descriptive string, never executed. |

Bare executable names search PATH from the **selected child environment**, using
`/bin:/usr/bin` when PATH is absent. Empty PATH components mean the child's cwd.
Names containing `/` are executed directly, relative to child cwd if relative.
Both backends try the same candidates and preserve an EACCES result when all
candidates fail with search errors. Executable text files without a recognized
executable format are **not** retried through `/bin/sh`; use a shebang or invoke
the intended interpreter explicitly. Neither backend changes the parent's cwd
or environment. Arguments receive no glob expansion, substitution or quote removal.

Fork preparation allocates argv/environment/PATH storage before forking. The
child uses async-signal-safe setup/exec/error-reporting calls and `_exit`, with
a close-on-exec error pipe distinguishing setup/exec failures from a program
that genuinely exits 127. Spawn uses `posix_spawn` with prepared PATH candidates
and file actions, giving the same explicit environment search semantics.

## Asynchronous fork/spawn/wait loop

```c
mulib_exec_command_t command;
mulib_process_t child = {0};
mulib_process_status_t status;
const char *args[] = {"git", "status", "--porcelain=v1"};
mulib_exec_command_init(&command);
command.argv = args;
command.argc = 3;
command.backend = MULIB_EXEC_FORK;
if (mulib_exec_start(&command, &child) == MULIB_EXEC_OK) {
    /* process_wait(...,1) polls; process_wait(...,0) retries EINTR and waits. */
    int rc = mulib_process_wait(&child, &status, 0);
    /* Check rc, then status.exited / exit_code / term_signal. */
    (void)rc;
}
```

The child handle must be initialized and unused before starting. Successful
start transfers ownership of one waitable direct child to the caller. A reap
clears the handle. `mulib_exec_signal` polls first to avoid signalling an already
exited/reaped handle; it returns `MULIB_PROCESS_RUNNING` after sending a signal,
or OK after reaping an already completed child. It does not wait for a signalled
child to stop. `mulib_process_terminate` sends SIGKILL and reaps synchronously.
Applications can create their own pipes with `mulib_process_pipe` and integrate
the descriptors and nonblocking waits into an event loop.

Do not install a competing SIGCHLD reaper or have another thread wait for the
same child. Process groups, sessions, terminal foreground ownership, resource
limits, privilege changes, arbitrary fd remapping and process-tree supervision
are application-level policy; this API signals direct children only. Signal
masks/dispositions and other process attributes follow normal POSIX inheritance.
Unrelated descriptors must be marked close-on-exec by their owners. On systems
without an atomic CLOEXEC pipe operation, the fallback pipe/fcntl sequence needs
external synchronization against unrelated concurrent execs.

## Run, pipelines and binary communication

`mulib_exec_run` is a one-stage `mulib_exec_pipeline`. A pipeline accepts 1–256
command specifications and launches **every stage before waiting**, connecting
each stdout to the next stdin. All internal endpoints are close-on-exec and the
parent promptly closes its copies of stage endpoints.

Zero-initialize `mulib_exec_options_t` and `mulib_exec_result_t`. Destroy the
result with `mulib_exec_result_destroy` before reusing it; destruction is NULL-safe
and resets fields. A result owns its capture buffers and stage-status array.

| Run option | Meaning |
| --- | --- |
| `input`, `input_size` | Non-NULL pointer supplies exact bytes to stage 1 using a temporary regular file. Non-NULL with size 0 supplies EOF; NULL inherits input. |
| `capture_stdout` | Capture final-stage stdout. |
| `capture_stderr` | Capture aggregate stderr from all stages; ordering across writers is not specified. |
| `max_output` | Maximum bytes **per captured stream**, default 16 MiB; SIZE_MAX is invalid. Exceeding the limit aborts/reaps direct children and retains previously captured bytes. |
| `timeout_ms` | Monotonic launch/drain/wait deadline; zero disables. |
| `pipefail` | Select rightmost nonzero stage status rather than the final stage's status. |

Input staging avoids a parent write deadlock and SIGPIPE disposition changes.
It deliberately provides seekable file input, not streaming callbacks, a pipe or
a pseudo-terminal. Use the asynchronous API with caller-owned descriptors when
interactive duplex streaming or tty semantics are needed. The temporary input
file requires sufficient temporary storage; it is removed on close.

Captured streams are drained concurrently through `poll`, with bounded reads per
iteration so neither stream starves deadline/reaping checks. Captures are
NUL-terminated for convenience, but `out_size`/`err_size` are authoritative and
embedded NUL bytes are preserved. stderr merging routes it through stdout,
including into a following pipeline stage. Internal stdin/stdout redirects are
rejected, as are explicit redirects conflicting with selected capture or supplied
input. This prevents silently disconnected pipelines.

Deadline observation occurs between blocking OS operations. A blocked `fork`,
`posix_spawn`, filesystem open (for example a FIFO), input staging or kernel wait
can delay observation; this is not a hard real-time deadline. Captures normally
drain until EOF even after direct children exit; descendants retaining endpoints
can delay EOF, so use a timeout when their behavior is unknown. On error the API
closes owned pipes, signals all remaining direct children, and reaps them. It does
not kill descendants or roll back filesystem/repository changes.

## Results and errors

API returns are `OK=0`, `INVALID=-1`, `MEMORY=-2`, `SYSTEM=-3`,
`UNSUPPORTED=-4`, `TIMEOUT=-5`, `LIMIT=-6`. Nonzero **program exit codes are not
API failures**. On SYSTEM errors, errno and `result.system_error` describe the
failure. Other errors do not assign a meaningful errno. Validation errors leave
the supplied result untouched; errors after setup begins retain partial results.

`result.stages` has `count` status slots. Each reports `exited`, `exit_code`, and
`term_signal`; unlaunched stages have zero fields. `failed_stage` is the zero-based
index of a failing launch or SIZE_MAX when no particular launch failed. `code`
is the selected exit status, mapping signal termination to 128+signal. Always
check the API return first: code alone does not describe launch or capture failure.

On non-POSIX targets, or with `MULIB_EXEC_NO_POSIX`/`MULIB_PROCESS_NO_POSIX`, native
execution returns UNSUPPORTED. Specifications and the Mune DSL still compile.
Monotonic timeout support follows `mulib-perf.h`; unavailable clock support
produces a SYSTEM error when a timeout is requested.

## Mune execution DSL

Create a Mune state and call `mulib_exec_bind(state, &error)`. This installs the
global `exec` table and its native runner. Plans are ordinary mutable Lua tables;
their argument and environment arrays must be dense string arrays. Keep all
plans rooted in Lua globals/results while calling the runtime from C. Binding
retains no borrowed application context. Rebinding replaces the global DSL.

```lua
local produce = exec.command {
  argv = {"printf", "%s", "one; two $HOME\n"},
  doc = [[Emit this exact string, including shell punctuation, on stdout.
No shell interprets these bytes. The command does not modify any files.]]
}
local uppercase = exec.command {
  argv = {"tr", "a-z", "A-Z"},
  doc = "Uppercase the incoming byte stream using tr's range semantics."
}
local result = exec.capture(exec.pipeline(produce, uppercase), {
  timeout_ms = 5000,
  max_output = 1024 * 1024,
  pipefail = true
})
assert(result.ok, result.stderr)
assert(result.stdout == "ONE; TWO $HOME\n")
```

`exec.command{...}` retains an optional quoted or multiline long-bracket `doc`.
Command fields mirror the C specification, with `stdin`/`stdout`/`stderr` for
paths and backend strings `"auto"`, `"fork"`, `"spawn"`. C descriptor fields are
not exposed in the Lua table interface. Arguments, paths and environments cannot
contain NUL; binary `input` and captured results can.

* `exec.pipeline(command, ...)` constructs an inert stage array.
* `exec.run(command_or_pipeline, options)` returns a result without throwing on
  ordinary child failure. Invalid Lua field types raise a Mune argument error.
* `exec.capture(plan, options)` copies options and enables both captures.
* `exec.check(plan, options)` raises a Lua error unless result.ok is true.
* `exec.sequence(plans, options, keep_going)` returns results in order, stopping
  on failure unless keep_going is true. Lua if/loops/functions supply conditional
  execution, retries and workflow composition.

Lua results contain `status`, `code`, `ok`, binary `stdout`/`stderr`,
`system_error`, `failed_stage` (one-based, or 0 when absent), and `stages` with
`exited`, `exit_code`, `signal`. Each DSL helper has a detailed runtime string in
`exec.docs`. Executing Mune source has ordinary Mune host capabilities; this is
an automation DSL, not a restricted configuration parser.

Tests: `tests/test_exec.c` exercises both backends, concurrent large captures,
binary input through multiple stages, literal arguments, stderr merging,
append redirection, environment replacement, cwd, missing executables, pipefail,
partial-launch cleanup, timeout reaping, asynchronous signal/wait and TREE/VM DSL
execution. Git workflows are covered separately in `tests/test_git_exec.c`.
`tests/test_vcs_netfs_alloc.c` injects failure at every native pipeline allocation
for both backends and checks that allocations and direct children are released.
