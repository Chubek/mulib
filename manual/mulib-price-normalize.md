# price-normalize

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Checked tax, discount, shipping and per-unit offer normalization.

## Contract and supported profile

Normalization discounts the base (0..10000 basis points), adds tax unless
already included (0..100000 basis points), then shipping, then divides by positive
quantity. Each scale step rounds half away from zero in minor units. Base and
shipping must be nonnegative, valid and in the same currency/precision. Total and
per-unit outputs must be distinct; both are unchanged on failure. This is an
explicit arithmetic recipe, not jurisdiction-specific tax calculation.

## Integration and ownership

Include `include/mulib-price-normalize.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_MONEY`, `MULIB_IMPL_PRICE_NORMALIZE`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/price-normalize.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_pricing.c`](../tests/test_web_pricing.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-price-normalize.h`](../include/mulib-price-normalize.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-price-normalize.h
 * @brief Checked tax, discount, shipping and per-unit offer normalization.
 * C11 single-header module. Define MULIB_IMPL_PRICE_NORMALIZE in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_PRICE_NORMALIZE_H
#define MULIB_PRICE_NORMALIZE_H
#include "mulib-money.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* Discount 0..10000 bp, tax 0..100000 bp; quantity is positive. */
  typedef struct mulib_price_normalize_input
  {
    mulib_money_t base, shipping;
    uint32_t discount_bp, tax_bp, quantity;
    int base_includes_tax;
  } mulib_price_normalize_input_t;
  /** @brief See manual/mulib-price-normalize.md for this operation's contract.
   */
  int mulib_price_normalize (const mulib_price_normalize_input_t *in,
                             mulib_money_t *total, mulib_money_t *per_unit);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_NORMALIZE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-price-normalize.h`](../include/mulib-price-normalize.h) |
| Implementation switch | `MULIB_IMPL_PRICE_NORMALIZE` |
| Repository-local includes | [mulib-money](mulib-money.md) |
| Complete example | [`examples/modules/price-normalize.c`](../examples/modules/price-normalize.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/price-normalize.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
