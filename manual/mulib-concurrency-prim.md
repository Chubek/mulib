# mulib-concurrency-prim

[Manuals](README.md) · [Library index](index.md)

Synchronization shared by native threads and stackful fibers. Define
`MULIB_IMPL_CONCURRENCY_PRIM`, `MULIB_IMPL_FIBER` and `MULIB_IMPL_XTHREADS` exactly
once each. Dependencies are included normally; switches are application-owned.
POSIX links with `-pthread`; see [fiber](mulib-fiber.md) for platform requirements.

## Primitives

| Object | Operations | Semantics |
|---|---|---|
| `mulib_mutex_t` | create, lock, unlock, destroy | Nonrecursive exclusive lock |
| `mulib_semaphore_t` | create, wait, post, destroy | Bounded counting permits |
| `mulib_waitgroup_t` | create, add, done, wait, destroy | Wait for a work counter to reach zero |
| `mulib_sync_event_t` | create, set, reset, wait, destroy | Level-triggered manual-reset event |
| `mulib_rwlock_t` | create, read_lock/unlock, write_lock/unlock, destroy | Writer-preferring shared/exclusive lock |

All wait/lock functions take a relative millisecond timeout: 0 is nonblocking,
`MULIB_XWAIT_FOREVER` is infinite. They always test availability before timeout,
so a ready object succeeds even with timeout 0. A fiber parks without blocking
its native thread; other callers sleep on a condition variable. Notifications
may wake several contenders; each rechecks the predicate under an internal lock.
There are no busy-wait loops and no strict FIFO/fairness promise.

### Mutexes and semaphores

The acquiring execution context must unlock its mutex. Ownership is a contract,
not tracked dynamically. Do not recursively acquire, unlock from another fiber,
or leave a mutex held when a callback exits. Fiber-aware locks may be held across
cooperative yields; native `mulib_xmutex_t` locks must not be held across yields
if another fiber might acquire them.

A semaphore starts at `initial` with a fixed, nonzero `maximum`. Wait consumes
one permit. Post adds the requested number atomically, returning OVERFLOW without
changing state if it would exceed maximum. Posting zero is harmless. A semaphore
has no owner; one thread/fiber can post permits consumed by another.

### Wait groups

Groups start at zero. Add work **before** allowing another execution context to
wait on a new generation; call done once for each unit of completed work.
Done at zero returns INVALID; addition overflow leaves state unchanged. Reuse
only after all previous-generation waits have returned. Waiting from work that
is itself counted in that group can deadlock.

```c
mulib_waitgroup_add(group, 2);
/* spawn two callbacks, each eventually calls mulib_waitgroup_done(group) */
/* In a fiber or a different native thread: */
mulib_waitgroup_wait(group, MULIB_XWAIT_FOREVER);
```

Calling wait from the scheduler's controller before running its fibers blocks the
native thread. Run the scheduler, or put the waiting controller in a fiber.

### Events and reader/writer locks

Setting an event wakes waiters and leaves it signaled for future waits. Wait does
not consume the signal. Reset clears the level; a concurrent reset may cause
already-notified waiters to wait again. This is not a counted notification queue.

Reader/writer locks admit multiple readers or one writer. Queued writers block
new readers, reducing writer starvation. No recursive acquisition, upgrade or
downgrade is supported. Release each acquisition with the matching unlock in the
same execution context; ownership is not tracked per reader. Read-count and
waiting-writer-count overflow are checked.

## Lifetime, memory and errors

Each create clears its output and allocates one owned object (mutex also owns a
semaphore). Initialize before publication. Objects are opaque and cannot be
copied. Destroy only with **no concurrent callers**, including calls attempting
entry. Destroy returns BUSY for registered waiters; mutex/rwlock also reject held
locks and waitgroup rejects outstanding work. NULL destruction succeeds. No
operation implicitly cancels waiters or frees application data.

Results use `MULIB_CONCURRENCY_OK/INVALID/NOMEM/SYSTEM/BUSY/TIMEOUT/OVERFLOW`.
Waiting uses stack-local registrations, without heap allocation. Timeouts use
UTC deadlines from xthreads, so wall-clock changes affect them. Primitive state
and notifications are serialized by short-lived native mutexes; they are never
held while context-switching. Native thread cancellation, signal-handler calls,
and forced fiber termination are unsupported. Native conditions, once flags and
TLS keys are additionally available in `mulib-xthreads.h`.

See [`examples/modules/concurrency-prim.c`](../examples/modules/concurrency-prim.c).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-concurrency-prim.h`](../include/mulib-concurrency-prim.h) |
| Implementation switch | `MULIB_IMPL_CONCURRENCY_PRIM` |
| Repository-local includes | [mulib-fiber](mulib-fiber.md) |
| Complete example | [`examples/modules/concurrency-prim.c`](../examples/modules/concurrency-prim.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/concurrency-prim.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
