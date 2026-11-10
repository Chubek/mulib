# zoneindex — numeric range pruning

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_ZONEINDEX`.

Each zone holds finite min/max with low ≤ high; zones need not be sorted.
Filter returns indices of zones intersecting a finite inclusive query range,
in input order. Return SIZE_MAX for invalid ranges/pointers or insufficient
capacity, without changing output. Empty input allows NULL. A selected zone
is a candidate requiring exact row filtering; null-only blocks should be
excluded by the caller. Two O(count) passes, no allocation or stored index.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-zoneindex.h`](../include/mulib-zoneindex.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Zone-map range pruning over finite per-block numeric min/max.
 * Zones need not be sorted. C11 single-header; define MULIB_IMPL_ZONEINDEX once. */
#ifndef MULIB_ZONEINDEX_H
#define MULIB_ZONEINDEX_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    double low, high;
  } mulib_zoneindex_zone_t;
  /* Returns number of zones intersecting query range; SIZE_MAX on invalid/capacity. */
  size_t mulib_zoneindex_filter (const mulib_zoneindex_zone_t *zones, size_t count,
                                 double low, double high, size_t *blocks,
                                 size_t capacity);
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
| Header | [`include/mulib-zoneindex.h`](../include/mulib-zoneindex.h) |
| Implementation switch | `MULIB_IMPL_ZONEINDEX` |
| Repository-local includes | None |
| Complete example | [`examples/modules/zoneindex.c`](../examples/modules/zoneindex.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/zoneindex.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
