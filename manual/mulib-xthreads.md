# mulib-xthreads

[Manuals](README.md) · [Library index](index.md)

Portable native threads and the low-level synchronization used by MuLIB's CSP
runtime. Define `MULIB_IMPL_XTHREADS` in exactly one translation unit, then include
`mulib-xthreads.h`. There are no MuLIB dependencies. On POSIX, compile/link with
`-pthread` (the CMake `mulib` target supplies the thread dependency).

## Platforms and API

The POSIX backend uses pthreads; Windows Vista and later use `_beginthreadex`,
SRW locks, condition variables, InitOnce and TLS. Other platforms produce a
compile-time diagnostic. Public native types require the platform headers.
Declarations are usable from C++; implementation is intended for C11.

`mulib_xthread_create(&thread, callback, argument)` starts an `int(void *)`
callback. The argument is borrowed and must outlive its use. On successful
creation, exactly one controller must either join or detach the thread.
`mulib_xthread_join(thread, &result)` waits and retrieves the full integer result;
`result` may be NULL. Join synchronizes callback writes with the joining thread.
Detachment releases the native handle, not the argument. Never join yourself,
join twice, or race join with detach. A failed create produces no joinable handle.

`mulib_xthread_sleep(ms)` sleeps the OS thread; `mulib_xthread_yield()` yields it.
Neither is fiber-aware. Use the corresponding fiber functions inside fibers.

Mutexes are nonrecursive. Initialize with `mulib_xmutex_init` or
`MULIB_XMUTEX_INIT`, then lock/trylock/unlock. Trylock reports `BUSY`. Only the
native-thread owner may unlock. Conditions initialize with `mulib_xcond_init`
or `MULIB_XCOND_INIT`. Wait with the mutex held, always in a predicate loop:

```c
mulib_xmutex_lock(&mutex);
while (!ready)
    mulib_xcond_wait(&condition, &mutex);
/* use protected data */
mulib_xmutex_unlock(&mutex);
```

Wait atomically releases the mutex and returns with it held. Signal wakes at
least one waiter, broadcast all. Spurious wakes are permitted. Destroy only after
all users have finished. Native locks/conditions block an entire scheduler thread;
fiber-aware equivalents live in `mulib-concurrency-prim.h`.

Initialize once flags with `MULIB_XONCE_INIT`. `mulib_xcall_once` publishes callback
effects to every successful caller. The callback must return normally and must
not recursively use the same flag. TLS keys store borrowed `void *` values,
initially NULL per native thread. They have no automatic destructor; free values
yourself before deleting the key. TLS is native-thread-local, not fiber-local.

## Time, failures and ownership

`mulib_xclock_ms()` reports UTC milliseconds, and `mulib_xdeadline(relative_ms)`
adds a timeout with saturation. `MULIB_XWAIT_FOREVER` (`UINT64_MAX`) is the
infinite-deadline sentinel. `mulib_xcond_wait_until` accepts an **absolute**
deadline. UTC clock changes can shorten or extend waits. On 32-bit `time_t`
systems, deadlines past the representable range are clamped. Very long Windows
waits may return spuriously before the deadline; predicate loops remain required.

Results are `OK` (0), `INVALID`, `NOMEM`, `SYSTEM`, `BUSY` or `TIMEOUT`, prefixed
`MULIB_XTHREAD_`. Native objects must be initialized successfully before use and
must not be copied, moved while in use, or concurrently destroyed. Native API
misuse (invalid handles, wrong-owner unlock, recursive acquisition) is outside
the contract. Thread cancellation/forced termination is unsupported.

See [`examples/modules/xthreads.c`](../examples/modules/xthreads.c).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-xthreads.h`](../include/mulib-xthreads.h) |
| Implementation switch | `MULIB_IMPL_XTHREADS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/xthreads.c`](../examples/modules/xthreads.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/xthreads.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
