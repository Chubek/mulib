# index

[Manuals](README.md) · [Library index](index.md)

Unique ordered int64-key to uint64-row-ID secondary index over the B+ tree. Zero
initialize. `insert` returns 0 for duplicates rather than replacing. `find` may
omit its value output, and `remove` returns whether a key existed. All successful
operations return 1; insert failures also include allocation/invalid input.

This index is independent of a row store: callers maintain cross-table
consistency. `free` releases nodes; no disk or concurrent synchronization.
Switches: `MULIB_IMPL_INDEX`, `MULIB_IMPL_BPTREE`.
Example: `examples/modules/index.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-index.h`](../include/mulib-index.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-index.h
 * @brief Unique ordered integer-to-row-ID secondary index using the B+ tree.
 * Define MULIB_IMPL_INDEX and MULIB_IMPL_BPTREE once. */
#ifndef MULIB_INDEX_H
#define MULIB_INDEX_H
#include "mulib-b+tree.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_index
  {
    mulib_bptree_t tree;
  } mulib_index_t;
  /** @brief Zero init. Insert unique key; 1 success, 0 duplicate/invalid/OOM.
   */
  int mulib_index_insert (mulib_index_t *index, int64_t key, uint64_t row);
  /** @brief Lookup; 1 found, optional output unchanged on miss. */
  int mulib_index_find (const mulib_index_t *index, int64_t key,
                        uint64_t *row);
  /** @brief Remove; 1 found, 0 absent. */
  int mulib_index_remove (mulib_index_t *index, int64_t key);
  /** @brief Release/reset index. */
  void mulib_index_free (mulib_index_t *index);
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
| Header | [`include/mulib-index.h`](../include/mulib-index.h) |
| Implementation switch | `MULIB_IMPL_INDEX` |
| Repository-local includes | [mulib-b+tree](mulib-b+tree.md) |
| Complete example | [`examples/modules/index.c`](../examples/modules/index.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/index.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
