# blockidx

[Manuals](README.md) · [Library index](index.md)

Sparse binary fence-key to uint64 block-offset map. Zero initialize. `add` copies
or replaces the minimum key associated with a block. `find` returns the offset
for the greatest fence <= search key: 0 found, 1 below all fences, negative error.
It does not check block contents or impose increasing offsets. Empty keys work.

Uses memtable ordering; offsets are encoded explicitly rather than native-cast.
Free/reset with `mulib_blockidx_free`.
Switches: `MULIB_IMPL_BLOCKIDX`, `MULIB_IMPL_MEMTABLE`, `MULIB_IMPL_BYCUTILS`.
Example: `examples/modules/blockidx.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-blockidx.h`](../include/mulib-blockidx.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-blockidx.h
 * @brief Sparse sorted binary fence keys to 64-bit block offsets.
 * Define MULIB_IMPL_BLOCKIDX plus MEMTABLE and BYCUTILS switches. */
#ifndef MULIB_BLOCKIDX_H
#define MULIB_BLOCKIDX_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_blockidx
  {
    mulib_memtable_t fences;
  } mulib_blockidx_t;
  /** @brief Copy/replace a block's minimum key; zero initialize index. 0
   * success. */
  int mulib_blockidx_add (mulib_blockidx_t *idx, const void *key, size_t size,
                          uint64_t offset);
  /** @brief Find greatest fence <= key; 0 found, 1 below all fences, negative
   * invalid. */
  int mulib_blockidx_find (const mulib_blockidx_t *idx, const void *key,
                           size_t size, uint64_t *offset);
  /** @brief Free/reset all keys. */
  void mulib_blockidx_free (mulib_blockidx_t *idx);
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
| Header | [`include/mulib-blockidx.h`](../include/mulib-blockidx.h) |
| Implementation switch | `MULIB_IMPL_BLOCKIDX` |
| Repository-local includes | [mulib-bycutils](mulib-bycutils.md), [mulib-memtable](mulib-memtable.md) |
| Complete example | [`examples/modules/blockidx.c`](../examples/modules/blockidx.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/blockidx.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
