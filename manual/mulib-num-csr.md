# mulib-num-csr

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-num-csr.h`  
**Implementation switch:** `MULIB_IMPL_NUM_CSR`  
**Dependencies:** `num-sparse`; `amb3r-base` (static inline, no additional link dependency).

## Purpose and supported profile

Compressed sparse row conversion, validation, matvec and transpose. Triplet
conversion and transpose return rows sorted by column index. The CSR validator
also accepts unsorted rows, repeated column indices, and explicit zero
coefficients. Matvec accumulates all stored entries in row storage order, permits
aliased input/output, and preserves output on failure. It takes `cols` readable
and `rows` writable `double` elements for `x` and `y`, respectively (neither
pointer may be null, even for an empty dimension).

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

### Checked Amb3r adapters

Both representations use zero-based `size_t` rows, columns, and offsets,
and finite `double` coefficients. The adapters take the readable element counts
of the three source arrays separately, **not byte lengths**. Each source
requires at least `rows + 1` offsets, including for a zero-row matrix, and at
least `nnz` column indices and values. An entry pointer may be null only when
its supplied count is zero; offsets must not be null. Counts whose byte sizes
would overflow `size_t` are rejected. Offsets must start at zero, be
nondecreasing, and end at `nnz`; each column index must be less than `cols`.
Trailing capacity beyond the logical array lengths is not inspected. The
caller vouches for allocation, alignment, and liveness of the supplied spans.

- `mulib_num_csr_from_amb3r` deep-copies to a zero-initialized `mulib_num_csr_t`.
  It allocates one offset array and, for `nnz > 0`, one column array and one
  coefficient array. The result is independent of the source; release the
  result with `mulib_num_csr_free`.
- `mulib_num_csr_borrow_amb3r` publishes a read-only `ma_csr` view with the source
  pointers and no allocation. The owner remains responsible for releasing the
  storage. Keep the arrays alive and unmodified until the last use of the view;
  destroying, reallocating, or modifying owner storage invalidates the borrow.

The destination object must be writable and disjoint from the source
descriptor and arrays. The source must remain unmodified during the call. Each
operation is O(`rows + nnz`) and returns `1` on success and `0` on error with
the output unchanged. Neither operation converts units, sorts, combines
duplicates, nor drops zeros. This preserves floating-point accumulation
order across the bridge itself, not across different numerical algorithms.
In particular, a valid CSR borrow is not proof of invertibility, sufficient
pivot magnitude, or a usable preconditioner. Amb3r sparse LU accepts a square
view, accumulates duplicates into its own factors, and does not retain the
borrow after factoring.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_NUM_CSR` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_NUM_CSR
#include "mulib-num-csr.h"
```

## Focused regression test

`tests/test_num_csr.c` checks canonical matvec and transpose against explicit
oracles, solves a pivoted nonsymmetric system via Amb3r sparse LU and checks the
solution and analytical residual, and covers malformed lengths, numerical
failures, ownership, and each copy allocation failure. The Amb3r suite also
exercises both adapter symbols in a separately compiled C++ consumer.

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
#include "mulib-amb3r-base.h"
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

/* Counts are readable ELEMENTS in each source array, not bytes:
 * at least rows+1 offsets, nnz columns and nnz values. Offset storage is
 * required even for zero rows; column/value pointers may be NULL only with
 * zero corresponding counts (and thus nnz=0).
 * Indices are zero-based; values are finite double coefficients with no unit
 * conversion. Both adapters preserve row order, duplicates and explicit zeros.
 * Source arrays must be live and unmodified during validation/copy; out must
 * be writable and disjoint from the source object and arrays. Return 1 on
 * success, 0 on invalid input or allocation failure, leaving *out unchanged. */
/* Copies the source into a zero-initialized destination. Free only the copy
 * with mulib_num_csr_free; the source keeps its original ownership. */
int mulib_num_csr_from_amb3r(const ma_csr *s, size_t offset_count, size_t column_count,
                            size_t value_count, mulib_num_csr_t *out);
/* Allocation-free read-only borrow. Keep the source arrays alive and immutable
 * until the last use of the borrow; never free through the view. */
int mulib_num_csr_borrow_amb3r(const mulib_num_csr_t *s, size_t offset_count,
                              size_t column_count, size_t value_count, ma_csr *out);

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
| Repository-local includes | [mulib-amb3r-base](amb3r-kernels.md), [mulib-num-sparse](mulib-num-sparse.md) |
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
