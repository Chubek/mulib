# vectorindex — exact Euclidean nearest neighbor

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_VECTORINDEX`; link `-lm`.

Zero state and set positive fixed `dimension`. Insert copies finite vectors of
that dimension, replacing existing IDs atomically; return 1 on success or 0
for invalid/allocation/overflow failure. Sources may alias existing vectors.
Nearest requires both outputs and a finite query, returning 1 with nearest ID
and Euclidean distance, or 0 on invalid/empty input without output mutation.
Ties choose the smallest ID. Distances may overflow to infinity; this can lose
ordering between unrepresentable distances. Search costs O(count × dimension),
not approximate nearest-neighbor indexing. Free clears dimension as well as data.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-vectorindex.h`](../include/mulib-vectorindex.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Owned exact single-nearest-neighbor index over finite double vectors.
 * IDs may be replaced. C11 single-header; define MULIB_IMPL_VECTORINDEX once. */
#ifndef MULIB_VECTORINDEX_H
#define MULIB_VECTORINDEX_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t id;
    double *values;
  } mulib_vectorindex_entry_t;
  typedef struct
  {
    size_t dimension, count, capacity;
    mulib_vectorindex_entry_t *entries;
  } mulib_vectorindex_t;
  int mulib_vectorindex_insert (mulib_vectorindex_t *idx, uint64_t id,
                                const double *values, size_t dimension);
  /* Return nearest id, 1 success, 0 absent/invalid. */
  int mulib_vectorindex_nearest (const mulib_vectorindex_t *idx, const double *query,
                                 size_t dimension, uint64_t *id, double *distance);
  void mulib_vectorindex_free (mulib_vectorindex_t *idx);
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
| Header | [`include/mulib-vectorindex.h`](../include/mulib-vectorindex.h) |
| Implementation switch | `MULIB_IMPL_VECTORINDEX` |
| Repository-local includes | None |
| Complete example | [`examples/modules/vectorindex.c`](../examples/modules/vectorindex.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/vectorindex.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
