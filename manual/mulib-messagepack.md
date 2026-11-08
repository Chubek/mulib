# MessagePack

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_MESSAGEPACK`, `BUFFER`, `JSON`, `PARSEC`, `STRBUF` and `UTF8`.
JSON-profile codec with text-keyed maps, no bin/ext, duplicate keys or unsafe
integer rounding. Float32/64 decode; numbers emit float64. Exact owned decode
is limited to 16 MiB/depth 64; encode atomically appends.
[Example](../examples/modules/messagepack.c), [contracts](storage-server.md).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-messagepack.h`](../include/mulib-messagepack.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-messagepack.h
 * @brief MessagePack JSON profile: null/bool/finite numbers/text/arrays/maps.
 * Reject binary/ext values, non-string/duplicate map keys and integers outside
 * exact double range. Float32/64 accepted; numbers encoded as float64.
 * Define MULIB_IMPL_MESSAGEPACK, JSON, BUFFER and JSON dependencies.
 * Encode atomic append; decode exact owned value, depth 64, input <=16 MiB. */
#ifndef MULIB_MESSAGEPACK_H
#define MULIB_MESSAGEPACK_H
#include "mulib-buffer.h"
#include "mulib-json.h"
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_messagepack_encode (const mulib_json_t *value, mulib_buffer_t *out);
  mulib_json_t *mulib_messagepack_decode (const void *data, size_t length);
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
| Header | [`include/mulib-messagepack.h`](../include/mulib-messagepack.h) |
| Implementation switch | `MULIB_IMPL_MESSAGEPACK` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-json](mulib-json.md) |
| Complete example | [`examples/modules/messagepack.c`](../examples/modules/messagepack.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/messagepack.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
