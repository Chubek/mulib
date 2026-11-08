# MuLIB matrix

[Manuals](README.md) · [Library index](index.md)

Heap-allocated row-major matrix of double precision numbers. Matrix multiplication uses straightforward O(rows × shared dimension × columns) arithmetic with no BLAS dependency. Initialize destination as an unowned empty object; free it after use.

## Contract and behavior

Matrices own zero-filled row-major doubles. `init()` returns 1 on
success, 0 on invalid sizing/allocation failure; zero dimensions are accepted.
Multiply requires `a.cols == b.rows`; transpose swaps dimensions. Both allocate
a new result and reject output that is the same structure as an input.

Output must not already own an allocation: successful multiply/transpose
overwrites its fields without freeing old storage. On failure output is
unchanged. Avoid shallow copies sharing a data pointer. `free()` releases data
and zeros state. Arithmetic uses ordinary doubles without finite-value checks;
multiply is O(rows(a) * cols(a) * cols(b)).

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_MATRIX
#include "mulib-matrix.h"
int main(void) {
    mulib_matrix_t m;
    assert(mulib_matrix_init(&m, 2, 2));
    m.data[0] = 5;
    assert(m.data[0] == 5);
    mulib_matrix_free(&m);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-matrix.h`](../include/mulib-matrix.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-matrix.h
 * @brief Heap-allocated row-major matrix of double precision numbers.
 * @defgroup mulib_matrix MuLIB matrix
 * @{
 * @details Matrix multiplication uses straightforward O(rows × shared
 * dimension × columns) arithmetic with no BLAS dependency. Initialize
 * destination as an unowned empty object; free it after use.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_MATRIX before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_MATRIX_H
#define MULIB_MATRIX_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Heap-allocated row-major matrix of double precision numbers. */
  typedef struct mulib_matrix
  {
    double *data;
    size_t rows, cols;
  } mulib_matrix_t;
  /** @brief mulib matrix init; see manual/mulib-matrix.md for contracts and
   * examples. */
  int mulib_matrix_init (mulib_matrix_t *m, size_t rows, size_t cols);
  /** @brief mulib matrix multiply; see manual/mulib-matrix.md for contracts
   * and examples. */
  int mulib_matrix_multiply (const mulib_matrix_t *a, const mulib_matrix_t *b,
                             mulib_matrix_t *out);
  /** @brief mulib matrix transpose; see manual/mulib-matrix.md for contracts
   * and examples. */
  int mulib_matrix_transpose (const mulib_matrix_t *m, mulib_matrix_t *out);
  /** @brief mulib matrix free; see manual/mulib-matrix.md for contracts and
   * examples. */
  void mulib_matrix_free (mulib_matrix_t *m);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_MATRIX_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-matrix.h`](../include/mulib-matrix.h) |
| Implementation switch | `MULIB_IMPL_MATRIX` |
| Repository-local includes | None |
| Complete example | [`examples/modules/matrix.c`](../examples/modules/matrix.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/matrix.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
