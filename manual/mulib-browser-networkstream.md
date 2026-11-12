# browser-networkstream

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Resource request and response event publishing.

## Contract and supported profile

This typed adapter publishes request/response events on the fixed NETWORK
channel of browser-stream. Kind/code and node ID are opaque caller metadata;
payload is an optional borrowed byte span and is not parsed, copied or retained.
The stream validates NULL/length pairs, checks sequence overflow and dispatches
synchronously to subscribers. Return one means accepted by every applicable
observer; zero may mean invalid input, reentrancy or observer cancellation after
partial delivery. The adapter performs no request/response processing itself.
Use browser-stream's lifecycle, mask, callback and synchronization rules.

## Integration and ownership

Include `include/mulib-browser-networkstream.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_BROWSER_NETWORKSTREAM`, `MULIB_IMPL_BROWSER_STREAM`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/browser-networkstream.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-browser-networkstream.h`](../include/mulib-browser-networkstream.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-browser-networkstream.h
 * @brief Resource request and response event publishing.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_NETWORKSTREAM in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_BROWSER_NETWORKSTREAM_H
#define MULIB_BROWSER_NETWORKSTREAM_H
#include "mulib-browser-stream.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief See manual/mulib-browser-networkstream.md for this operation's
   * contract. */
  int mulib_browser_networkstream_emit (mulib_browser_stream_t *stream,
                                        unsigned kind, uint64_t node_id,
                                        const void *data, size_t length);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_NETWORKSTREAM_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browser-networkstream.h`](../include/mulib-browser-networkstream.h) |
| Implementation switch | `MULIB_IMPL_BROWSER_NETWORKSTREAM` |
| Repository-local includes | [mulib-browser-stream](mulib-browser-stream.md) |
| Complete example | [`examples/modules/browser-networkstream.c`](../examples/modules/browser-networkstream.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browser-networkstream.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
