# MuLIB popcount

[Manuals](README.md) · [Library index](index.md)

Bit population count, leading zeros, and trailing zeros for 64-bit words. Portable fallback routines that avoid compiler-specific intrinsics. Zero is defined to have 64 leading/trailing zero bits.

## Contract and behavior

All three helpers operate on `uint64_t`. `u64()` counts set bits;
`clz()` counts leading zeros from bit 63; `ctz()` counts trailing zeros from
bit 0. Both zero-count functions explicitly return **64** for input zero.
Results are unsigned counts, not statuses. Implementations allocate nothing
and do not require compiler-specific intrinsics.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_POPCOUNT
#include "mulib-popcount.h"
int main(void) {
    assert(mulib_popcount_u64(15) == 4);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-popcount.h`](../include/mulib-popcount.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-popcount.h
 * @brief Bit population count, leading zeros, and trailing zeros for 64-bit
 * words.
 * @defgroup mulib_popcount MuLIB popcount
 * @{
 * @details Portable fallback routines that avoid compiler-specific intrinsics.
 * Zero is defined to have 64 leading/trailing zero bits.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_POPCOUNT before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_POPCOUNT_H
#define MULIB_POPCOUNT_H
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

  /** @brief mulib popcount u64; see manual/mulib-popcount.md for contracts and
   * examples. */
  unsigned mulib_popcount_u64 (uint64_t x);
  /** @brief mulib popcount clz; see manual/mulib-popcount.md for contracts and
   * examples. */
  unsigned mulib_popcount_clz (uint64_t x);
  /** @brief mulib popcount ctz; see manual/mulib-popcount.md for contracts and
   * examples. */
  unsigned mulib_popcount_ctz (uint64_t x);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_POPCOUNT_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-popcount.h`](../include/mulib-popcount.h) |
| Implementation switch | `MULIB_IMPL_POPCOUNT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/popcount.c`](../examples/modules/popcount.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/popcount.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
