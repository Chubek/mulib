# MuLIB dsu

[Manuals](README.md) · [Library index](index.md)

Union-find disjoint-set forest with path compression and union by rank. Find returns SIZE_MAX for out-of-range indices, which cannot be a valid root in an allocated DSU.

## Contract and behavior

Initialize fresh storage with the number of elements; indices run from
zero to count minus one. `find()` returns a representative, or `SIZE_MAX` for
an invalid index. It performs path compression and therefore mutates the set.
`union()` uses rank and returns 1 even when the two elements are already joined;
0 means invalid input. `init()` returns 0 on allocation/size failure. `free()`
releases both arrays and zeros state. Representatives can change after unions;
they are not stable application identifiers.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_DSU
#include "mulib-dsu.h"
int main(void) {
    mulib_dsu_t d;
    assert(mulib_dsu_init(&d, 3));
    assert(mulib_dsu_union(&d, 0, 2));
    assert(mulib_dsu_find(&d, 0) == mulib_dsu_find(&d, 2));
    mulib_dsu_free(&d);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-dsu.h`](../include/mulib-dsu.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-dsu.h
 * @brief Union-find disjoint-set forest with path compression and union by
 * rank.
 * @defgroup mulib_dsu MuLIB dsu
 * @{
 * @details Find returns SIZE_MAX for out-of-range indices, which cannot be a
 * valid root in an allocated DSU.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_DSU before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_DSU_H
#define MULIB_DSU_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Union-find disjoint-set forest with path compression and union by
   * rank. */
  typedef struct mulib_dsu
  {
    size_t *parent;
    unsigned char *rank;
    size_t count;
  } mulib_dsu_t;
  /** @brief mulib dsu init; see manual/mulib-dsu.md for contracts and
   * examples. */
  int mulib_dsu_init (mulib_dsu_t *d, size_t n);
  /** @brief mulib dsu find; see manual/mulib-dsu.md for contracts and
   * examples. */
  size_t mulib_dsu_find (mulib_dsu_t *d, size_t i);
  /** @brief mulib dsu union; see manual/mulib-dsu.md for contracts and
   * examples. */
  int mulib_dsu_union (mulib_dsu_t *d, size_t a, size_t b);
  /** @brief mulib dsu free; see manual/mulib-dsu.md for contracts and
   * examples. */
  void mulib_dsu_free (mulib_dsu_t *d);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_DSU_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dsu.h`](../include/mulib-dsu.h) |
| Implementation switch | `MULIB_IMPL_DSU` |
| Repository-local includes | None |
| Complete example | [`examples/modules/dsu.c`](../examples/modules/dsu.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dsu.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
