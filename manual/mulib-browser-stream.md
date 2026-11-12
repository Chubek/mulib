# browser-stream

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Typed bounded event stream with channel-based observer dispatch.

## Contract and supported profile

A zero-initialized stream owns observer records but borrows callback contexts.
Subscribe accepts nonzero masks containing only bits 0..4 (token/DOM/layout/
network/execution); max_observers zero is unlimited. Emit validates channel/span,
increments a checked sequence, and synchronously dispatches in subscription order.
Payload is borrowed only during callbacks. Newly subscribed observers wait for
the next event. Recursive emit is rejected and free during dispatch is ignored.
Callback zero stops dispatch but consumes the sequence and does not roll back
earlier effects. External synchronization is required; no event history is stored.

## Integration and ownership

Include `include/mulib-browser-stream.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_BROWSER_STREAM`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/browser-stream.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-browser-stream.h`](../include/mulib-browser-stream.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-browser-stream.h
 * @brief Typed bounded event stream with channel-based observer dispatch.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_STREAM in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_BROWSER_STREAM_H
#define MULIB_BROWSER_STREAM_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_browser_channel
  {
    MULIB_BROWSER_CHANNEL_TOKEN = 1,
    MULIB_BROWSER_CHANNEL_DOM = 2,
    MULIB_BROWSER_CHANNEL_LAYOUT = 3,
    MULIB_BROWSER_CHANNEL_NETWORK = 4,
    MULIB_BROWSER_CHANNEL_EXECUTION = 5
  } mulib_browser_channel_t;
  typedef struct mulib_browser_event
  {
    mulib_browser_channel_t channel;
    unsigned code;
    uint64_t sequence, node_id;
    const void *payload;
    size_t length;
  } mulib_browser_event_t;
  typedef int (*mulib_browser_event_fn) (void *context,
                                         const mulib_browser_event_t *event);
  typedef struct mulib_browser_observer
  {
    unsigned channel_mask;
    mulib_browser_event_fn fn;
    void *ctx;
  } mulib_browser_observer_t;
  typedef struct mulib_browser_stream
  {
    mulib_browser_observer_t *observers;
    size_t len, cap, max_observers;
    uint64_t sequence;
    int dispatching;
  } mulib_browser_stream_t;
  /** @brief See manual/mulib-browser-stream.md for this operation's contract.
   */
  int mulib_browser_stream_subscribe (mulib_browser_stream_t *stream,
                                      unsigned channel_mask,
                                      mulib_browser_event_fn callback,
                                      void *ctx);
  /** @brief See manual/mulib-browser-stream.md for this operation's contract.
   */
  int mulib_browser_stream_emit (mulib_browser_stream_t *stream,
                                 mulib_browser_channel_t channel,
                                 unsigned code, uint64_t node_id,
                                 const void *data, size_t length);
  /** @brief See manual/mulib-browser-stream.md for this operation's contract.
   */
  void mulib_browser_stream_free (mulib_browser_stream_t *stream);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_STREAM_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browser-stream.h`](../include/mulib-browser-stream.h) |
| Implementation switch | `MULIB_IMPL_BROWSER_STREAM` |
| Repository-local includes | None |
| Complete example | [`examples/modules/browser-stream.c`](../examples/modules/browser-stream.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browser-stream.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
