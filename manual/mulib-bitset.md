# MuLIB bitset

[Manuals](README.md) · [Library index](index.md)

Fixed-size allocated bit-array with test, set, clear, and population count.

## Contract and behavior

Initialize fresh storage with the logical bit count; zero bits is
valid. `init()` and `set()` return 1 on success, 0 on failure. `test()` returns
0 for both a clear bit and an invalid index. `count()` scans the allocated bytes;
leave unused high bits in the last byte clear rather than modifying them
directly. Bit indices are zero-based and packed low-bit-first. `free()` releases
the byte array and clears the state. Release an existing allocation before init.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_BITSET
#include "mulib-bitset.h"
int main(void) {
    mulib_bitset_t b;
    assert(mulib_bitset_init(&b, 100));
    assert(mulib_bitset_set(&b, 42, 1));
    assert(mulib_bitset_test(&b, 42));
    mulib_bitset_free(&b);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-bitset.h`](../include/mulib-bitset.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-bitset.h
 * @brief Fixed-size allocated bit-array with test, set, clear, and population count.
 * @defgroup mulib_bitset MuLIB bitset
 * @{
 * @details Bit indices start at zero. Unused high bits in the last byte stay
 * zero because allocation is zero-filled.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_BITSET before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_BITSET_H
#define MULIB_BITSET_H
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

  /** @brief Fixed-size allocated bit-array with test, set, clear, and population count.
   */
  typedef struct mulib_bitset
  {
    unsigned char *bytes;
    size_t bits;
  } mulib_bitset_t;
  /** @brief mulib bitset init; see manual/mulib-bitset.md for contracts and
   * examples. */
  int mulib_bitset_init (mulib_bitset_t *b, size_t bits);
  /** @brief mulib bitset test; see manual/mulib-bitset.md for contracts and
   * examples. */
  int mulib_bitset_test (const mulib_bitset_t *b, size_t i);
  /** @brief mulib bitset set; see manual/mulib-bitset.md for contracts and
   * examples. */
  int mulib_bitset_set (mulib_bitset_t *b, size_t i, int value);
  /** @brief mulib bitset count; see manual/mulib-bitset.md for contracts and
   * examples. */
  size_t mulib_bitset_count (const mulib_bitset_t *b);
  /** @brief mulib bitset free; see manual/mulib-bitset.md for contracts and
   * examples. */
  void mulib_bitset_free (mulib_bitset_t *b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_BITSET_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-bitset.h`](../include/mulib-bitset.h) |
| Implementation switch | `MULIB_IMPL_BITSET` |
| Repository-local includes | None |
| Complete example | [`examples/modules/bitset.c`](../examples/modules/bitset.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bitset.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
