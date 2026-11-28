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
Free releases both queues outside callbacks and leaves both queues unchanged
when called from a loop callback. There are no timers, blocking I/O, thread
synchronization or browser rendering phases.

### Trusted host: mixed checkpoints

`mulib_browser_eventloop_post_microjs` appends a same-VM function to the same
FIFO as native microtasks. Set a nonzero `loop.microtasks.limit` first; that
pending-entry cap applies to all producers. On success the loop owns a root:
the caller may release its handle immediately. The function runs with no
arguments and undefined `this`; its result is discarded. Each attempt
including a failed call consumes one checkpoint unit. The optional diagnostic
callback context is borrowed until dispatch or cancellation, and the error
pointer is valid only during the callback. Failure does not discard or
reorder subsequent work. The diagnostic callback may post more work at the
tail.
Destroying the VM first prevents pending jobs from executing; they report
an invalid call if stepped, or are silently discarded when the loop is freed.

For C callbacks with owned state, `mulib_browser_eventloop_post_owned` takes
ownership only on success and calls the required cleanup once, after the
callback or on cancellation by loop free. During teardown, posts, free and
step are rejected. Treat both queue fields as owned by the loop: configure
their limits, but use loop functions to post, dispatch or free them; do not
manipulate their entries directly.

A single-threaded renderer host can call
`mulib_browser_eventloop_at_checkpoint` after each step. It is false while
loop callbacks are executing or any microtasks remain after a truncated
checkpoint, and true when the shared FIFO is empty even if ordinary tasks
remain. It does not capture state or guarantee a new turn occurred. The host
must capture state before dispatching another task and must not reevaluate
scripts or pump the loop from renderer delivery. Keep out-of-loop VM
execution separate from loop pumping.

This is explicit, trusted host scheduling only. The standalone
`mulib_microjs_enqueue_job` FIFO does not join this loop, and neither ECMAScript
Promise reactions nor document scripts are automatically scheduled. No
security isolation, preemption, native DOM wrappers, or untrusted script
execution is provided.

## Integration and ownership

Include `kernels/mulib-browser-eventloop.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_BROWSER_EVENTLOOP`, `MULIB_IMPL_MICROJS_PROMISE`.

The typed MicroJS adapter is inline. Define `MULIB_BROWSER_EVENTLOOP_MICROJS`
before the first inclusion in any translation unit that uses it. Its users
also need `MULIB_IMPL_MICROJS` and `MULIB_IMPL_MINIVM` once; native-only users
do not link either implementation.

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
The [mixed checkpoint regression](../tests/test-browser-eventloop-microjs.c)
uses constant JavaScript, native callbacks and an in-memory renderer to verify
ordering, retained roots and cancellation, and defers publication until the
shared queue is empty.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels \
  tests/test-browser-eventloop-microjs.c -lm -o mulib-mixed-checkpoint
./mulib-mixed-checkpoint
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`kernels/mulib-browser-eventloop.h`](../kernels/mulib-browser-eventloop.h).
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
#ifdef MULIB_BROWSER_EVENTLOOP_MICROJS
#include "mulib-microjs.h"
#endif
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

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
  /** @brief Post an owned microtask. On success, cleanup runs once after cb
   * or on loop teardown if cb has not run. On failure, ownership stays with
   * the caller. Cleanup must not manipulate the queues directly. */
  int mulib_browser_eventloop_post_owned (mulib_browser_eventloop_t *e,
                                          mulib_microjs_job_fn cb, void *ctx,
                                          mulib_microjs_job_fn cleanup);
  /** @brief See manual/mulib-browser-eventloop.md for this operation's
   * contract. */
  size_t mulib_browser_eventloop_step (mulib_browser_eventloop_t *e);
  /** @brief True outside callbacks if the loop's microtask FIFO is empty.
   * Pending tasks are allowed. This is a local publication gate, not a
   * snapshot or a check of any standalone MicroJS queues. */
  int mulib_browser_eventloop_at_checkpoint (const mulib_browser_eventloop_t *e);
  /** @brief See manual/mulib-browser-eventloop.md for this operation's
   * contract. */
  void mulib_browser_eventloop_free (mulib_browser_eventloop_t *e);

#ifdef MULIB_BROWSER_EVENTLOOP_MICROJS
  typedef void (*mulib_browser_eventloop_job_error_fn) (
      void *, const mulib_microjs_error_t *);

  typedef struct mulib_browser_eventloop_js_job_
  {
    mulib_microjs_t *vm;
    mulib_microjs_value_t function;
    mulib_browser_eventloop_job_error_fn report;
    void *report_ctx;
  } mulib_browser_eventloop_js_job_;

  static inline void
  mulib_browser_eventloop_js_run_ (void *ctx)
  {
    mulib_browser_eventloop_js_job_ *job
        = (mulib_browser_eventloop_js_job_ *)ctx;
    mulib_microjs_error_t error;
    mulib_microjs_value_t result = { MULIB_MICROJS_NULL, 0, 0, NULL, 0, NULL };
    if (mulib_microjs_call (job->vm, &job->function, NULL, 0, &result, &error)
            != MULIB_MICROJS_OK
        && job->report)
      job->report (job->report_ctx, &error);
    mulib_microjs_value_free (&result);
  }

  static inline void
  mulib_browser_eventloop_js_cleanup_ (void *ctx)
  {
    mulib_browser_eventloop_js_job_ *job
        = (mulib_browser_eventloop_js_job_ *)ctx;
    mulib_microjs_value_free (&job->function);
    free (job);
  }

  /** @brief Root a trusted function and append it to the native microtask
   * FIFO. Requires a nonzero microtasks.limit. Each accepted job consumes one
   * checkpoint unit. A failed call is reported (if requested) and consumed;
   * other entries continue in FIFO order. report_ctx is borrowed. */
  static inline int
  mulib_browser_eventloop_post_microjs (mulib_browser_eventloop_t *e,
                                        mulib_microjs_t *vm,
                                        const mulib_microjs_value_t *function,
                                        mulib_browser_eventloop_job_error_fn report,
                                        void *report_ctx)
  {
    if (!e || !vm || !function || function->type != MULIB_MICROJS_FUNCTION
        || !e->microtasks.limit
        || e->microtasks.len - e->microtasks.head >= e->microtasks.limit)
      return 0;
    mulib_browser_eventloop_js_job_ *job
        = (mulib_browser_eventloop_js_job_ *)malloc (sizeof *job);
    if (!job)
      return 0;
    const mulib_microjs_value_t empty = { MULIB_MICROJS_NULL, 0, 0, NULL, 0,
                                          NULL };
    job->vm = vm;
    job->function = empty;
    job->report = report;
    job->report_ctx = report_ctx;
    if (mulib_microjs_value_copy (vm, function, &job->function)
            != MULIB_MICROJS_OK
        || !mulib_browser_eventloop_post_owned (
            e, mulib_browser_eventloop_js_run_, job,
            mulib_browser_eventloop_js_cleanup_))
      {
        mulib_browser_eventloop_js_cleanup_ (job);
        return 0;
      }
    return 1;
  }
#endif /* MULIB_BROWSER_EVENTLOOP_MICROJS */
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
| Header | [`kernels/mulib-browser-eventloop.h`](../kernels/mulib-browser-eventloop.h) |
| Implementation switch | `MULIB_IMPL_BROWSER_EVENTLOOP` |
| Repository-local includes | [mulib-microjs-promise](mulib-microjs-promise.md), [mulib-microjs](mulib-microjs.md) |
| Complete example | [`examples/modules/browser-eventloop.c`](../examples/modules/browser-eventloop.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/browser-eventloop.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
