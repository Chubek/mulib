# price-demand

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Bounded linear demand estimation with closed-form ridge regularization.

## Contract and supported profile

Fit estimates an intercept and slope by centered least squares with a
nonnegative ridge penalty on the slope, not the intercept. At least two finite
nonnegative price/unit pairs are required; singular unregularized fits fail.
R-squared is 1-SSE/SST (one for a constant response). Fit commits only finite
results. Prediction returns max(0, intercept+slope*price), or NAN on invalid
input/nonfinite result. There are no causal, elasticity or out-of-sample claims.

## Integration and ownership

Include `include/mulib-price-demand.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_PRICE_DEMAND`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/price-demand.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_pricing.c`](../tests/test_web_pricing.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-price-demand.h`](../include/mulib-price-demand.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-price-demand.h
 * @brief Bounded linear demand estimation with closed-form ridge
 * regularization. C11 single-header module. Define MULIB_IMPL_PRICE_DEMAND in
 * exactly one C translation unit.
 *
 */
#ifndef MULIB_PRICE_DEMAND_H
#define MULIB_PRICE_DEMAND_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_price_demand_model
  {
    double intercept, slope, r_squared;
  } mulib_price_demand_model_t;
  /** @brief See manual/mulib-price-demand.md for this operation's contract. */
  int mulib_price_demand_fit (const double *prices, const double *units,
                              size_t n, double ridge,
                              mulib_price_demand_model_t *out);
  /** @brief See manual/mulib-price-demand.md for this operation's contract. */
  double mulib_price_demand_predict (const mulib_price_demand_model_t *model,
                                     double price);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_DEMAND_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-price-demand.h`](../include/mulib-price-demand.h) |
| Implementation switch | `MULIB_IMPL_PRICE_DEMAND` |
| Repository-local includes | None |
| Complete example | [`examples/modules/price-demand.c`](../examples/modules/price-demand.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/price-demand.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
