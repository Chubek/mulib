# Embeddings

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_EMBEDDINGSDB`; link `-lm` where required. Set dimension before
use. Put copies finite vectors; nearest performs exact Euclidean linear search,
breaking ties by smallest id. Distances may overflow to infinity. No ANN index.
[Example](../examples/modules/embeddingsdb.c), [contracts](storage-server.md).

## Vector storage and nearest-neighbor queries

Zero-initialize state and set `dimension` to a positive fixed value before use.
Put requires exactly that dimension and finite doubles. It copies the vector;
replacing an ID copies successfully before releasing its old vector. Return
is 1 on success and 0 on invalid input, overflow, or allocation failure. Delete
returns 1 for an existing ID, 0 when absent, and may change entry-array order.

Nearest requires a finite query of the same dimension and both output pointers.
It returns 1 with an ID and Euclidean distance, or 0 for an empty/invalid query,
leaving outputs unchanged. Exact ties choose the smallest ID. Search scans all
vectors in O(count × dimension); distances can become infinity on arithmetic
overflow. This is exact linear search, not an approximate index.

Free releases all vectors and zeros the structure, including dimension. Reset
dimension before reuse. Keep internal vector pointers out of later mutations;
state has no persistence or internal concurrency control.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-embeddingsdb.h`](../include/mulib-embeddingsdb.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-embeddingsdb.h
 * @brief Exact linear Euclidean nearest-neighbor search over owned vectors.
 * Define MULIB_IMPL_EMBEDDINGSDB. Zero initialize and set dimension >0 before
 * use. Stable uint64 ids; replace copies before releasing old vector. No ANN,
 * persistence or concurrent access. Distances may be infinity on overflow. */
#ifndef MULIB_EMBEDDINGSDB_H
#define MULIB_EMBEDDINGSDB_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_embeddingsdb_entry
  {
    uint64_t id;
    double *vector;
  } mulib_embeddingsdb_entry_t;
  typedef struct mulib_embeddingsdb
  {
    size_t dimension, count;
    mulib_embeddingsdb_entry_t *entries;
  } mulib_embeddingsdb_t;
  int mulib_embeddingsdb_put (mulib_embeddingsdb_t *db, uint64_t id,
                              const double *vector, size_t dimension);
  int mulib_embeddingsdb_delete (mulib_embeddingsdb_t *db, uint64_t id);
  /** Output unchanged if empty/invalid. Ties choose smallest id. */
  int mulib_embeddingsdb_nearest (const mulib_embeddingsdb_t *db,
                                  const double *query, size_t dimension,
                                  uint64_t *id, double *distance);
  void mulib_embeddingsdb_free (mulib_embeddingsdb_t *db);
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
| Header | [`include/mulib-embeddingsdb.h`](../include/mulib-embeddingsdb.h) |
| Implementation switch | `MULIB_IMPL_EMBEDDINGSDB` |
| Repository-local includes | None |
| Complete example | [`examples/modules/embeddingsdb.c`](../examples/modules/embeddingsdb.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/embeddingsdb.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
