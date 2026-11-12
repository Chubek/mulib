# money

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Checked integer-minor-unit monetary arithmetic and rational conversions.

## Contract and supported profile

Money stores signed 64-bit minor units, an uppercase three-letter currency and
0..6 decimal places. `make`/`valid` validate metadata; add/sub require identical
metadata and reject overflow. `scale` accepts a nonnegative numerator and positive
denominator and uses exact integer arithmetic with ties rounded away from zero,
including INT64_MIN. Decimal parsing accepts an optional sign, required integer
digits, and optional nonempty fractional digits up to the configured precision;
no whitespace, exponent, separators or silent rounding. Value outputs remain
unchanged on failure. No exchange-rate or ISO currency registry is implied.

## Integration and ownership

Include `include/mulib-money.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_MONEY`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

[`test_money_reference.py`](../tests/test_money_reference.py) additionally compares
more than ten thousand full-width signed-money/rational cases against Python's
exact integer arithmetic, including overflow rejection and INT64_MIN.

The complete [offline example](../examples/modules/money.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_pricing.c`](../tests/test_web_pricing.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-money.h`](../include/mulib-money.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-money.h
 * @brief Checked integer-minor-unit monetary arithmetic and rational
 * conversions. C11 single-header module. Define MULIB_IMPL_MONEY in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_MONEY_H
#define MULIB_MONEY_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_money
  {
    int64_t minor;
    char currency[4];
    uint8_t decimals;
  } mulib_money_t;
  /** @brief Validate an uppercase three-letter currency and 0..6 decimal
   * places. */
  int mulib_money_valid (mulib_money_t money);
  /** @brief See manual/mulib-money.md for this operation's contract. */
  int mulib_money_make (mulib_money_t *out, int64_t minor,
                        const char *currency, uint8_t decimals);
  /** @brief See manual/mulib-money.md for this operation's contract. */
  int mulib_money_add (mulib_money_t a, mulib_money_t b, mulib_money_t *out);
  /** @brief See manual/mulib-money.md for this operation's contract. */
  int mulib_money_sub (mulib_money_t a, mulib_money_t b, mulib_money_t *out);
  /** @brief See manual/mulib-money.md for this operation's contract. */
  int mulib_money_scale (mulib_money_t a, int64_t num, int64_t den,
                         mulib_money_t *out);
  /** @brief See manual/mulib-money.md for this operation's contract. */
  int mulib_money_parse (const char *decimal, const char *currency,
                         uint8_t digits, mulib_money_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MONEY_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-money.h`](../include/mulib-money.h) |
| Implementation switch | `MULIB_IMPL_MONEY` |
| Repository-local includes | None |
| Complete example | [`examples/modules/money.c`](../examples/modules/money.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/money.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
