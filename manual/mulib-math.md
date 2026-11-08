# MuLIB math

[Manuals](README.md) · [Library index](index.md)

Integer arithmetic utilities: GCD, LCM and bounded integer power. GCD(0,0) returns 0. Overflow-sensitive functions return zero without writing their output when multiplication would overflow.

## Contract and behavior

GCD and LCM operate on unsigned 64-bit values. `gcd(0,0)` is zero;
LCM is zero if either input is zero. `pow_u64()` uses checked exponentiation
by squaring; exponent zero yields one, including 0⁰. LCM and power return 1
on success and 0 on overflow or NULL output, preserving output on failure.
GCD returns the value directly. No storage is allocated or retained.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_MATH
#include "mulib-math.h"
int main(void) {
    assert(mulib_math_gcd(12, 20) == 4);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-math.h`](../include/mulib-math.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-math.h
 * @brief Integer arithmetic utilities: GCD, LCM and bounded integer power.
 * @defgroup mulib_math MuLIB math
 * @{
 * @details GCD(0,0) returns 0. Overflow-sensitive functions return zero
 * without writing their output when multiplication would overflow.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_MATH before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_MATH_H
#define MULIB_MATH_H
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

  /** @brief mulib math gcd; see manual/mulib-math.md for contracts and
   * examples. */
  uint64_t mulib_math_gcd (uint64_t a, uint64_t b);
  /** @brief mulib math lcm; see manual/mulib-math.md for contracts and
   * examples. */
  int mulib_math_lcm (uint64_t a, uint64_t b, uint64_t *out);
  /** @brief mulib math pow u64; see manual/mulib-math.md for contracts and
   * examples. */
  int mulib_math_pow_u64 (uint64_t base, unsigned exponent, uint64_t *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_MATH_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-math.h`](../include/mulib-math.h) |
| Implementation switch | `MULIB_IMPL_MATH` |
| Repository-local includes | None |
| Complete example | [`examples/modules/math.c`](../examples/modules/math.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/math.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
