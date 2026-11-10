# MuLIB sort

## Stable sorting and lower bounds

`mulib_sort_stable` is a bottom-up stable merge sort: equal elements preserve
their original order. It returns 1 on success, 0 on invalid arguments, count×stride
overflow, or allocation failure; allocation precedes mutation. Count zero permits
NULL base; nonzero stride and comparator are still required. Time O(n log n),
scratch O(n × stride). The comparator must impose a consistent ordering.

`mulib_sort_lower_bound` returns the first index whose element is not less than
the key, or count when all are less, or SIZE_MAX for invalid arguments. It uses
the same heterogeneous comparator convention as bsearch: `compare(key,element)`.
An empty array permits NULL base and returns 0. It requires data sorted in the
same ordering. Lookup is O(log n) with no allocation.

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
/* MuLIB compiler tools: Patched generic sorting with additional stable merge sort and lower-bound lookup.
 * ISO C11 single-header module. Define MULIB_IMPL_SORT in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_SORT_H
#define MULIB_SORT_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
void mulib_sort_array(void *base,size_t count,size_t stride,int (*compare)(const void *,const void *));
void *mulib_sort_binary(const void *key,const void *base,size_t count,size_t stride,int (*compare)(const void *,const void *));
/* Stable O(n log n), 1 success, 0 invalid or allocation failure. */
int mulib_sort_stable(void *base,size_t count,size_t stride,int (*compare)(const void *,const void *));
/* Index of first element not less than key; SIZE_MAX invalid. */
size_t mulib_sort_lower_bound(const void *key,const void *base,size_t count,size_t stride,int (*compare)(const void *,const void *));
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
