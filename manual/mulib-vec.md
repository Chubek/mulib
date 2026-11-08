# MuLIB vec

[Manuals](README.md) · [Library index](index.md)

An untyped, byte-stride dynamic vector with amortized growth. Elements are copied as raw bytes. Store trivially copyable C objects and manage any resources inside elements yourself.

## Contract and behavior

Initialize fresh storage with a nonzero byte stride. Reserve changes
capacity, not length; push copies one element and supports copying from live
vector storage across growth. Reserve/push return 1 on success, 0 on invalid
state, overflow, or allocation failure. Failed growth preserves the old vector.
At returns a borrowed element pointer or NULL outside length; growth invalidates
borrowed pointers.

Pop removes the last element and returns 0 when empty; NULL output discards it.
Clear sets length to zero without freeing storage. Free releases storage and
retains the stride, permitting reuse. Bytes have no nested destructors. Push
is amortized constant-time for fixed stride; indexing/pop/clear are constant-time.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_VEC
#include "mulib-vec.h"
int main(void) {
    mulib_vec_t v;
    mulib_vec_init(&v, sizeof(int));
    int x = 42;
    assert(mulib_vec_push(&v, &x));
    assert(*(int *)mulib_vec_at(&v, 0) == 42);
    mulib_vec_free(&v);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-vec.h`](../include/mulib-vec.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-vec.h
 * @brief An untyped, byte-stride dynamic vector with amortized growth.
 * @defgroup mulib_vec MuLIB vec
 * @{
 * @details Elements are copied as raw bytes. Store trivially copyable C
 * objects and manage any resources inside elements yourself.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_VEC before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_VEC_H
#define MULIB_VEC_H
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

  /** @brief An untyped, byte-stride dynamic vector with amortized growth. */
  typedef struct mulib_vec
  {
    unsigned char *data;
    size_t length, capacity, stride;
  } mulib_vec_t;
  /** @brief mulib vec init; see manual/mulib-vec.md for contracts and
   * examples. */
  void mulib_vec_init (mulib_vec_t *v, size_t element_size);
  /** @brief mulib vec reserve; see manual/mulib-vec.md for contracts and
   * examples. */
  int mulib_vec_reserve (mulib_vec_t *v, size_t elements);
  /** @brief Copy one element, including an element borrowed from this vector.
   * Growth rebases internal sources; failure preserves existing
   * elements/length.
   */
  int mulib_vec_push (mulib_vec_t *v, const void *element);
  /** @brief mulib vec at; see manual/mulib-vec.md for contracts and examples.
   */
  void *mulib_vec_at (mulib_vec_t *v, size_t index);
  /** @brief mulib vec pop; see manual/mulib-vec.md for contracts and examples.
   */
  int mulib_vec_pop (mulib_vec_t *v, void *out_element);
  /** @brief mulib vec clear; see manual/mulib-vec.md for contracts and
   * examples. */
  void mulib_vec_clear (mulib_vec_t *v);
  /** @brief mulib vec free; see manual/mulib-vec.md for contracts and
   * examples. */
  void mulib_vec_free (mulib_vec_t *v);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_VEC_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-vec.h`](../include/mulib-vec.h) |
| Implementation switch | `MULIB_IMPL_VEC` |
| Repository-local includes | None |
| Complete example | [`examples/modules/vec.c`](../examples/modules/vec.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/vec.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
