# hll — approximate distinct counting

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_HLL`; link `-lm`.

Zero-initialize the 1024-register sketch. Add hashes binary keys; NULL plus
zero length is an empty key, invalid pointer/length is ignored. Duplicate keys
are idempotent. Estimate uses the HLL estimator plus small-range linear counting;
empty/NULL sketches estimate zero. Merge takes register-wise maxima and accepts
self-merge. Expected relative standard error is about 3.25%, not a hard bound.
The fixed hash is noncryptographic, with no large-range correction. No allocation;
add is O(key bytes), estimate/merge O(1024). Registers must not be edited manually.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-hll.h`](../include/mulib-hll.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: HyperLogLog cardinality sketch with 64-bit hashing and small-range correction.
 * C11 single-header; define MULIB_IMPL_HLL once. */
#ifndef MULIB_HLL_H
#define MULIB_HLL_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    unsigned char registers[1024];
  } mulib_hll_t;
  void mulib_hll_add (mulib_hll_t *h, const void *key, size_t length);
  double mulib_hll_estimate (const mulib_hll_t *h);
  void mulib_hll_merge (mulib_hll_t *dst, const mulib_hll_t *src);
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
| Header | [`include/mulib-hll.h`](../include/mulib-hll.h) |
| Implementation switch | `MULIB_IMPL_HLL` |
| Repository-local includes | None |
| Complete example | [`examples/modules/hll.c`](../examples/modules/hll.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/hll.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
