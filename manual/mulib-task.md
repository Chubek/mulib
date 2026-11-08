# MuLIB task

[Manuals](README.md) · [Library index](index.md)

**Header:** `include/mulib-task.h` · **Switch:** `MULIB_IMPL_TASK`

An inert `mulib_task_t` describes one native process boundary. Initialize with
`mulib_task_init`, set a nonempty name, and select:

* COMMAND: `command.argv/argc` are literal arguments, with optional file override.
* FILE: `source` is an executable file, including executable shebang scripts.
  Optional `command.argv/argc` includes argv[0]; absent argv defaults to source.
* SHELL: `source` is trusted `/bin/sh -c` code; command file/argv are ignored.

The plan borrows all strings, arrays, input bytes and descriptors through task
completion. Results own captures and stage statuses; destroy with
`mulib_exec_result_destroy` even after errors. `mulib_task_run` returns an EXEC
API status, separate from the child exit status in `result.code`. Initialize the
result to zero before each use. No automatic retries or dependency handling occur.

## Boundaries and defaults

Initialization supplies an empty replacement environment, EOF stdin, captured
stdout/stderr, the EXEC default 16 MiB per-stream output cap, and a 60000 ms
deadline. Override `command` and `options` explicitly when needed. In particular,
setting `command.env = NULL` opts back into inheritance; an explicit array
replaces the environment. `timeout_ms = 0` disables the deadline.

`inputs/input_count` and `outputs/output_count` describe unique canonical relative
logical paths, each up to 4095 bytes. Empty components, traversal, absolute paths,
backslashes, colons and control bytes fail validation. An input cannot also be an
output. Each list is limited to 65536 entries. Validation checks plan shape before
execution; EXEC additionally validates detailed command/OS descriptor constraints.

These declarations are a **contract, not an OS sandbox**: a native process still
sees host filesystem/network capabilities, cwd and caller-managed descriptors.
Paths are not automatically staged, checked for existence or published as graph
outputs. Deadlines kill/reap direct children only; applications must manage
descendants. Timeout observation may be delayed by blocking OS launch/open calls.
Cancellation of pending batches belongs to PolyTask. For enforced in-memory
hermetic transformations use BuildVM, not native TASK.

## Implementation and example

Define TASK, EXEC, PROCESS, PERF, MUNE, DYNLOAD, MIR, VMWEAVE, EGRAPH, REWRITE,
DYNCALL, JIT, PARSEC, PEG, AST, STRBUF and GLOB implementation switches in exactly
one C11 TU (all names prefixed `MULIB_IMPL_`). The explicit placement helper
`examples/tasks-common.h` includes these; it is not a public implicit-switch API.
No dependency switches are silently enabled by this header. See also EXEC's
manual. POSIX hosts need `-pthread -lm -ldl` on Linux.

```sh
cc -std=c11 -Iinclude examples/modules/task.c -pthread -lm -ldl -o /tmp/opencode/task
/tmp/opencode/task
```

`tests/test_build_tasks.c` checks default environment, shebang execution, missing
executables, capture, deadlines and output limits. Native execution is POSIX-only
and returns EXEC_UNSUPPORTED elsewhere.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-task.h`](../include/mulib-task.h) |
| Implementation switch | `MULIB_IMPL_TASK` |
| Repository-local includes | [mulib-exec](mulib-exec.md) |
| Complete example | [`examples/modules/task.c`](../examples/modules/task.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/task.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
