# MuLIB PolyTask

[Manuals](README.md) · [Library index](index.md)

**Header:** `include/mulib-polytask.h` · **Switch:** `MULIB_IMPL_POLYTASK`

PolyTask combines native TASK plans, an owned JSON plan reader and a persistent
bounded TASKPOOL. There are no external executors or mandatory JSON libraries.
Create a pool with nonzero worker count and pending capacity; `*out` must be NULL.
The pool must have one controlling caller, never another synchronous operation
from its own workers. Destroy joins remaining work; same-pool worker destruction
is ignored so ownership remains with the controller. Threadless platforms return
TASK_UNSUPPORTED. Passing NULL pool to batch/schedule executes serially.

## Batches and ownership

`mulib_polytask_run(pool, tasks, count, cancel, results)` validates task shapes
and logical declarations before launching anything. The task array and all its
borrowed fields must remain immutable and alive until it returns. Each result
must be zero initialized (or destroyed), and holds the EXEC API status and an
owned `execution` result. Results are always in input order; independent failures
do not stop other tasks. Return is TASK_OK, TASK_FAILED if any task API/exit status
fails, or a pool/submission error. Inspect individual statuses for details.
Already completed results survive a submission failure. Destroy each result using
`mulib_polytask_result_destroy`, including after errors.

Cancellation is checked immediately before each launch; unstarted tasks receive
TASK_CANCELLED and no child result. Running children continue under their own
EXEC timeout. Batches with cancelled tasks return TASK_FAILED. Queue-full
backpressure joins a submitted group before retrying; it never drops tasks, but
this is bounded fork/join scheduling, not a work-stealing scheduler. An empty
batch succeeds without task storage. All submitted callbacks join before return.

## JSON grammar

Parse with `mulib_polytask_parse(text, length, &plan, &error)`. Parsing is inert;
`mulib_polytask_tasks(plan, &count)` borrows specifications for the batch API.
Destroy the plan only after the batch finishes. The plan owns its JSON tree and
pointer arrays. Maximum 65536 tasks, with unique nonempty names:

```json
[
  {"name":"compile", "argv":["cc","-c","a.c"],
   "env":["PATH=/usr/bin:/bin"], "inputs":["a.c"], "outputs":["a.o"]},
  {"name":"script", "file":"./generate-script"},
  {"name":"shell", "shell":"printf '%s' hello", "timeout_ms":1000}
]
```

Exactly one of `argv` (nonempty string array), `file` (nonempty string), or
`shell` (string, possibly empty) is required. Optional fields: `env`, `inputs`,
`outputs` are string arrays; `cwd` is a string; `timeout_ms` and `max_output`
are integers 0..UINT_MAX. Unknown fields, wrong types, NUL in OS strings,
invalid environment assignments and invalid logical paths are rejected. This
focused schema has no FILE arguments, redirects, pipelines, dependencies,
stdin data or arbitrary options; use the C task API for those. JSON duplicate
members follow the existing JSON model's replacement semantics. An empty root
array is valid; any other root type fails. Task names identify batch results,
not implicit filenames. Parsing returns EXEC_INVALID or EXEC_MEMORY (underlying
JSON parse errors, including parser OOM, report INVALID with JSON diagnostic).
Schema errors provide a static diagnostic message, not a field source location.

No path staging, host sandboxing or graph-output harvesting occurs. SHELL text is
trusted executable code. See TASK for defaults and security boundaries.

## BuildGraph adapter

`mulib_polytask_schedule(pool, count, callback, context)` synchronously runs each
index once and joins before returning. It matches `mulib_buildgraph_schedule_fn`:

```c
mulib_buildgraph_build(graph, targets, count,
                      mulib_polytask_schedule, pool, &report);
```

Callbacks need not be processes: this allows hermetic BuildVM programs to share
the pool. A callback's capabilities determine hermeticity, not the dispatcher.

## Switches and verification

In addition to POLYTASK, define TASKPOOL, TASK, JSON, UTF8, PARSEC, STRBUF, EXEC,
PROCESS, PERF, MUNE, DYNLOAD, MIR, VMWEAVE, EGRAPH, REWRITE, DYNCALL, JIT, PEG,
AST and GLOB (each `MULIB_IMPL_` prefixed) in the implementation TU.
The runnable example's `examples/tasks-common.h` explicitly lists them.

```sh
cc -std=c11 -Iinclude examples/modules/polytask.c -pthread -lm -ldl -o /tmp/opencode/polytask
/tmp/opencode/polytask
```

The focused `tests/test_build_tasks.c` exercises JSON rejection/empty plans,
concurrent batches, stable results, failures, cancellation and graph dispatch.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-polytask.h`](../include/mulib-polytask.h) |
| Implementation switch | `MULIB_IMPL_POLYTASK` |
| Repository-local includes | [mulib-json](mulib-json.md), [mulib-task](mulib-task.md), [mulib-taskpool](mulib-taskpool.md) |
| Complete example | [`examples/modules/polytask.c`](../examples/modules/polytask.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/polytask.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
