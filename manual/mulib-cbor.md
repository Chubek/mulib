# CBOR

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_CBOR`, `BUFFER`, `JSON`, `PARSEC`, `STRBUF` and `UTF8`.
RFC 8949 JSON-profile codec: text/arrays/text maps/null/bool/finite numbers.
No tags, bytes, indefinite lengths, half floats, duplicate keys or unsafe integer
rounding. Encode emits binary64 numbers. Exact owned decode is limited to
16 MiB/depth 64. [Example](../examples/modules/cbor.c), [contracts](storage-server.md).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-cbor.h`](../include/mulib-cbor.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-cbor.h
 * @brief RFC 8949 JSON-profile CBOR: null, bool, finite numbers, UTF-8 text,
 * arrays and text-keyed maps. Reject tags, byte strings, indefinite lengths,
 * duplicate keys, half floats and integers outside exact JSON double range.
 * Define MULIB_IMPL_CBOR, JSON, BUFFER and JSON dependencies.
 * Encode appends atomically, decode owns a value and requires exact input.
 * Maximum nesting 64; decode input bounded to 16 MiB. */
#ifndef MULIB_CBOR_H
#define MULIB_CBOR_H
#include "mulib-buffer.h"
#include "mulib-json.h"
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_cbor_encode (const mulib_json_t *value, mulib_buffer_t *out);
  mulib_json_t *mulib_cbor_decode (const void *data, size_t length);
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
| Header | [`include/mulib-cbor.h`](../include/mulib-cbor.h) |
| Implementation switch | `MULIB_IMPL_CBOR` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-json](mulib-json.md) |
| Complete example | [`examples/modules/cbor.c`](../examples/modules/cbor.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/cbor.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
