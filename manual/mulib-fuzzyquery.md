# fuzzyquery — bounded byte edit distance

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_FUZZYQUERY`.

Compute unit-cost Levenshtein insertion/deletion/substitution distance over
explicit byte ranges, including embedded NUL. Return the exact distance when
≤ max_distance, otherwise max_distance + 1. `SIZE_MAX` indicates invalid input,
allocation/size failure; max_distance must be less than SIZE_MAX. With limit
SIZE_MAX-1 the above-limit sentinel and error coincide. NULL is valid for an
empty range. Inputs remain unchanged. O(an × bn) worst-case time and
O(min(an,bn)) memory; this is byte distance, not Unicode code-point distance.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-fuzzyquery.h`](../include/mulib-fuzzyquery.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Levenshtein bounded byte edit-distance with O(min(n,m)) memory. C11
 * single-header; define MULIB_IMPL_FUZZYQUERY once. */
#ifndef MULIB_FUZZYQUERY_H
#define MULIB_FUZZYQUERY_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  /* Returns edit distance if <=max_distance, otherwise max_distance+1;
   * SIZE_MAX indicates invalid/overflow/OOM. max_distance must be <SIZE_MAX. */
  size_t mulib_fuzzyquery_distance (const void *a, size_t an, const void *b, size_t bn,
                                    size_t max_distance);
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
| Header | [`include/mulib-fuzzyquery.h`](../include/mulib-fuzzyquery.h) |
| Implementation switch | `MULIB_IMPL_FUZZYQUERY` |
| Repository-local includes | None |
| Complete example | [`examples/modules/fuzzyquery.c`](../examples/modules/fuzzyquery.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/fuzzyquery.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
