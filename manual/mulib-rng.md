# MuLIB rng

[Manuals](README.md) · [Library index](index.md)

Deterministic 64-bit SplitMix generator; not cryptographic. This generator gives reproducible pseudorandom streams. Never use it for passwords, secrets, keys, or secure UUIDs.

## Contract and behavior

State is caller-owned. Seed accepts every 64-bit value, including zero;
`next()` advances SplitMix64 and returns a deterministic 64-bit value. Equal
seeds reproduce equal sequences. `unit()` uses 53 high bits to return a double
in **[0,1)**. A NULL generator yields zero. There is no OS entropy acquisition,
allocation, or internal synchronization. This generator is for reproducible
simulation/testing, not cryptographic keys or unpredictable tokens.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_RNG
#include "mulib-rng.h"
int main(void) {
    mulib_rng_t r;
    mulib_rng_seed(&r, 5);
    uint64_t n = mulib_rng_next(&r);
    assert(n != 0);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-rng.h`](../include/mulib-rng.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-rng.h
 * @brief Deterministic 64-bit SplitMix generator; not cryptographic.
 * @defgroup mulib_rng MuLIB rng
 * @{
 * @details This generator gives reproducible pseudorandom streams. Never use
 * it for passwords, secrets, keys, or secure UUIDs.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_RNG before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_RNG_H
#define MULIB_RNG_H
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

  /** @brief Deterministic 64-bit SplitMix generator; not cryptographic. */
  typedef struct mulib_rng
  {
    uint64_t state;
  } mulib_rng_t;
  /** @brief mulib rng seed; see manual/mulib-rng.md for contracts and
   * examples. */
  void mulib_rng_seed (mulib_rng_t *r, uint64_t seed);
  /** @brief mulib rng next; see manual/mulib-rng.md for contracts and
   * examples. */
  uint64_t mulib_rng_next (mulib_rng_t *r);
  /** @brief mulib rng unit; see manual/mulib-rng.md for contracts and
   * examples. */
  double mulib_rng_unit (mulib_rng_t *r);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_RNG_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-rng.h`](../include/mulib-rng.h) |
| Implementation switch | `MULIB_IMPL_RNG` |
| Repository-local includes | None |
| Complete example | [`examples/modules/rng.c`](../examples/modules/rng.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/rng.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
