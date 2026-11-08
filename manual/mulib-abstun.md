# abstun — abstract packet tunnel

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_ABSTUN`; no dependencies.
See [`examples/modules/abstun.c`](../examples/modules/abstun.c).

An abstract tunnel borrows a context, positive maximum packet size (MTU), and
packet-atomic receive/send callbacks. No allocation, closure or stream framing
is implicit. `send` rejects empty or oversize packets before invoking the
backend. `receive` requires output capacity at least MTU and passes exactly
MTU to the backend. This avoids handing a partial-packet-sized buffer to a
packet device. The callback must enforce its maximum packet size itself.

Callback results are 0 complete packet, 1 AGAIN, or a negative error. Successful
receives must report 1..MTU bytes; failures/AGAIN report zero. There are no EOF
packets or short successful sends. Violations return -2; invalid public
arguments return -1. Public receive counts reset on failure. Caller buffers
are borrowed during each operation. External synchronization is required.

`tuntap` supplies a native Linux backend. Other application backends can map
these operations to authenticated network packet channels. Tests:
`tests/test_transport_extensions.c` exercises MTU and callback-result checks.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-abstun.h`](../include/mulib-abstun.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-abstun.h
 * @brief Abstract packet tunnel with checked MTU and packet-atomic callbacks.
 * Define MULIB_IMPL_ABSTUN once. Backend/context are borrowed.
 */
#ifndef MULIB_ABSTUN_H
#define MULIB_ABSTUN_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_abstun
  {
    void *context;
    size_t mtu;
    int (*receive) (void *, void *, size_t, size_t *);
    int (*send) (void *, const void *, size_t);
  } mulib_abstun_t;
  /** @brief Return 0 packet, 1 would-block, negative error. Receive capacity
   * must be >= MTU; successful packets have length 1..MTU. No EOF packets. */
  int mulib_abstun_receive (const mulib_abstun_t *, void *, size_t, size_t *);
  /** @brief Send one complete packet; reject empty/oversize packets before
   * I/O. */
  int mulib_abstun_send (const mulib_abstun_t *, const void *, size_t);
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
| Header | [`include/mulib-abstun.h`](../include/mulib-abstun.h) |
| Implementation switch | `MULIB_IMPL_ABSTUN` |
| Repository-local includes | None |
| Complete example | [`examples/modules/abstun.c`](../examples/modules/abstun.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/abstun.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
