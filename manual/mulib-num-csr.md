# mulib-num-csr

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-num-csr.h`  
**Implementation switch:** `MULIB_IMPL_NUM_CSR`  
**Dependencies:** `num-sparse`.

## Purpose and supported profile

Compressed sparse row conversion, validation, matvec and transpose. Columns within each row are sorted; duplicate coordinates remain additive unless the COO input is canonicalized first. Matvec permits aliased input/output and preserves output on failure.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_NUM_CSR` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_NUM_CSR
#include "mulib-num-csr.h"
```

## Focused regression test

`tests/test_num_csr.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_num_csr.c -lm -o /tmp/test_num_csr
/tmp/test_num_csr
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-num-csr.h`](../include/mulib-num-csr.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-num-csr.h
 * @brief Compressed sparse row conversion, validation, matvec and transpose.
 * @details C11 single-header implementation; define MULIB_IMPL_NUM_CSR in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_NUM_CSR_H
#define MULIB_NUM_CSR_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-num-sparse.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_num_csr {
    size_t rows, cols, nnz;
    size_t *offset, *column;
    double *values;
} mulib_num_csr_t;
int mulib_num_csr_from_triplets(const mulib_num_sparse_t *s, mulib_num_csr_t *out);
int mulib_num_csr_matvec(const mulib_num_csr_t *s, const double *x, double *y);
void mulib_num_csr_free(mulib_num_csr_t *s);
int mulib_num_csr_validate(const mulib_num_csr_t *s);

int mulib_num_csr_transpose(const mulib_num_csr_t *s, mulib_num_csr_t *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_NUM_CSR_H */
```

</details>
<!-- /manual-interface -->

Constructors and conversions require an empty destination; release existing owned storage before replacing it. Public pointer fields must refer to live arrays of the declared sizes. Validation checks logical structure, not the allocation bounds of caller-provided pointers.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-num-csr.h`](../include/mulib-num-csr.h) |
| Implementation switch | `MULIB_IMPL_NUM_CSR` |
| Repository-local includes | [mulib-num-sparse](mulib-num-sparse.md) |
| Complete example | [`examples/modules/num-csr.c`](../examples/modules/num-csr.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/num-csr.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
