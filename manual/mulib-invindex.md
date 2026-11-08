# invindex

[Manuals](README.md) · [Library index](index.md)

Binary terms map to unique ascending uint64 document-ID postings. Zero initialize.
`add` copies a posting idempotently; `at` decodes by sorted position; `remove`
removes a single posting. Empty/NUL terms and document ID zero are supported.
No tokenization, normalization, ranking, position lists or automatic intersection.

Returns 0 success, 1 absent/out-of-range, negative invalid/allocation errors.
Edits copy posting bytes before publication, so failure preserves the index.
Empty posting lists remain interned. `free` releases/reset all owned data.
Switches: `MULIB_IMPL_INVINDEX`, `MULIB_IMPL_MEMTABLE`, `MULIB_IMPL_BUFFER`,
`MULIB_IMPL_BYCUTILS`. Example: `examples/modules/invindex.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-invindex.h`](../include/mulib-invindex.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-invindex.h
 * @brief Binary term to sorted unique uint64 posting lists over memtable
 * storage. Define MULIB_IMPL_INVINDEX plus MEMTABLE, BUFFER and BYCUTILS. */
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
  /** @brief Zero init. Add a posting idempotently; 0 success, negative error.
   */
  int mulib_invindex_add (mulib_invindex_t *t, const void *term, size_t size,
                          uint64_t document);
  /** @brief Enumerate one posting by ascending index; 0 found, 1
   * absent/out-of-range. */
  int mulib_invindex_at (const mulib_invindex_t *t, const void *term,
                         size_t size, size_t index, uint64_t *document);
  /** @brief Remove posting; 0 removed, 1 absent, negative error. */
  int mulib_invindex_remove (mulib_invindex_t *t, const void *term,
                             size_t size, uint64_t document);
  /** @brief Release/reset all terms/postings. */
  void mulib_invindex_free (mulib_invindex_t *t);
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
