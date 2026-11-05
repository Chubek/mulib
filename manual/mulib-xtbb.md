# MuLIB xtbb

**Header:** `include/mulib-xtbb.h` · **Switches:** `MULIB_IMPL_XTBB`,
`MULIB_IMPL_PARALLEL`, `MULIB_IMPL_TASKPOOL` · **Link:** `-pthread` on POSIX.

xtbb provides a C11, OneTBB-inspired task arena and fork/join algorithms using
MuLIB's bounded worker pool. It is **not** OneTBB source/ABI compatible and does
not claim work stealing, concurrent containers, flow graphs, nested task groups,
priorities or the OneTBB scheduler's performance characteristics.

## Arena and parallel algorithms

`mulib_xtbb_create(&arena, workers, capacity)` requires `*out == NULL` and nonzero
workers/pending capacity. `destroy` joins and frees it; NULL is harmless. A single
controller owns lifecycle and synchronous operations. Do not run synchronous
operations from that arena's workers: they return TASK_INVALID before submitting
borrowed work. Same-pool worker destruction is ignored, retaining controller
ownership. No concurrent lifecycle calls. Unsupported hosts return TASK_UNSUPPORTED.

`for(arena, begin, end, grain, body, context, cancel, error)` runs `[begin,end)`.
`map` handles fixed-stride buffers with PARALLEL's overlap rules: disjoint buffers
or exactly identical in-place buffers/strides, each output slot exclusively owned
by its callback. Both expose original callback errors separately from TASK_FAILED.
`invoke(arena, jobs, count, cancel, error)` runs independent `{function,context}`
callbacks and joins them. Failure cooperatively stops jobs not yet started;
already running callbacks finish. Null callbacks are rejected before submission.
Empty ranges/job lists succeed; grain must be nonzero. Contexts/buffers are
borrowed until return, shared state needs caller synchronization.

## Reduction

`reduce(arena, begin, end, grain, identity, size, output, body, join, context,
cancel, error)` creates one private byte-copyable accumulator per grain chunk,
initializes it by copying identity, and folds indices using `body(index, acc, ctx)`.
After all workers join, the controller merges partials into another identity copy
using `join(left, right, ctx)` in ascending chunk order. Finally it copies into
output **only on success**. An empty range copies identity without calling body
or join. `size` must be nonzero; normally pass `sizeof(T)` for the accumulator
type, ensuring every array slot is suitably aligned. No nested resources or
automatic destructors are supported. Identity should be the join operation's
neutral element. Fixed grain gives deterministic merge order, not necessarily
serial-fold equality for floating-point/non-associative operations.

Allocation/range multiplication overflow fail before work. Body errors, join
errors and cancellation preserve output. Callbacks must return normally.
Cancellation is cooperative, not an interrupt. Temporary storage is
`(ceil(range/grain)+1)*size`, plus PARALLEL job metadata; callers should choose
bounded ranges and grain. Return codes are TASK_*; error receives original
nonzero body/join codes.

```sh
cc -std=c11 -Iinclude examples/modules/xtbb.c -pthread -o /tmp/opencode/xtbb
/tmp/opencode/xtbb
```

`tests/test_build_tasks.c` checks reductions, empty ranges, failure preservation
and overflow; PARALLEL/TASKPOOL tests cover queueing, map ownership and cancellation.
