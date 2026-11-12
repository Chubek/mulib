# microjs-promise

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Native deferred-job queues and one-shot promise-like settlement.

## Contract and supported profile

Promise settlement is one-shot from zero-initialized pending state and
borrows an arbitrary value pointer; nonzero reject chooses rejected state.
The independent zero-initialized job queue owns job records but borrows callback
contexts. Post enforces pending-count limit (zero unlimited), drain executes FIFO
up to budget and includes jobs posted by callbacks. Nested drain returns zero;
free during drain is ignored. Free otherwise resets all state. Jobs cannot
destroy/reinitialize their active queue or outlive contexts. No thenable
assimilation, Promise chaining, threads or ECMAScript execution is provided.

## Integration and ownership

Include `include/mulib-microjs-promise.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_MICROJS_PROMISE`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/microjs-promise.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-microjs-promise.h`](../include/mulib-microjs-promise.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-microjs-promise.h
 * @brief Native deferred-job queues and one-shot promise-like settlement.
 * C11 single-header module. Define MULIB_IMPL_MICROJS_PROMISE in exactly one C
 * translation unit. A native job-queue substrate; not an ECMAScript Promise
 * implementation.
 */
#ifndef MULIB_MICROJS_PROMISE_H
#define MULIB_MICROJS_PROMISE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_microjs_promise_state
  {
    MULIB_MICROJS_PROMISE_PENDING = 0,
    MULIB_MICROJS_PROMISE_RESOLVED = 1,
    MULIB_MICROJS_PROMISE_REJECTED = 2
  } mulib_microjs_promise_state_t;
  typedef struct mulib_microjs_promise
  {
    mulib_microjs_promise_state_t state;
    void *value;
  } mulib_microjs_promise_t;
  typedef void (*mulib_microjs_job_fn) (void *ctx);
  typedef struct mulib_microjs_job
  {
    mulib_microjs_job_fn cb;
    void *ctx;
  } mulib_microjs_job_t;
  typedef struct mulib_microjs_microtasks
  {
    mulib_microjs_job_t *jobs;
    size_t len, cap, head, limit;
    int draining;
  } mulib_microjs_microtasks_t;
  /** @brief See manual/mulib-microjs-promise.md for this operation's contract.
   */
  int mulib_microjs_promise_settle (mulib_microjs_promise_t *p, int reject,
                                    void *value);
  /** @brief See manual/mulib-microjs-promise.md for this operation's contract.
   */
  int mulib_microjs_microtask_post (mulib_microjs_microtasks_t *q,
                                    mulib_microjs_job_fn fn, void *ctx);
  /** @brief See manual/mulib-microjs-promise.md for this operation's contract.
   */
  size_t mulib_microjs_microtask_drain (mulib_microjs_microtasks_t *q,
                                        size_t budget);
  /** @brief See manual/mulib-microjs-promise.md for this operation's contract.
   */
  void mulib_microjs_microtask_free (mulib_microjs_microtasks_t *q);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MICROJS_PROMISE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-microjs-promise.h`](../include/mulib-microjs-promise.h) |
| Implementation switch | `MULIB_IMPL_MICROJS_PROMISE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/microjs-promise.c`](../examples/modules/microjs-promise.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/microjs-promise.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
