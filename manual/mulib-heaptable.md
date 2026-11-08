# heaptable

[Manuals](README.md) · [Library index](index.md)

Binary row storage with monotonically assigned positive uint64 row IDs. Zero
initialize. `insert` copies a row and returns its ID only on success. IDs are
never reused until `free` resets the table. `get` borrows row bytes until mutation
or release; `update` replaces or tombstones an existing live row. Deleted rows
return absent. Tombstones have empty values. No pages, disk or concurrent access.

Results: 0 success, 1 absent, negative invalid/allocation/overflow. Storage is the
shared sorted memtable with big-endian IDs as keys, not a physical disk heap.
Switches: `MULIB_IMPL_HEAPTABLE`, `MULIB_IMPL_MEMTABLE`.
Example: `examples/modules/heaptable.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-heaptable.h`](../include/mulib-heaptable.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-heaptable.h
 * @brief Append-ID binary row storage over the shared memtable.
 * Define MULIB_IMPL_HEAPTABLE and MULIB_IMPL_MEMTABLE once. */
#ifndef MULIB_HEAPTABLE_H
#define MULIB_HEAPTABLE_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_heaptable
  {
    mulib_memtable_t rows;
    uint64_t last_id;
  } mulib_heaptable_t;
  /** @brief Zero init. Copy a row and return a never-reused positive ID; 0
   * success. */
  int mulib_heaptable_insert (mulib_heaptable_t *t, const void *data,
                              size_t size, uint64_t *id);
  /** @brief Borrow row until mutation/free; 0 found, 1 absent, negative
   * invalid. */
  int mulib_heaptable_get (const mulib_heaptable_t *t, uint64_t id,
                           const void **data, size_t *size);
  /** @brief Replace or tombstone an existing live row; 0 success, 1 absent. */
  int mulib_heaptable_update (mulib_heaptable_t *t, uint64_t id,
                              const void *data, size_t size, int deleted);
  /** @brief Release all rows/reset; IDs may be reused only after reset. */
  void mulib_heaptable_free (mulib_heaptable_t *t);
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
| Header | [`include/mulib-heaptable.h`](../include/mulib-heaptable.h) |
| Implementation switch | `MULIB_IMPL_HEAPTABLE` |
| Repository-local includes | [mulib-memtable](mulib-memtable.md) |
| Complete example | [`examples/modules/heaptable.c`](../examples/modules/heaptable.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/heaptable.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
