# MuLIB fraction

[Manuals](README.md) · [Library index](index.md)

Normalized signed rational numbers with checked arithmetic. The representation uses positive denominators and reduces fractions by GCD. Arithmetic is deliberately conservative on overflow. The addition helper may reject some mathematically valid operations where the intermediate product exceeds signed 64-bit range.

## Contract and behavior

`make(num, den, out)` reduces by the GCD and normalizes to a positive
denominator; zero becomes 0/1. It returns 0 for a zero denominator, NULL output,
or a reduced result not representable in signed 64-bit fields. `add()` expects
positive input denominators and checks intermediate arithmetic. It can fail
when an intermediate does not fit even if a more elaborate reduction would
produce a representable result. Both return 1 on success and preserve output
on failure. Values own no dynamic storage.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_FRACTION
#include "mulib-fraction.h"
int main(void) {
    mulib_fraction_t f;
    assert(mulib_fraction_make(2, 4, &f));
    assert(f.num == 1 && f.den == 2);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-fraction.h`](../include/mulib-fraction.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-fraction.h
 * @brief Normalized signed rational numbers with checked arithmetic.
 * @defgroup mulib_fraction MuLIB fraction
 * @{
 * @details The representation uses positive denominators and reduces fractions
 * by GCD. Arithmetic is deliberately conservative on overflow. The addition
 * helper may reject some mathematically valid operations where the
 * intermediate product exceeds signed 64-bit range.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_FRACTION before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_FRACTION_H
#define MULIB_FRACTION_H
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

  /** @brief Normalized signed rational numbers with checked arithmetic. */
  typedef struct mulib_fraction
  {
    int64_t num, den;
  } mulib_fraction_t;
  /** @brief mulib fraction make; see manual/mulib-fraction.md for contracts
   * and examples. */
  int mulib_fraction_make (int64_t numerator, int64_t denominator,
                           mulib_fraction_t *out);
  /** @brief mulib fraction add; see manual/mulib-fraction.md for contracts and
   * examples. */
  int mulib_fraction_add (mulib_fraction_t a, mulib_fraction_t b,
                          mulib_fraction_t *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_FRACTION_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-fraction.h`](../include/mulib-fraction.h) |
| Implementation switch | `MULIB_IMPL_FRACTION` |
| Repository-local includes | None |
| Complete example | [`examples/modules/fraction.c`](../examples/modules/fraction.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/fraction.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
