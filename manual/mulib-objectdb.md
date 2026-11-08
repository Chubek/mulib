# Object records

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_OBJECTDB` and `MEMTABLE`. Stores copied opaque bytes with
nonzero application type tags and binary keys, not live C pointers. Gets borrow
until mutation/free. Zero means success; type mismatch/absence is 1.
[Example](../examples/modules/objectdb.c), [contracts](storage-server.md).

## Lifecycle and record semantics

Start with `mulib_objectdb_t db = {0}` and release with
`mulib_objectdb_free(&db)`. IDs and payloads are explicit binary ranges; put
copies both. A type tag must be nonzero. An empty payload is valid, and the
database never interprets or follows pointers encoded in payload bytes.

Put/delete return the [memtable](mulib-memtable.md) result: zero for success,
negative for invalid input, allocation failure, or overflow. Sequence numbers
order updates per ID: an older update is ignored successfully, and an equal
sequence replaces the current record. Delete writes a tombstone, including for
an absent ID; it does not physically remove the key.

Get requires both output pointers and the expected type. It returns zero and
a borrowed payload on success, 1 for absent/deleted/type mismatch, or a negative
error. Outputs are unchanged unless successful. Copy data before mutating or
freeing the database. State is in-memory and caller-synchronized.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-objectdb.h`](../include/mulib-objectdb.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-objectdb.h
 * @brief In-memory type-tagged opaque object blobs, not C pointer persistence.
 * Define MULIB_IMPL_OBJECTDB and MEMTABLE. Zero initialize. Binary ids/values;
 * nonzero application type tags, sequence ordering and tombstones supported. */
#ifndef MULIB_OBJECTDB_H
#define MULIB_OBJECTDB_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_objectdb
  {
    mulib_memtable_t records;
  } mulib_objectdb_t;
  int mulib_objectdb_put (mulib_objectdb_t *db, const void *id,
                          size_t id_length, uint32_t type, const void *data,
                          size_t length, uint64_t sequence);
  /** 0 success (borrow until mutation), 1 absent/deleted/type mismatch,
   * negative invalid. Outputs unchanged on failure. */
  int mulib_objectdb_get (const mulib_objectdb_t *db, const void *id,
                          size_t id_length, uint32_t type, const void **data,
                          size_t *length);
  int mulib_objectdb_delete (mulib_objectdb_t *db, const void *id,
                             size_t id_length, uint64_t sequence);
  void mulib_objectdb_free (mulib_objectdb_t *db);
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
| Header | [`include/mulib-objectdb.h`](../include/mulib-objectdb.h) |
| Implementation switch | `MULIB_IMPL_OBJECTDB` |
| Repository-local includes | [mulib-memtable](mulib-memtable.md) |
| Complete example | [`examples/modules/objectdb.c`](../examples/modules/objectdb.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/objectdb.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
