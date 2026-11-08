# mulib-containterpool

[Manuals](README.md) · [Library index](index.md)

## NAME AND INTEGRATION

Stable owned container slots, generation handles and fair cooperative polling.
The requested `containterpool` spelling is retained. Define
`MULIB_IMPL_CONTAINTERPOOL` and `image2container`/`containerboost` closures.
Example: `examples/modules/containterpool.c`.

## INSERTION AND LOOKUP

`init(pool,capacity)` allocates a fixed slot-pointer table. `add` materializes an
image, checks duplicate identity, selects an explicit backend and prepares its
runtime before publishing a stable heap entry. Entry owns image metadata,
rootfs and runtime. An optional ID override and import limits are accepted.
Preparation errors release partial image state and leave the pool unchanged.

Handles contain slot and never-reused insertion generation. `find(id,&handle)`
resolves an exact identity. `acquire(handle,&entry)` pins a stable entry and
returns its pointer; `release` drops exactly one reference. This prevents pool
removal while borrowed, but does not synchronize concurrent record/runtime access.
Acquire/release return EBUSY while a pool operation invokes a backend callback.
This also prevents a destructor from acquiring a reference after removal preflight.

## SCHEDULING

`tick(pool,max_visits,per_container_budget,now,report)` visits at most capacity
slots once per call, beginning at the saved cursor. RUNNING/PAUSED entries are
polled; inactive/empty slots still count as visited. Cursor advances across empty
slots and backend failures so one failure does not starve later peers.

Report contains visited, polled, exited, failed and first_error. The return value
is first_error or success. Work budget must be positive, while max_visits can be
zero. Exit and deadline transitions follow runtime. Polling uses no hidden threads
or process scheduling; applications invoke tick from their own loop.

## REMOVAL AND RELEASE

`remove(handle,now)` requires an unborrowed inactive entry, closes runtime, then
frees image/rootfs/entry. Backend close errors retain entry ownership. `free`
preflights running/paused/borrowed entries, then progressively removes inactive
ones. If a backend close fails, already-closed entries remain removed and the
remaining table is live for retry; only complete success releases the table.

Generation exhaustion is ELIMIT; stale handles ESTALE; active/borrowed/reentrant
mutation EBUSY. All operations use CT codes. External exports borrowing rootfs
must be removed before raw pool removal; `containerd` coordinates this ordering.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-containterpool.h`](../include/mulib-containterpool.h) |
| Implementation switch | `MULIB_IMPL_CONTAINTERPOOL` |
| Repository-local includes | [mulib-containerboost](mulib-containerboost.md), [mulib-image2container](mulib-image2container.md) |
| Complete example | [`examples/modules/containterpool.c`](../examples/modules/containterpool.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/containterpool.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
