# datagram — UDP endpoints

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_FILECTL` and `MULIB_IMPL_DATAGRAM`.
See [`examples/modules/datagram.c`](../examples/modules/datagram.c).

POSIX sockets support IPv4 and IPv6. Addresses contain network-order address
bytes, a host-order port, family 4/6, and IPv6 scope ID. No DNS or text parsing
occurs. Initialize owning descriptors with `MULIB_DATAGRAM_INIT`; `open` binds
one, sets close-on-exec and optional nonblocking mode, and reports the actual
local address (port zero selects an ephemeral port). Failure leaves outputs
unchanged and closes any intermediate descriptor. Do not copy a live owner.

`send` transmits an entire packet, including an empty packet, with a portable
65507-byte maximum. `receive` reports the sender and copied length. A truncated
packet is consumed and returns TRUNCATED (2); the copied prefix length is valid.
An empty packet returns OK, not EOF. A zero-capacity receive still consumes a
packet and reports truncation if needed. Interrupted send/receive calls retry.

Results: 0 OK, 1 AGAIN, 2 TRUNCATED (receive only), -1 invalid, -2 OS error,
-3 unsupported platform. Counts reset on errors; OS failures preserve errno.
`close` resets ownership and never retries close after EINTR. No connection,
multicast, authentication or fragmentation-reassembly API is supplied.
Tests: `tests/test_ipc_extensions.c`, using unprivileged loopback UDP.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-datagram.h`](../include/mulib-datagram.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-datagram.h
 * @brief IPv4/IPv6 UDP sockets with explicit truncation and address metadata.
 * Define MULIB_IMPL_DATAGRAM and MULIB_IMPL_FILECTL once.
 */
#ifndef MULIB_DATAGRAM_H
#define MULIB_DATAGRAM_H
#include "mulib-filectl.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_datagram_address
  {
    unsigned char bytes[16];
    uint16_t port;
    unsigned family; /* 4 or 6 */
    uint32_t scope;
  } mulib_datagram_address_t;
  typedef struct mulib_datagram
  {
    int fd;
  } mulib_datagram_t;
#define MULIB_DATAGRAM_INIT { -1 }
  enum
  {
    MULIB_DATAGRAM_OK = 0,
    MULIB_DATAGRAM_AGAIN = 1,
    MULIB_DATAGRAM_TRUNCATED = 2,
    MULIB_DATAGRAM_EINVAL = -1,
    MULIB_DATAGRAM_EIO = -2,
    MULIB_DATAGRAM_ENOTSUP = -3
  };
  /** @brief Bind a new owning socket; port zero selects an ephemeral port.
   * On success local receives the bound address. Output must have fd == -1. */
  int mulib_datagram_open (mulib_datagram_t *,
                           const mulib_datagram_address_t *, int nonblocking,
                           mulib_datagram_address_t *local);
  /** @brief Send an entire datagram (including zero bytes); 65507 byte
   * maximum. */
  int mulib_datagram_send (mulib_datagram_t *,
                           const mulib_datagram_address_t *, const void *,
                           size_t);
  /** @brief Receive one datagram. Truncation consumes it and returns the
   * copied prefix length. Empty datagrams are OK, never EOF. */
  int mulib_datagram_receive (mulib_datagram_t *, mulib_datagram_address_t *,
                              void *, size_t, size_t *);
  /** @brief Close once and reset fd to -1. */
  int mulib_datagram_close (mulib_datagram_t *);
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
| Header | [`include/mulib-datagram.h`](../include/mulib-datagram.h) |
| Implementation switch | `MULIB_IMPL_DATAGRAM` |
| Repository-local includes | [mulib-filectl](mulib-filectl.md) |
| Complete example | [`examples/modules/datagram.c`](../examples/modules/datagram.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/datagram.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
