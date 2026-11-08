# MuLIB interpolate

[Manuals](README.md) · [Library index](index.md)

Scalar linear interpolation and clamped interpolation. Parameter t=0 returns the first endpoint and t=1 returns the second; clamped interpolation limits t to [0,1].

## Contract and behavior

`linear(a,b,t)` returns a at t=0 and b at t=1, and extrapolates for t
outside 0..1. `clamped()` first clamps t to that interval. The implementation
chooses a weighted expression for endpoints of opposite sign to avoid a needless
overflow in `b-a`. This is still double-precision arithmetic, with no separate
error reporting or finite-input validation. There is no allocation or state.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_INTERPOLATE
#include "mulib-interpolate.h"
int main(void) {
    assert(mulib_interpolate_linear(0, 10, 0.5) == 5);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-interpolate.h`](../include/mulib-interpolate.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-interpolate.h
 * @brief Scalar linear interpolation and clamped interpolation.
 * @defgroup mulib_interpolate MuLIB interpolate
 * @{
 * @details Parameter t=0 returns the first endpoint and t=1 returns the
 * second; clamped interpolation limits t to [0,1].
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_INTERPOLATE before
 * including this file. Other translation units may include it without the
 * implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_INTERPOLATE_H
#define MULIB_INTERPOLATE_H
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

  /** @brief mulib interpolate linear; see manual/mulib-interpolate.md for
   * contracts and examples. */
  double mulib_interpolate_linear (double a, double b, double t);
  /** @brief mulib interpolate clamped; see manual/mulib-interpolate.md for
   * contracts and examples. */
  double mulib_interpolate_clamped (double a, double b, double t);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_INTERPOLATE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-interpolate.h`](../include/mulib-interpolate.h) |
| Implementation switch | `MULIB_IMPL_INTERPOLATE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/interpolate.c`](../examples/modules/interpolate.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/interpolate.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
