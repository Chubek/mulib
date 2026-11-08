# lsmtree

[Manuals](README.md) · [Library index](index.md)

Memory-backed LSM composition of mutable memtable, encoded immutable runs and WAL.
Zero initialize; `put` copies key/value or deletion and advances sequence. It
allocates candidate memory before appending WAL and publishing changes, so failed
writes leave logical state/WAL unchanged. `get` appends a copied value to a caller
buffer: 0 found, 1 missing/deleted, negative error. Clear its length to replace.

`flush` creates a run and clears mutable records; `compact` merges runs preserving
newest sequences/tombstones. Both are explicit and failure-atomic. Lookup decodes
runs into temporary tables; this reference implementation prioritizes checked
composition, not production read amplification or bounded disk storage.
`recover` constructs a fresh tree from a WAL and optionally accepts only a torn
final frame; consumed length reports the valid prefix. `free` discards everything.
No disk I/O, fsync, background threads or locking. WAL bytes are retained by flush.

Switches: `MULIB_IMPL_LSMTREE`, `MULIB_IMPL_SSTFILE`, `MULIB_IMPL_MEMTABLE`,
`MULIB_IMPL_WAL`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_CRC32`.
Example: `examples/modules/lsmtree.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-lsmtree.h`](../include/mulib-lsmtree.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-lsmtree.h
 * @brief Memory-backed LSM runs with WAL-before-publication and atomic
 * compaction. Define MULIB_IMPL_LSMTREE and all MEMTABLE/SSTFILE/WAL
 * dependencies explicitly. No disk durability, concurrent access or automatic
 * background work is implied.
 */
#ifndef MULIB_LSMTREE_H
#define MULIB_LSMTREE_H
#include "mulib-sstfile.h"
#include "mulib-wal.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Zero initialize; owns mutable table, immutable runs and WAL bytes.
   */
  typedef struct mulib_lsmtree
  {
    mulib_memtable_t memory;
    mulib_buffer_t *runs;
    size_t run_count;
    mulib_buffer_t wal;
    uint64_t sequence;
  } mulib_lsmtree_t;
  /** @brief Copy a put/delete (deleted=0/1). Failure leaves logical state and
   * WAL unchanged. */
  int mulib_lsmtree_put (mulib_lsmtree_t *t, const void *key, size_t kn,
                         const void *value, size_t vn, int deleted);
  /** @brief Copy value to buffer; 0 found, 1 absent/deleted, negative on
   * failure. */
  int mulib_lsmtree_get (const mulib_lsmtree_t *t, const void *key, size_t kn,
                         mulib_buffer_t *out);
  /** @brief Publish the mutable table as another immutable run; WAL is
   * retained. */
  int mulib_lsmtree_flush (mulib_lsmtree_t *t);
  /** @brief Merge runs atomically; retain tombstones and mutable records
   * separately. */
  int mulib_lsmtree_compact (mulib_lsmtree_t *t);
  /** @brief Replay a WAL into a zero/uninitialized tree. Accept a torn last
   * frame only when allow_torn=1; consumed reports the validated prefix.
   * Corruption fails. */
  int mulib_lsmtree_recover (const void *log, size_t size, int allow_torn,
                             mulib_lsmtree_t *out, size_t *consumed);
  /** @brief Free/reset all state; no implicit persistence. */
  void mulib_lsmtree_free (mulib_lsmtree_t *t);
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
| Header | [`include/mulib-lsmtree.h`](../include/mulib-lsmtree.h) |
| Implementation switch | `MULIB_IMPL_LSMTREE` |
| Repository-local includes | [mulib-sstfile](mulib-sstfile.md), [mulib-wal](mulib-wal.md) |
| Complete example | [`examples/modules/lsmtree.c`](../examples/modules/lsmtree.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/lsmtree.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
