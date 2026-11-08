# Jaccard similarity

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_JACCARD_SIMILARITY`; link `-lm` where needed.
[Example](../examples/modules/jaccard_similarity.c).

`mulib_jaccard_similarity(a,n,b,m)` treats borrowed uint64 arrays as sets,
ignoring duplicate elements and order, and returns intersection/union.
Two empty sets return 1, exactly one empty set returns 0. NULL requires zero
corresponding length; invalid input returns NAN.
No hashing, allocation, sorting, weighting or multiset semantics: time
O(n²+m²+n*m), constant extra storage. Intended for small sets.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-jaccard_similarity.h`](../include/mulib-jaccard_similarity.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-jaccard_similarity.h
 * @brief Jaccard similarity of uint64 sets, ignoring repeated elements.
 * Define MULIB_IMPL_JACCARD_SIMILARITY. Quadratic time, constant storage.
 */
#ifndef MULIB_JACCARD_SIMILARITY_H
#define MULIB_JACCARD_SIMILARITY_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Intersection/union; two empty sets give 1. NAN for invalid pointers. */
double mulib_jaccard_similarity(const uint64_t *a, size_t n, const uint64_t *b, size_t m);
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
| Header | [`include/mulib-jaccard_similarity.h`](../include/mulib-jaccard_similarity.h) |
| Implementation switch | `MULIB_IMPL_JACCARD_SIMILARITY` |
| Repository-local includes | None |
| Complete example | [`examples/modules/jaccard_similarity.c`](../examples/modules/jaccard_similarity.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/jaccard_similarity.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
