# deltacodec — monotone integer deltas

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_DELTACODEC`.

Encode emits the first value relative to zero, then successive differences.
Nondecreasing uint64 input permits duplicates (delta zero). Decode reconstructs
prefix sums and rejects overflow. Both return 1 for success, 0 for invalid
pointers/order/overflow, validating the entire input before writing output.
Provide `count` output slots. Exact in-place operation is supported; other
overlap is not. Empty ranges permit NULL. O(count), allocation-free.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-deltacodec.h`](../include/mulib-deltacodec.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Monotone uint64 delta encoding and decoding with overflow checks. C11
 * single-header; define MULIB_IMPL_DELTACODEC once. */
#ifndef MULIB_DELTACODEC_H
#define MULIB_DELTACODEC_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_deltacodec_encode (const uint64_t *values, size_t count, uint64_t *deltas);
  int mulib_deltacodec_decode (const uint64_t *deltas, size_t count, uint64_t *values);
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
| Header | [`include/mulib-deltacodec.h`](../include/mulib-deltacodec.h) |
| Implementation switch | `MULIB_IMPL_DELTACODEC` |
| Repository-local includes | None |
| Complete example | [`examples/modules/deltacodec.c`](../examples/modules/deltacodec.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/deltacodec.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
