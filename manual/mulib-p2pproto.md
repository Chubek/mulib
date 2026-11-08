# p2pproto — multistream-select records

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_VARINT` and `MULIB_IMPL_P2PPROTO`.
See [`examples/modules/p2pproto.c`](../examples/modules/p2pproto.c).

This module implements libp2p multistream-select token records: a canonical
unsigned LEB128 byte length, followed by a printable token and LF. Length
includes LF. Tokens are 1..1023 bytes, start `/`, or equal `na`/`ls`; controls,
spaces, embedded newlines and non-ASCII bytes are rejected. The version greeting
is `/multistream/1.0.0`. Encode returns 1/0 and writes to disjoint caller storage.

Decode returns 1 complete, 0 incomplete, -1 malformed/capacity. It copies a
NUL-terminated token without LF, requires minimal varints, and reports exact
consumed bytes. Outputs are unchanged except used=0 on failure. Retain the
entire partial frame for retry. `select` returns the index of an exact supported
protocol proposal or SIZE_MAX; only slash-prefixed proposals can be selected.
The application controls greeting/order/retry state and sends encoded `na` for
an unsupported proposal. `ls` is recognized as a token; list responses are
not generated automatically.

No transport, authentication, peer discovery, stream multiplexing or specific
libp2p application protocol is implicit. All storage is borrowed, no allocation
occurs. Tests: `tests/test_protocol_extensions.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-p2pproto.h`](../include/mulib-p2pproto.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-p2pproto.h
 * @brief libp2p multistream-select length-delimited protocol negotiation
 * records. Define MULIB_IMPL_P2PPROTO and MULIB_IMPL_VARINT once.
 * Transport-independent.
 */
#ifndef MULIB_P2PPROTO_H
#define MULIB_P2PPROTO_H
#include "mulib-varint.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Encode newline-terminated protocol token, unsigned-varint length.
   * Tokens start '/' or equal "na"/"ls", printable ASCII, 1..1023 bytes.
   * 1 success/0 invalid/capacity. Output nonoverlapping, used=0 on failure. */
  int mulib_p2pproto_encode (const char *token, void *, size_t, size_t *used);
  /** @brief Decode one canonical frame; copy token without LF to
   * NUL-terminated output. 1 complete, 0 incomplete, -1 malformed/capacity.
   * used=0 on failure. */
  int mulib_p2pproto_decode (const void *, size_t, char *, size_t,
                             size_t *used);
  /** @brief Choose an exact supported protocol token; returns its index or
   * SIZE_MAX. Negotiation greeting is /multistream/1.0.0; unsupported
   * proposals should be answered with an encoded "na". No
   * transport/authentication. */
  size_t mulib_p2pproto_select (const char *proposal,
                                const char *const *supported, size_t count);
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
| Header | [`include/mulib-p2pproto.h`](../include/mulib-p2pproto.h) |
| Implementation switch | `MULIB_IMPL_P2PPROTO` |
| Repository-local includes | [mulib-varint](mulib-varint.md) |
| Complete example | [`examples/modules/p2pproto.c`](../examples/modules/p2pproto.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/p2pproto.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
