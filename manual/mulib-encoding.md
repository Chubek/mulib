# encoding — ULEB128 and ZigZag

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_ENCODING`.

Unsigned encode requires a ten-byte buffer and returns 1–10 bytes written,
or 0 for NULL output. Decode consumes at most ten bytes, returning the number
used or 0 for truncation/overflow/NULL arguments. Failure preserves the output
value. Nonminimal encodings are accepted; encode always emits minimal ULEB128.
Trailing bytes after a terminating byte are not consumed. ZigZag maps all int64
values bijectively to uint64 without signed shifts or signed overflow. These
allocation-free helpers do not frame records or validate application schemas.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-encoding.h`](../include/mulib-encoding.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Checked ULEB128 and ZigZag codecs for portable signed/unsigned record storage.
 * C11 single-header; define MULIB_IMPL_ENCODING once. */
#ifndef MULIB_ENCODING_H
#define MULIB_ENCODING_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  size_t mulib_encoding_uvarint (uint64_t value, unsigned char out[10]);
  /* Returns number of bytes used or 0 for truncated/overflow/invalid. */
  size_t mulib_encoding_read_uvarint (const unsigned char *data, size_t length,
                                      uint64_t *value);
  uint64_t mulib_encoding_zigzag (int64_t value);
  int64_t mulib_encoding_unzigzag (uint64_t value);
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
| Header | [`include/mulib-encoding.h`](../include/mulib-encoding.h) |
| Implementation switch | `MULIB_IMPL_ENCODING` |
| Repository-local includes | None |
| Complete example | [`examples/modules/encoding.c`](../examples/modules/encoding.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/encoding.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
