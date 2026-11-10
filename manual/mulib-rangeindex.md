# rangeindex — inclusive interval scans

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_RANGEINDEX`.

Insert signed int64 [low,high] with an opaque uint64 ID; low must not exceed
high. Duplicate IDs/intervals are retained, ordered by low then insertion order.
Return 1 on success, 0 on invalid/allocation/overflow failure. Overlap invokes
the visitor for intersecting intervals (point query uses low==high), stopping
on callback zero. Returned count includes that visit; SIZE_MAX means invalid
arguments. Callback must not mutate/free the index. Insertion and queries are
O(n); query stops when interval lows exceed high. Free resets owned entries.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-rangeindex.h`](../include/mulib-rangeindex.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Mutable interval index with exact inclusive point and overlap queries. C11
 * single-header; define MULIB_IMPL_RANGEINDEX once. */
#ifndef MULIB_RANGEINDEX_H
#define MULIB_RANGEINDEX_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    int64_t low, high;
    uint64_t id;
  } mulib_rangeindex_entry_t;
  typedef struct
  {
    mulib_rangeindex_entry_t *entries;
    size_t count, capacity;
  } mulib_rangeindex_t;
  int mulib_rangeindex_insert (mulib_rangeindex_t *idx, int64_t low, int64_t high,
                               uint64_t id);
  /* Linear exact scan, callback returns nonzero to continue; count includes stop. */
  typedef int (*mulib_rangeindex_visit_fn) (uint64_t id, void *context);
  size_t mulib_rangeindex_overlap (const mulib_rangeindex_t *idx, int64_t low,
                                   int64_t high, mulib_rangeindex_visit_fn visit,
                                   void *context);
  void mulib_rangeindex_free (mulib_rangeindex_t *idx);
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
| Header | [`include/mulib-rangeindex.h`](../include/mulib-rangeindex.h) |
| Implementation switch | `MULIB_IMPL_RANGEINDEX` |
| Repository-local includes | None |
| Complete example | [`examples/modules/rangeindex.c`](../examples/modules/rangeindex.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/rangeindex.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
