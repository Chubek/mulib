# MuLIB complex

[Manuals](README.md) · [Library index](index.md)

Complex arithmetic for double precision real and imaginary components. This type is a plain struct intended for interoperability with C programs that do not rely on C99 _Complex. Use the platform libm for hypot.

## Contract and behavior

Complex numbers are value structures with double `real` and `imag`
members. Addition and multiplication use ordinary floating-point arithmetic;
magnitude uses `hypot(real, imag)`. The API allocates nothing, has no ownership
transfer, and returns values directly. There is no separate status for NaN,
infinity, or arithmetic overflow. Link the system math library where required.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_COMPLEX
#include "mulib-complex.h"
int main(void) {
    mulib_complex_t a = {1, 2}, b = {3, 4}, c = mulib_complex_add(a, b);
    assert(c.real == 4);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-complex.h`](../include/mulib-complex.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-complex.h
 * @brief Complex arithmetic for double precision real and imaginary
 * components.
 * @defgroup mulib_complex MuLIB complex
 * @{
 * @details This type is a plain struct intended for interoperability with C
 * programs that do not rely on C99 _Complex. Use the platform libm for hypot.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_COMPLEX before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_COMPLEX_H
#define MULIB_COMPLEX_H
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

  /** @brief Complex arithmetic for double precision real and imaginary
   * components. */
  typedef struct mulib_complex
  {
    double real, imag;
  } mulib_complex_t;
  /** @brief mulib complex add; see manual/mulib-complex.md for contracts and
   * examples. */
  mulib_complex_t mulib_complex_add (mulib_complex_t a, mulib_complex_t b);
  /** @brief mulib complex mul; see manual/mulib-complex.md for contracts and
   * examples. */
  mulib_complex_t mulib_complex_mul (mulib_complex_t a, mulib_complex_t b);
  /** @brief mulib complex abs; see manual/mulib-complex.md for contracts and
   * examples. */
  double mulib_complex_abs (mulib_complex_t z);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_COMPLEX_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-complex.h`](../include/mulib-complex.h) |
| Implementation switch | `MULIB_IMPL_COMPLEX` |
| Repository-local includes | None |
| Complete example | [`examples/modules/complex.c`](../examples/modules/complex.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/complex.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
