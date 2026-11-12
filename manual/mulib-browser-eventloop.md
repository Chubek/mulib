# browser-eventloop

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Deterministic bounded FIFO tasks and run-to-completion microtask checkpoints.

## Contract and supported profile

A zero-initialized loop owns separate FIFO task and microtask queues and
borrows their callback contexts. Step first drains any pending microtasks;
otherwise it runs at most one task and then a microtask checkpoint. Checkpoint
budget is max_tasks_per_turn (default 10000), despite the historical field name;
remaining microtasks defer the next task. Step returns callbacks run, zero for
no work/reentrancy. Posts may occur inside callbacks; nested step is rejected.
Free releases both queues outside callbacks. There are no timers, blocking I/O,
thread synchronization or browser rendering phases.

## Integration and ownership

Include `include/mulib-browser-eventloop.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_BROWSER_EVENTLOOP`, `MULIB_IMPL_MICROJS_PROMISE`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/browser-eventloop.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-browser-eventloop.h`](../include/mulib-browser-eventloop.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-browser-eventloop.h
 * @brief Deterministic bounded FIFO tasks and run-to-completion microtask
 * checkpoints. C11 single-header module. Define MULIB_IMPL_BROWSER_EVENTLOOP
 * in exactly one C translation unit.
 *
 */
#ifndef MULIB_BROWSER_EVENTLOOP_H
#define MULIB_BROWSER_EVENTLOOP_H
#include "mulib-microjs-promise.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_eventloop
  {
    mulib_microjs_microtasks_t tasks, microtasks;
    size_t max_tasks_per_turn;
  } mulib_browser_eventloop_t;
  /** @brief See manual/mulib-browser-eventloop.md for this operation's
   * contract. */
  int mulib_browser_eventloop_post (mulib_browser_eventloop_t *e,
                                    mulib_microjs_job_fn cb, void *ctx,
                                    int microtask);
  /** @brief See manual/mulib-browser-eventloop.md for this operation's
   * contract. */
  size_t mulib_browser_eventloop_step (mulib_browser_eventloop_t *e);
  /** @brief See manual/mulib-browser-eventloop.md for this operation's
   * contract. */
  void mulib_browser_eventloop_free (mulib_browser_eventloop_t *e);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_EVENTLOOP_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browser-eventloop.h`](../include/mulib-browser-eventloop.h) |
| Implementation switch | `MULIB_IMPL_BROWSER_EVENTLOOP` |
| Repository-local includes | [mulib-microjs-promise](mulib-microjs-promise.md) |
| Complete example | [`examples/modules/browser-eventloop.c`](../examples/modules/browser-eventloop.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browser-eventloop.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
