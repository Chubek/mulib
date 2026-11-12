# price-observation

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Validated price, product, merchant, market, time and evidence record.

## Contract and supported profile

Records are copied value objects with fixed-capacity terminated strings.
Validation requires nonempty product/merchant, nonnegative price/time, valid
money metadata and only the three documented stock/promotion/tax bits. Empty
variant, URL and market are allowed; URL is metadata, not an authorization.
`same_offer` validates both records and compares product, merchant, variant,
market, currency and precision exactly, not URL, flags or observation time.

## Integration and ownership

Include `include/mulib-price-observation.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_MONEY`, `MULIB_IMPL_PRICE_OBSERVATION`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/price-observation.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_pricing.c`](../tests/test_web_pricing.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-price-observation.h`](../include/mulib-price-observation.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-price-observation.h
 * @brief Validated price, product, merchant, market, time and evidence record.
 * C11 single-header module. Define MULIB_IMPL_PRICE_OBSERVATION in exactly one
 * C translation unit.
 *
 */
#ifndef MULIB_PRICE_OBSERVATION_H
#define MULIB_PRICE_OBSERVATION_H
#include "mulib-money.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_price_observation
  {
    char product[128], merchant[128], variant[64], url[512], market[16];
    mulib_money_t price;
    int64_t observed_at;
    uint64_t source_hash;
    unsigned
        flags; /* bit 0: in stock; bit 1: promotion; bit 2: includes tax */
  } mulib_price_observation_t;
  /** @brief See manual/mulib-price-observation.md for this operation's
   * contract. */
  int mulib_price_observation_validate (const mulib_price_observation_t *o);
  /** @brief See manual/mulib-price-observation.md for this operation's
   * contract. */
  int mulib_price_observation_same_offer (const mulib_price_observation_t *a,
                                          const mulib_price_observation_t *b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_OBSERVATION_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-price-observation.h`](../include/mulib-price-observation.h) |
| Implementation switch | `MULIB_IMPL_PRICE_OBSERVATION` |
| Repository-local includes | [mulib-money](mulib-money.md) |
| Complete example | [`examples/modules/price-observation.c`](../examples/modules/price-observation.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/price-observation.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
