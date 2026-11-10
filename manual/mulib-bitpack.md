# bitpack — fixed-width unsigned packing

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_BITPACK`.

Widths are 1–32 bits. `bytes(count,width)` returns ceil(count × width / 8),
or `SIZE_MAX` for invalid width/arithmetic overflow. Bits are emitted least
significant first within each value and byte; unused final bits are zero.
Encode/decode return 1 on success, 0 on invalid input or insufficient bytes.
Encode checks every value fits before writing. Decode requires `count` output
slots. Empty input accepts NULL buffers. Buffers must not overlap. Both
operations are O(count × width), allocation-free, and leave output intact on error.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-bitpack.h`](../include/mulib-bitpack.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Pack/unpack unsigned fixed-width values 1..32 bits with explicit byte bounds.
 * C11 single-header; define MULIB_IMPL_BITPACK once. */
#ifndef MULIB_BITPACK_H
#define MULIB_BITPACK_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  size_t mulib_bitpack_bytes (size_t count, unsigned width);
  int mulib_bitpack_encode (const uint32_t *values, size_t count, unsigned width,
                            unsigned char *out, size_t out_bytes);
  int mulib_bitpack_decode (const unsigned char *input, size_t input_bytes,
                            size_t count, unsigned width, uint32_t *out);
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
| Header | [`include/mulib-bitpack.h`](../include/mulib-bitpack.h) |
| Implementation switch | `MULIB_IMPL_BITPACK` |
| Repository-local includes | None |
| Complete example | [`examples/modules/bitpack.c`](../examples/modules/bitpack.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bitpack.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
