# invindex

[Manuals](README.md) · [Library index](index.md)

Binary terms map to unique ascending uint64 document-ID postings. Zero initialize.
`add` copies a posting idempotently; `at` decodes by sorted position; `remove`
removes a single posting. Empty/NUL terms and document ID zero are supported.
No tokenization, normalization, ranking or position lists are performed.

Returns 0 success, 1 absent/out-of-range, negative invalid/allocation errors.
Edits copy posting bytes before publication, so failure preserves the index.
Empty posting lists remain interned. `free` releases/reset all owned data.
Switches: `MULIB_IMPL_INVINDEX`, `MULIB_IMPL_MEMTABLE`, `MULIB_IMPL_BUFFER`,
`MULIB_IMPL_BYCUTILS`. Example: `examples/modules/invindex.c`.

`count` returns posting count, zero for absent terms, SIZE_MAX for invalid input
or malformed posting bytes. `intersect` emits ascending unique IDs shared by
two terms. Capacity must hold all matches; invalid input/insufficient capacity
returns SIZE_MAX without output mutation. Both inputs may name the same term.
Output storage must not overlap index storage. Two merge passes perform term
lookups while walking postings. Regression: `tests/test_data_patches.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-invindex.h`](../include/mulib-invindex.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-invindex.h
 * @brief Patched owned sorted document postings with count and AND-intersection.
 * Define MULIB_IMPL_INVINDEX plus MEMTABLE, BUFFER, BYCUTILS. */
#ifndef MULIB_INVINDEX_H
#define MULIB_INVINDEX_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_invindex
  {
    mulib_memtable_t terms;
  } mulib_invindex_t;
  int mulib_invindex_add (mulib_invindex_t *t, const void *term, size_t size,
                          uint64_t doc);
  int mulib_invindex_at (const mulib_invindex_t *t, const void *term, size_t size,
                         size_t index, uint64_t *doc);
  int mulib_invindex_remove (mulib_invindex_t *t, const void *term, size_t size,
                             uint64_t doc);
  void mulib_invindex_free (mulib_invindex_t *t);
  /* New: number of postings; returns SIZE_MAX on error, zero when absent. */
  size_t mulib_invindex_count (const mulib_invindex_t *t, const void *term,
                               size_t size);
  /* New: document-ID intersection of two terms; returns SIZE_MAX on error or
   * inadequate capacity. On success out is ascending unique IDs. */
  size_t mulib_invindex_intersect (const mulib_invindex_t *t, const void *a, size_t an,
                                   const void *b, size_t bn, uint64_t *out,
                                   size_t capacity);
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
| Header | [`include/mulib-invindex.h`](../include/mulib-invindex.h) |
| Implementation switch | `MULIB_IMPL_INVINDEX` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-bycutils](mulib-bycutils.md), [mulib-memtable](mulib-memtable.md) |
| Complete example | [`examples/modules/invindex.c`](../examples/modules/invindex.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/invindex.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
