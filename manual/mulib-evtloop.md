# Cooperative event loop

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_EVTLOOP`; no MuLIB dependencies. Zero-initialize and set a
nonzero task limit. Post absolute uint64 due ticks, cancel by id, run with
monotonic time and a callback budget. Due/id ordering is deterministic. Callbacks
may post/cancel but not free the loop. No OS I/O polling or threads.
[Example](../examples/modules/evtloop.c), [contracts](storage-server.md).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-evtloop.h`](../include/mulib-evtloop.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-evtloop.h
 * @brief Cooperative caller-clocked one-shot task/timer queue. No polling I/O.
 * Define MULIB_IMPL_EVTLOOP. Zero initialize; set nonzero limit before use.
 * Not thread-safe. Callbacks may post/cancel, but must not free the loop. */
#ifndef MULIB_EVTLOOP_H
#define MULIB_EVTLOOP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef void (*mulib_evtloop_fn) (void *context);
  typedef struct mulib_evtloop_task
  {
    uint64_t id, due;
    mulib_evtloop_fn run;
    void *context;
  } mulib_evtloop_task_t;
  typedef struct mulib_evtloop
  {
    mulib_evtloop_task_t *tasks;
    size_t count, limit;
    uint64_t serial, now;
    int running;
  } mulib_evtloop_t;
  /** Absolute due time. Return nonzero cancellation id, zero invalid/full/OOM.
   */
  uint64_t mulib_evtloop_post (mulib_evtloop_t *loop, uint64_t due,
                               mulib_evtloop_fn run, void *context);
  int mulib_evtloop_cancel (mulib_evtloop_t *loop, uint64_t id);
  /** Execute at most budget tasks due <= now, in due/id order. Backward time or
   * recursive run returns SIZE_MAX. Tasks are removed before callback. */
  size_t mulib_evtloop_run (mulib_evtloop_t *loop, uint64_t now, size_t budget);
  void mulib_evtloop_free (mulib_evtloop_t *loop);
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
| Header | [`include/mulib-evtloop.h`](../include/mulib-evtloop.h) |
| Implementation switch | `MULIB_IMPL_EVTLOOP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/evtloop.c`](../examples/modules/evtloop.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/evtloop.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
