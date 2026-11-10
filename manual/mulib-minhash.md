# minhash — set similarity signatures

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_MINHASH`.

Init fills caller-owned signature slots with UINT64_MAX. Add hashes a binary
key with a deterministic per-slot seed and keeps each minimum; insertion order
and duplicates do not matter. NULL key is valid for zero bytes; invalid input
is ignored. Similarity is equal slots/count; NULL signatures or zero count
return zero. Both empty initialized signatures compare as 1. Compare signatures
with equal lengths and hashing conventions. This estimates Jaccard similarity,
not exact equality. Add costs O(bytes + slots), comparison O(slots); no allocation.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-minhash.h`](../include/mulib-minhash.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Seeded MinHash signatures for Jaccard similarity estimation. C11
 * single-header; define MULIB_IMPL_MINHASH once. */
#ifndef MULIB_MINHASH_H
#define MULIB_MINHASH_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  void mulib_minhash_init (uint64_t *signature, size_t count);
  void mulib_minhash_add (uint64_t *signature, size_t count, const void *key,
                          size_t bytes);
  double mulib_minhash_similarity (const uint64_t *a, const uint64_t *b, size_t count);
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
| Header | [`include/mulib-minhash.h`](../include/mulib-minhash.h) |
| Implementation switch | `MULIB_IMPL_MINHASH` |
| Repository-local includes | None |
| Complete example | [`examples/modules/minhash.c`](../examples/modules/minhash.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/minhash.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
