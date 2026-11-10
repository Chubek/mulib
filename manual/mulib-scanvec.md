# scanvec — numeric selection vectors

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_SCANVEC`.

Evaluate EQ/NE/LT/LE/GT/GE against a double RHS, returning matching input indices
in increasing order. NaN on either side never matches, including NE; infinities
otherwise follow ordinary comparison rules. Return count or SIZE_MAX for
invalid predicate/pointers or insufficient capacity; output remains unchanged
on error. Empty arrays accept NULL. Two passes avoid partial output, O(count)
time and no allocation. Null validity masks must be handled by the caller.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-scanvec.h`](../include/mulib-scanvec.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Selection-vector numeric predicates with explicit per-call capacity. C11
 * single-header; define MULIB_IMPL_SCANVEC once. */
#ifndef MULIB_SCANVEC_H
#define MULIB_SCANVEC_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum
  {
    MULIB_SCAN_EQ,
    MULIB_SCAN_NE,
    MULIB_SCAN_LT,
    MULIB_SCAN_LE,
    MULIB_SCAN_GT,
    MULIB_SCAN_GE
  } mulib_scan_predicate_t;
  /* Returns count; SIZE_MAX if invalid or output insufficient (no write). NaN never
   * matches. */
  size_t mulib_scanvec_double (const double *values, size_t count,
                               mulib_scan_predicate_t pred, double rhs, size_t *indices,
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
| Header | [`include/mulib-scanvec.h`](../include/mulib-scanvec.h) |
| Implementation switch | `MULIB_IMPL_SCANVEC` |
| Repository-local includes | None |
| Complete example | [`examples/modules/scanvec.c`](../examples/modules/scanvec.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/scanvec.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
