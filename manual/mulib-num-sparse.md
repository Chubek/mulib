# mulib-num-sparse

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-num-sparse.h`  
**Implementation switch:** `MULIB_IMPL_NUM_SPARSE`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Owned coordinate-list sparse double matrix with checked assembly, repeated-coordinate accumulation and explicit canonicalization. Canonicalization sorts by row and column, coalesces duplicates and removes exact zeros. Matvec permits aliased input/output and preserves output on failure.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_NUM_SPARSE` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_NUM_SPARSE
#include "mulib-num-sparse.h"
```

## Focused regression test

`tests/test_num_sparse.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_num_sparse.c -lm -o /tmp/test_num_sparse
/tmp/test_num_sparse
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-num-sparse.h`](../include/mulib-num-sparse.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-num-sparse.h
 * @brief Owned coordinate-list sparse double matrix with checked assembly, repeated-coordinate
 * accumulation and explicit canonicalization.
 * @details C11 single-header implementation; define MULIB_IMPL_NUM_SPARSE in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_NUM_SPARSE_H
#define MULIB_NUM_SPARSE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_num_triplet {
    size_t row, col;
    double value;
} mulib_num_triplet_t;
typedef struct mulib_num_sparse {
    size_t rows, cols, count, capacity;
    mulib_num_triplet_t *triplets;
} mulib_num_sparse_t;
int mulib_num_sparse_init(mulib_num_sparse_t *s, size_t rows, size_t cols);
int mulib_num_sparse_add(mulib_num_sparse_t *s, size_t row, size_t col, double value);
int mulib_num_sparse_matvec(const mulib_num_sparse_t *s, const double *x, double *y);
void mulib_num_sparse_free(mulib_num_sparse_t *s);
int mulib_num_sparse_validate(const mulib_num_sparse_t *s);
int mulib_num_sparse_canonicalize(mulib_num_sparse_t *s);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_NUM_SPARSE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-num-sparse.h`](../include/mulib-num-sparse.h) |
| Implementation switch | `MULIB_IMPL_NUM_SPARSE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/num-sparse.c`](../examples/modules/num-sparse.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/num-sparse.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
