# mulib-fiber

Stackful cooperative fibers: callbacks can suspend from nested functions without
manually storing continuation state. Define `MULIB_IMPL_FIBER` and
`MULIB_IMPL_XTHREADS` in exactly one implementation translation unit each; define
all desired switches before including the headers. Modules never enable their
dependencies' implementation switches themselves.

## Scheduling

1. Create a scheduler with `mulib_fiber_scheduler_create(&scheduler)`.
2. Spawn callbacks with `mulib_fiber_spawn(scheduler, fn, arg, stack_size, &fiber)`.
3. Call `mulib_fiber_run(scheduler)` to run until all callbacks return.
4. Destroy the scheduler after all external users of its handles finish.

`out` for spawn is optional. Each callback is `void(void *)` and borrows its
argument. Create/spawn/run/destroy belong to **one native thread** throughout the
scheduler's lifetime. A callback may spawn more work into its own scheduler.
Nested `run` calls are rejected. Multiple native threads may run distinct
schedulers, communicating through channels and synchronization primitives.

Scheduling rotates among ready fibers. Yield with `mulib_fiber_yield()`;
`mulib_fiber_sleep(ms)` suspends just the current fiber. With no ready fibers,
the scheduler sleeps on a native condition until a wakeup or earliest timer.
No polling thread is created. Outside a callback, yield/sleep use native threads.
`mulib_fiber_current()` returns NULL outside callbacks.

This is cooperative N:1 scheduling per scheduler, not Go's preemptive M:N
runtime: there is no work stealing, stack growth, implicit I/O interception or
forced cancellation. CPU loops must yield explicitly. Blocking system calls or
native mutexes block that scheduler's thread. Use `mulib-channel.h` and
`mulib-concurrency-prim.h` to park fibers on synchronization instead.

## Stacks and portability

POSIX uses `getcontext/makecontext/swapcontext` from `<ucontext.h>`; the host libc
must provide these APIs (including Linux/glibc and systems with compatible
ucontext implementations). Windows uses native Fibers and temporarily converts
the scheduler thread if necessary. Unsupported hosts fail compilation rather
than silently executing callbacks serially. POSIX links with `-pthread`.

Stack size 0 selects 256 KiB; nonzero sizes below 16 KiB are rejected. POSIX
stacks are heap allocations, fixed size, without guard pages. Choose enough
space for callback depth and local arrays. Exhausting a stack is undefined.
POSIX ASan builds use sanitizer fiber-switch hooks, including fake-stack support
for stack-use-after-return checking. ASan may still print its standard warning
about limited `makecontext/swapcontext` support. ThreadSanitizer is suitable for
the native-thread-only test path; fiber context annotations for TSan are absent.
Finished stacks are released during `run`; handle records remain until scheduler
destruction. There is no per-fiber join API: `run` joins the scheduler's work,
while wait groups can join a subset. Thread-local variables are shared by fibers
on the same native thread. Callbacks must return normally, without longjmp or
exceptions crossing context boundaries.

## Parking and wakeups

`mulib_fiber_park_until(absolute_utc_ms)` consumes a one-bit wake permit or parks
until a permit/deadline. `mulib_fiber_unpark(handle)` is thread-safe and sets the
permit even before the target parks, preventing lost wakeups. Repeated wakes
coalesce. Unparking a finished, still-live handle is harmless.

Wait adapters must register their notification source under a lock, release the
lock, park, then reacquire it and test the predicate. Unregister before allowing
the wait record to expire. `mulib_fiber_clear_wake()` clears an unused permit;
it is for adapters after unregistration, not a general cancellation mechanism.
Do not externally unpark fibers to cancel higher-level operations; those
operations resume only when their predicate is true or timeout expires.

Deadlines use the UTC clock from xthreads, so clock adjustments affect timers.
`UINT64_MAX` means forever. If every fiber waits forever and nothing external can
wake it, `run` waits forever as well; deadlock detection is not provided.

## Errors and ownership

Results use `MULIB_FIBER_OK/INVALID/NOMEM/SYSTEM/BUSY/TIMEOUT`. Create clears its
output; spawn failure schedules nothing and clears its optional handle output.
Destroy returns BUSY while any callback remains unfinished, retaining ownership.
Do not destroy concurrently with unpark, or retain handles after destruction.
Valid context-switch failure is fatal (`abort`), since returning normally would
violate the scheduler's stack/state invariants.

See [`examples/modules/fiber.c`](../examples/modules/fiber.c).
