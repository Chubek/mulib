# queryjoin — sorted equijoin pairs

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_QUERYJOIN`.

Inputs must be sorted nondecreasing by uint64 key; row IDs are opaque. Every
duplicate-key Cartesian pair is emitted, left rows outermost and right rows
innermost, in key order. Return pair count or SIZE_MAX for unsorted/invalid
inputs, count overflow or insufficient capacity. A validation/counting pass
leaves output untouched on errors. NULL is valid for empty arrays. Output must
not overlap inputs. O(an + bn + pairs) time, no allocation. Capacity must hold
all pairs; there is no truncation or count-only mode.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-queryjoin.h`](../include/mulib-queryjoin.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Sorted equi-join pairs over uint64 keys and original row IDs. C11
 * single-header; define MULIB_IMPL_QUERYJOIN once. */
#ifndef MULIB_QUERYJOIN_H
#define MULIB_QUERYJOIN_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t key, row;
  } mulib_queryjoin_row_t;
  typedef struct
  {
    uint64_t left_row, right_row;
  } mulib_queryjoin_hit_t;
  /* Inputs sorted by key. Emits all duplicate-key pairs; SIZE_MAX on invalid/capacity.
   */
  size_t mulib_queryjoin_merge (const mulib_queryjoin_row_t *a, size_t an,
                                const mulib_queryjoin_row_t *b, size_t bn,
                                mulib_queryjoin_hit_t *out, size_t capacity);
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
| Header | [`include/mulib-queryjoin.h`](../include/mulib-queryjoin.h) |
| Implementation switch | `MULIB_IMPL_QUERYJOIN` |
| Repository-local includes | None |
| Complete example | [`examples/modules/queryjoin.c`](../examples/modules/queryjoin.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/queryjoin.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
