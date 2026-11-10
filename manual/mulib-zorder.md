# zorder — 2D Morton keys

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_ZORDER`.

Encode interleaves all 32 bits of unsigned x and y into a uint64: x occupies
even bits, y odd bits. Decode exactly reverses this; either output pointer may
be NULL. All coordinate/key values are valid. Allocation-free with 32 fixed
iterations. Coordinates must already be quantized; Morton ordering alone does
not implement a rectangle query or preserve ordinary Euclidean distance.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-zorder.h`](../include/mulib-zorder.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Morton Z-order encoding/decoding for 2D unsigned 32-bit coordinates. C11
 * single-header; define MULIB_IMPL_ZORDER once. */
#ifndef MULIB_ZORDER_H
#define MULIB_ZORDER_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  uint64_t mulib_zorder_encode (uint32_t x, uint32_t y);
  void mulib_zorder_decode (uint64_t key, uint32_t *x, uint32_t *y);
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
| Header | [`include/mulib-zorder.h`](../include/mulib-zorder.h) |
| Implementation switch | `MULIB_IMPL_ZORDER` |
| Repository-local includes | None |
| Complete example | [`examples/modules/zorder.c`](../examples/modules/zorder.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/zorder.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
