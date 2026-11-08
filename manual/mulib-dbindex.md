# dbindex

[Manuals](README.md) · [Library index](index.md)

Binary key to uint64 row mapping over shared memtable records. Zero initialize;
`set` copies/replaces, `get` decodes a row ID, `remove` writes a tombstone.
Empty/NUL-containing keys are supported. Returns 0 success, 1 absent/deleted,
negative invalid/allocation errors; lookup output unchanged on failure.

Does not automatically update a row store or other indexes. `free` releases all
keys/tombstones. Switches: `MULIB_IMPL_DBINDEX`, `MULIB_IMPL_MEMTABLE`,
`MULIB_IMPL_BYCUTILS`. Example: `examples/modules/dbindex.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-dbindex.h`](../include/mulib-dbindex.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-dbindex.h
 * @brief Binary database key to row-ID index, sharing the memtable model.
 * Define MULIB_IMPL_DBINDEX plus MEMTABLE and BYCUTILS switches. */
#ifndef MULIB_DBINDEX_H
#define MULIB_DBINDEX_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_dbindex
  {
    mulib_memtable_t keys;
  } mulib_dbindex_t;
  /** @brief Copy/replace key->row mapping; 0 success, negative failure. Zero
   * init. */
  int mulib_dbindex_set (mulib_dbindex_t *t, const void *key, size_t size,
                         uint64_t row);
  /** @brief Lookup; 0 found, 1 absent/deleted, negative invalid. */
  int mulib_dbindex_get (const mulib_dbindex_t *t, const void *key,
                         size_t size, uint64_t *row);
  /** @brief Tombstone a key; 0 success, 1 already absent, negative error. */
  int mulib_dbindex_remove (mulib_dbindex_t *t, const void *key, size_t size);
  /** @brief Free/reset. */
  void mulib_dbindex_free (mulib_dbindex_t *t);
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
| Header | [`include/mulib-dbindex.h`](../include/mulib-dbindex.h) |
| Implementation switch | `MULIB_IMPL_DBINDEX` |
| Repository-local includes | [mulib-bycutils](mulib-bycutils.md), [mulib-memtable](mulib-memtable.md) |
| Complete example | [`examples/modules/dbindex.c`](../examples/modules/dbindex.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dbindex.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
