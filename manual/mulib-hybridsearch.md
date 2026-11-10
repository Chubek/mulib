# hybridsearch — reciprocal rank fusion

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_HYBRIDSEARCH`.

Each best-first input list contributes `1/(offset + rank)` with one-based rank.
Only the first occurrence of an ID in each list contributes; its original index
sets its rank. Shared IDs sum both contributions. Offset must be finite and
nonnegative. Results are unique IDs sorted by descending score, then ascending
ID. Capacity must fit the entire union; return its size or SIZE_MAX for invalid
arguments/capacity, without writes on error. Empty lists accept NULL pointers.
No allocation; duplicate detection is quadratic and final sorting O(u log u).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-hybridsearch.h`](../include/mulib-hybridsearch.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Reciprocal rank fusion of two ranked ID arrays. C11 single-header; define
 * MULIB_IMPL_HYBRIDSEARCH once. */
#ifndef MULIB_HYBRIDSEARCH_H
#define MULIB_HYBRIDSEARCH_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t id;
    double score;
  } mulib_hybridsearch_hit_t;
  /* Arrays are ordered best first; only the first occurrence per list scores.
   * Return unique IDs sorted by descending score, then ascending ID.
   * SIZE_MAX means invalid/capacity error; output is unchanged on failure. */
  size_t mulib_hybridsearch_rrf (const uint64_t *lexical, size_t lex_count,
                                 const uint64_t *vector, size_t vec_count,
                                 double offset, mulib_hybridsearch_hit_t *out,
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
| Header | [`include/mulib-hybridsearch.h`](../include/mulib-hybridsearch.h) |
| Implementation switch | `MULIB_IMPL_HYBRIDSEARCH` |
| Repository-local includes | None |
| Complete example | [`examples/modules/hybridsearch.c`](../examples/modules/hybridsearch.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/hybridsearch.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
