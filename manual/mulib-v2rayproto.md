# v2rayproto — VLESS v0 headers

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_SOCKS5` and `MULIB_IMPL_V2RAYPROTO`.
See [`examples/modules/v2rayproto.c`](../examples/modules/v2rayproto.c).

This module implements VLESS version-zero request headers: version, 16-byte
UUID, zero addons length, command, big-endian port, address type and address.
TCP=1 and UDP=2 are supported. Address types are correctly translated between
VLESS (1 IPv4, 2 domain, 3 IPv6) and the shared SOCKS5 address model. Port zero
is rejected. A complete header's consumed length leaves any following payload
in caller storage; UUID/address data are copied into the result.

`decode` returns 1 complete, 0 incomplete, -1 malformed/invalid, -2 unsupported
version/addons/command. Outputs stay unchanged on failure and used is zero.
`encode` returns 1/0 with the same supported profile and resets used on failure.
All buffers are borrowed; output must not overlap the source. No allocation.

UUIDs identify clients; they do not encrypt or authenticate transport traffic
by themselves. Authorization and an authenticated transport belong to the
application. VMess, VLESS addons/flow extensions, multiplexing, response
headers and UDP payload framing are outside this request-header API.
Tests: `tests/test_protocol_extensions.c`, including every incomplete prefix.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-v2rayproto.h`](../include/mulib-v2rayproto.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-v2rayproto.h
 * @brief VLESS version-zero request headers without addons or multiplexing.
 * Define MULIB_IMPL_V2RAYPROTO and MULIB_IMPL_SOCKS5 once. UUID is an
 * identifier, not encryption; use an authenticated transport. VMess is not
 * implemented.
 */
#ifndef MULIB_V2RAYPROTO_H
#define MULIB_V2RAYPROTO_H
#include "mulib-socks5.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_v2rayproto_request
  {
    unsigned char uuid[16];
    unsigned command; /* 1 TCP, 2 UDP */
    mulib_socks5_address_t address;
  } mulib_v2rayproto_request_t;
  /** @brief Decode header: 1 success, 0 incomplete, -1 malformed, -2
   * unsupported version/addons/command. Output unchanged on failure; used=0.
   */
  int mulib_v2rayproto_decode (const void *, size_t,
                               mulib_v2rayproto_request_t *, size_t *used);
  /** @brief Encode supported header, 1 success/0 failure; used=0 on failure.
   */
  int mulib_v2rayproto_encode (const mulib_v2rayproto_request_t *, void *,
                               size_t, size_t *used);
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
| Header | [`include/mulib-v2rayproto.h`](../include/mulib-v2rayproto.h) |
| Implementation switch | `MULIB_IMPL_V2RAYPROTO` |
| Repository-local includes | [mulib-socks5](mulib-socks5.md) |
| Complete example | [`examples/modules/v2rayproto.c`](../examples/modules/v2rayproto.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/v2rayproto.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
