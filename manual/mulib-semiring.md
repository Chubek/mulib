# semiring — boolean, natural and tropical arithmetic

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_SEMIRING`; link `-lm`.

BOOLEAN uses logical OR/AND, zero 0 and one 1; nonzero numbers are true.
NATURAL accepts nonnegative integral doubles and +infinity, uses addition and
multiplication, and preserves zero absorption even with infinity. Overflow
saturates to +infinity; integers above 2^53 may round, so exact algebraic laws
are not guaranteed over arbitrary floating representations.

MIN_PLUS uses min/add, zero +infinity and one 0. MAX_PLUS uses max/add, zero
-infinity and one 0. Their zero elements absorb multiplication, including
opposite infinities. Invalid kinds, NaN operands and negative/fractional natural
operands return NaN. Operations are O(1), allocation-free.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-semiring.h`](../include/mulib-semiring.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Boolean, nonnegative integral-double and tropical semirings.
 * Natural overflow saturates to +infinity; integers above 2^53 round.
 * NaN, invalid kinds and negative/fractional natural operands return NaN. */
#ifndef MULIB_SEMIRING_H
#define MULIB_SEMIRING_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum
  {
    MULIB_SEMIRING_BOOLEAN,
    MULIB_SEMIRING_NATURAL,
    MULIB_SEMIRING_MIN_PLUS,
    MULIB_SEMIRING_MAX_PLUS
  } mulib_semiring_kind_t;
  double mulib_semiring_zero (mulib_semiring_kind_t kind);
  double mulib_semiring_one (mulib_semiring_kind_t kind);
  double mulib_semiring_add (mulib_semiring_kind_t kind, double a, double b);
  double mulib_semiring_mul (mulib_semiring_kind_t kind, double a, double b);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-semiring.h`](../include/mulib-semiring.h) |
| Implementation switch | `MULIB_IMPL_SEMIRING` |
| Repository-local includes | None |
| Complete example | [`examples/modules/semiring.c`](../examples/modules/semiring.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/semiring.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
