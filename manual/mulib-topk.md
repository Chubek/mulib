# topk — deterministic scored selection

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_TOPK`.

Select up to k entries by descending finite score, breaking ties by ascending
ID. Exact score/ID ties retain input order; repeated IDs are not deduplicated.
Return min(k,n), or SIZE_MAX for invalid pointers, nonfinite scores or exact
input/output identity. All overlap is prohibited. Scores are validated before
writing, including when k=0. NULL output is allowed only for k=0. The caller
provides k output slots. O(n × k) time, no allocation; appropriate for small k.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-topk.h`](../include/mulib-topk.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Stable deterministic top-k selection of scored identifiers. C11 single-header;
 * define MULIB_IMPL_TOPK once. */
#ifndef MULIB_TOPK_H
#define MULIB_TOPK_H
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
  } mulib_topk_entry_t;
  /* Order: descending score, ascending id. Returns min(k,n), SIZE_MAX invalid. */
  size_t mulib_topk_select (const mulib_topk_entry_t *in, size_t n,
                            mulib_topk_entry_t *out, size_t k);
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
| Header | [`include/mulib-topk.h`](../include/mulib-topk.h) |
| Implementation switch | `MULIB_IMPL_TOPK` |
| Repository-local includes | None |
| Complete example | [`examples/modules/topk.c`](../examples/modules/topk.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/topk.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
