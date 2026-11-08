# MuLIB prime

[Manuals](README.md) · [Library index](index.md)

Primality testing and next-prime search for 64-bit integers. Deterministic
Miller-Rabin covers the full uint64_t range with bases 2, 325, 9375, 28178,
450775, 9780504 and 1795265022. Modular multiplication uses checked add/double
arithmetic without compiler-specific 128-bit types. No cryptographic or
constant-time guarantees are provided.

## Contract and behavior

`is_prime()` is a Boolean predicate using deterministic Miller–Rabin
bases for the complete `uint64_t` domain; values below 2 are not prime.
`next(n)` returns the smallest prime **greater than or equal to n**, returning
2 for n≤2 and zero when no representable candidate remains. Modular arithmetic
avoids requiring a nonstandard 128-bit integer. There is no allocation, random
seed, or probabilistic error within the documented integer domain.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_PRIME
#include "mulib-prime.h"
int main(void) {
    assert(mulib_prime_is_prime(97));
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-prime.h`](../include/mulib-prime.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-prime.h
 * @brief Primality testing and next-prime search for 64-bit integers.
 * @defgroup mulib_prime MuLIB prime
 * @{
 * @details Deterministic Miller-Rabin covers the full uint64_t domain using
 * portable overflow-safe modular arithmetic. This module is not a
 * cryptographic or constant-time prime generator.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_PRIME before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_PRIME_H
#define MULIB_PRIME_H
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

  /** @brief Deterministic uint64_t primality via seven Miller-Rabin bases; not
   * constant-time. */
  int mulib_prime_is_prime (uint64_t number);
  /** @brief mulib prime next; see manual/mulib-prime.md for contracts and
   * examples. */
  uint64_t mulib_prime_next (uint64_t number);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRIME_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-prime.h`](../include/mulib-prime.h) |
| Implementation switch | `MULIB_IMPL_PRIME` |
| Repository-local includes | None |
| Complete example | [`examples/modules/prime.c`](../examples/modules/prime.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/prime.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
