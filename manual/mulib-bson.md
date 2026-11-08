# BSON

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_BSON`, `BUFFER`, `JSON`, `PARSEC`, `STRBUF` and `UTF8`.
Object-root JSON-profile codec: double, string, nested object/array, bool, null;
int32/int64 also decode within safe double range. No ObjectId/binary/date types.
NUL keys and duplicates fail. Decode owns values, requires exact input, limits
depth to 64 and input to 16 MiB. [Example](../examples/modules/bson.c),
[contracts](storage-server.md#storage-and-serialization).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-bson.h`](../include/mulib-bson.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-bson.h
 * @brief BSON JSON profile: documents, arrays, strings, doubles, booleans,
 * null and exact-range int32/int64 decoding. No binary/ObjectId/date types.
 * Define MULIB_IMPL_BSON, JSON, BUFFER and JSON dependencies.
 * Root must be object; duplicate keys/NUL keys rejected. Atomic encode append;
 * decode exact owned value, max depth 64 and input 16 MiB. */
#ifndef MULIB_BSON_H
#define MULIB_BSON_H
#include "mulib-buffer.h"
#include "mulib-json.h"
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_bson_encode (const mulib_json_t *document, mulib_buffer_t *out);
  mulib_json_t *mulib_bson_decode (const void *data, size_t length);
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
| Header | [`include/mulib-bson.h`](../include/mulib-bson.h) |
| Implementation switch | `MULIB_IMPL_BSON` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-json](mulib-json.md) |
| Complete example | [`examples/modules/bson.c`](../examples/modules/bson.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bson.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
