# Graph records

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_GRAPHDB` and `MEMTABLE`. Directed uint64 source/label/target
triples are ordered and sequence-aware; deletion writes tombstones. Neighbor
callbacks borrow records and must not mutate the store. Zero callback stops.
[Example](../examples/modules/graphdb.c), [contracts](storage-server.md).

## Edges, traversal, and ownership

Start with a zeroed database and free it with `mulib_graphdb_free`. An edge is
the directed `(source, label, target)` triple of 64-bit IDs; no vertex object
is created. `edge()` inserts or tombstones that triple and returns memtable
status (0 success, negative failure). `deleted` must be 0 or 1. Per-edge sequence
ordering ignores older updates and accepts replacements at an equal sequence.

Neighbors visits live outgoing edges in increasing `(label, target)` order.
The callback returns nonzero to continue or zero to stop; the returned count
includes the stopping callback. An empty neighborhood returns zero; invalid
arguments return `SIZE_MAX`. Do not mutate the database from a callback.
Traversal borrows database state and owns no callback context. Storage is an
ordered in-memory memtable; this API adds no persistence or query language.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-graphdb.h`](../include/mulib-graphdb.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-graphdb.h
 * @brief Directed labelled uint64 triples with memtable sequence/tombstones.
 * Define MULIB_IMPL_GRAPHDB and MEMTABLE. No implicit vertex objects, graph
 * query language or persistence. Zero initialize; callbacks must not mutate. */
#ifndef MULIB_GRAPHDB_H
#define MULIB_GRAPHDB_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_graphdb
  {
    mulib_memtable_t edges;
  } mulib_graphdb_t;
  typedef int (*mulib_graphdb_visit_fn) (void *context, uint64_t label,
                                         uint64_t target);
  int mulib_graphdb_edge (mulib_graphdb_t *db, uint64_t source, uint64_t label,
                          uint64_t target, uint64_t sequence, int deleted);
  /** Visit live outgoing edges in label,target order; zero callback stops.
   * Count includes stopping callback; SIZE_MAX invalid. */
  size_t mulib_graphdb_neighbors (const mulib_graphdb_t *db, uint64_t source,
                                  mulib_graphdb_visit_fn visit, void *context);
  void mulib_graphdb_free (mulib_graphdb_t *db);
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
| Header | [`include/mulib-graphdb.h`](../include/mulib-graphdb.h) |
| Implementation switch | `MULIB_IMPL_GRAPHDB` |
| Repository-local includes | [mulib-memtable](mulib-memtable.md) |
| Complete example | [`examples/modules/graphdb.c`](../examples/modules/graphdb.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/graphdb.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
