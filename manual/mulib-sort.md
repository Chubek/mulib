# MuLIB sort

[Manuals](README.md) · [Library index](index.md)

Standard-library-backed generic sorting and binary lookup. Sorting uses the platform C runtime qsort algorithm; the order of equal elements is not stable. Binary lookup requires a sorted array with the same comparison relation.

## Contract and behavior

`array()` delegates to C `qsort`; it mutates caller storage and is not
stable. The comparator must consistently order elements and return negative,
zero, or positive. `binary()` delegates to `bsearch` and requires an array
already sorted under the same ordering. It returns a borrowed matching element
or NULL; which duplicate is found is unspecified. NULL pointers, zero stride,
or size multiplication overflow cause a no-op/NULL result. The API does not
copy elements into owned storage or expose allocation/error status.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_SORT
#include "mulib-sort.h"
static int mulib_example_compare(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}
int main(void) {
    int a[] = {3, 1, 2};
    mulib_sort_array(a, 3, sizeof(int), mulib_example_compare);
    assert(a[0] == 1);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-sort.h`](../include/mulib-sort.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-sort.h
 * @brief Standard-library-backed generic sorting and binary lookup.
 * @defgroup mulib_sort MuLIB sort
 * @{
 * @details Sorting uses the platform C runtime qsort algorithm; the order of
 * equal elements is not stable. Binary lookup requires a sorted array with the
 * same comparison relation.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_SORT before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_SORT_H
#define MULIB_SORT_H
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

  /** @brief mulib sort array; see manual/mulib-sort.md for contracts and
   * examples. */
  void mulib_sort_array (void *base, size_t count, size_t stride,
                         int (*compare) (const void *, const void *));
  /** @brief mulib sort binary; see manual/mulib-sort.md for contracts and
   * examples. */
  void *mulib_sort_binary (const void *key, const void *base, size_t count,
                           size_t stride,
                           int (*compare) (const void *, const void *));

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SORT_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-sort.h`](../include/mulib-sort.h) |
| Implementation switch | `MULIB_IMPL_SORT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/sort.c`](../examples/modules/sort.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/sort.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
