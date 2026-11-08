# tunnel — duplex stream relay

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_XTKS` and `MULIB_IMPL_TUNNEL`.
See [`examples/modules/tunnel.c`](../examples/modules/tunnel.c).

Initialize with two full-duplex XTK streams and two disjoint, nonempty buffers
of the same capacity. Streams, contexts and buffers are borrowed for the
relay's lifetime. There is no descriptor closure or allocation. Each direction
tracks pending bytes independently. A source is read only after its prior
buffer has drained, so destination short writes and AGAIN do not lose data.

`step` performs at most one read and one write per direction. Return 0 for
progress, 1 when both directions would block without progress, 2 once both
inputs have reached EOF and all pending output has drained, or a negative
sticky failure. Call again after readiness notification; the relay does not
busy-wait or poll sockets. A blocking backend may block within a step.

One source's EOF does not close the opposite stream or discard its pending
data. Applications implement protocol-specific half-close and timeout policy.
Initialization resets a relay but cannot close a previously supplied backend.
This module relays bytes; packet tunnels use `abstun` instead.
Tests: `tests/test_transport_extensions.c`, with independently blocked lanes.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-tunnel.h`](../include/mulib-tunnel.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-tunnel.h
 * @brief Backpressure-preserving, bounded duplex stream relay.
 * Define MULIB_IMPL_TUNNEL and MULIB_IMPL_XTKS once.
 */
#ifndef MULIB_TUNNEL_H
#define MULIB_TUNNEL_H
#include "mulib-xtks.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_tunnel_lane
  {
    unsigned char *buffer;
    size_t capacity, begin, end;
    int eof;
  } mulib_tunnel_lane_t;
  typedef struct mulib_tunnel
  {
    mulib_xtks_t peers[2];
    mulib_tunnel_lane_t lanes[2];
    int failed;
  } mulib_tunnel_t;
  /** @brief Borrow two streams and disjoint nonempty buffers. No close
   * ownership. */
  int mulib_tunnel_init (mulib_tunnel_t *, const mulib_xtks_t *,
                         const mulib_xtks_t *, void *, void *,
                         size_t capacity);
  /** @brief At most one read and write per direction. OK progress, AGAIN idle,
   * EOF both inputs ended and buffers drained, negative sticky failure. Caller
   * handles half-close policy; EOF in one direction does not discard the
   * other. */
  int mulib_tunnel_step (mulib_tunnel_t *);
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
| Header | [`include/mulib-tunnel.h`](../include/mulib-tunnel.h) |
| Implementation switch | `MULIB_IMPL_TUNNEL` |
| Repository-local includes | [mulib-xtks](mulib-xtks.md) |
| Complete example | [`examples/modules/tunnel.c`](../examples/modules/tunnel.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/tunnel.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
