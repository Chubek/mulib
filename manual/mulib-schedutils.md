# schedutils — native scheduling and deadline arithmetic

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_SCHEDUTILS`; no dependencies.
See [`examples/modules/schedutils.c`](../examples/modules/schedutils.c).

`current` queries the calling process's native policy and priority with
`sched_getscheduler` and `sched_getparam`; outputs are committed only when
both succeed. `yield` calls `sched_yield`. These return 0 success, -1 invalid,
-2 native error, -3 unsupported. Native policy numbers are not translated and
no privileged scheduler changes are performed. The platform must expose these
POSIX scheduler calls for the Unix backend.

`next(previous, period, now, &next)` is portable checked arithmetic for periodic
work. All values use caller-chosen monotonic ticks. A future previous deadline
is retained; otherwise the first scheduled tick strictly after now is returned,
skipping missed periods without drift. Period zero and uint64 overflow fail
with unchanged output. Return 1 success or 0 failure. The function never reads
a clock, sleeps, allocates, or changes native process scheduling.

Tests: `tests/test_ipc_extensions.c`, including UINT64_MAX and equality cases.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-schedutils.h`](../include/mulib-schedutils.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-schedutils.h
 * @brief Scheduler inspection, cooperative yield and checked periodic
 * deadlines. Define MULIB_IMPL_SCHEDUTILS once.
 */
#ifndef MULIB_SCHEDUTILS_H
#define MULIB_SCHEDUTILS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Inspect the calling process's native scheduling policy/priority.
   * 0 success, -1 invalid, -2 OS error, -3 unsupported. */
  int mulib_schedutils_current (int *policy, int *priority);
  /** @brief Yield to the operating-system scheduler; same result convention.
   */
  int mulib_schedutils_yield (void);
  /** @brief Compute the first periodic deadline strictly after now from
   * previous. A future previous is retained. period must be nonzero; overflow
   * fails with unchanged output. Times are caller-supplied monotonic ticks. 1
   * success. */
  int mulib_schedutils_next (uint64_t previous, uint64_t period, uint64_t now,
                             uint64_t *next);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-schedutils.h`](../include/mulib-schedutils.h) |
| Implementation switch | `MULIB_IMPL_SCHEDUTILS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/schedutils.c`](../examples/modules/schedutils.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/schedutils.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
