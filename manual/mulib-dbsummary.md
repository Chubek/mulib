# dbsummary

[Manuals](README.md) · [Library index](index.md)

`compute` calculates live/deleted record counts, stored key/value byte totals and
maximum sequence for a memtable. Includes tombstone keys in byte totals. Empty
tables produce zeros. It checks size_t addition overflow before publishing any
output. Returns 1 success, 0 invalid/overflow. It does not count allocator overhead,
SST bytes, cache bytes or estimate disk space.

Switches: `MULIB_IMPL_DBSUMMARY`; `MULIB_IMPL_MEMTABLE` when using table operations.
Example: `examples/modules/dbsummary.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-dbsummary.h`](../include/mulib-dbsummary.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-dbsummary.h
 * @brief Checked record/byte/sequence statistics for shared database tables.
 * Define MULIB_IMPL_DBSUMMARY; only memtable declarations are needed. */
#ifndef MULIB_DBSUMMARY_H
#define MULIB_DBSUMMARY_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_dbsummary
  {
    size_t live, deleted, key_bytes, value_bytes;
    uint64_t max_sequence;
  } mulib_dbsummary_t;
  /** @brief Compute without mutation; 1 succeeds, 0 invalid/overflow. */
  int mulib_dbsummary_compute (const mulib_memtable_t *t,
                               mulib_dbsummary_t *out);
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
| Header | [`include/mulib-dbsummary.h`](../include/mulib-dbsummary.h) |
| Implementation switch | `MULIB_IMPL_DBSUMMARY` |
| Repository-local includes | [mulib-memtable](mulib-memtable.md) |
| Complete example | [`examples/modules/dbsummary.c`](../examples/modules/dbsummary.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dbsummary.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
