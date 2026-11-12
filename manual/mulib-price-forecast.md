# price-forecast

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Uniform forecasting API with naive, EMA, drift and seasonal baselines.

## Contract and supported profile

Forecasts consume finite nonnegative uniformly spaced samples. Horizon is
1..1000000. Naive repeats the last value; EMA uses alpha in (0,1]; drift needs two
points and extrapolates endpoint slope, clamped at zero; seasonal repeats the
last complete season (1..n). MAE uses rolling one-step predictions starting at
index two, or season for seasonal models, with n>=3; it is O(n squared) for models
that rescan each prefix. Unsupported model, invalid samples or nonfinite output
fail without changing the scalar output. These are baselines, not trained models.

## Integration and ownership

Include `include/mulib-price-forecast.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_MONEY`, `MULIB_IMPL_PRICE_FORECAST`, `MULIB_IMPL_PRICE_HISTORY`, `MULIB_IMPL_PRICE_OBSERVATION`, `MULIB_IMPL_PRICE_TIMESERIES`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/price-forecast.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_pricing.c`](../tests/test_web_pricing.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-price-forecast.h`](../include/mulib-price-forecast.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-price-forecast.h
 * @brief Uniform forecasting API with naive, EMA, drift and seasonal
 * baselines. C11 single-header module. Define MULIB_IMPL_PRICE_FORECAST in
 * exactly one C translation unit.
 *
 */
#ifndef MULIB_PRICE_FORECAST_H
#define MULIB_PRICE_FORECAST_H
#include "mulib-price-timeseries.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_price_forecast_model
  {
    MULIB_PRICE_FORECAST_NAIVE,
    MULIB_PRICE_FORECAST_EMA,
    MULIB_PRICE_FORECAST_DRIFT,
    MULIB_PRICE_FORECAST_SEASONAL
  } mulib_price_forecast_model_t;
  /** @brief See manual/mulib-price-forecast.md for this operation's contract.
   */
  int mulib_price_forecast (const double *series, size_t n, size_t horizon,
                            mulib_price_forecast_model_t model, double alpha,
                            size_t season, double *out);
  /** @brief See manual/mulib-price-forecast.md for this operation's contract.
   */
  int mulib_price_forecast_mae (const double *series, size_t n,
                                mulib_price_forecast_model_t model,
                                double alpha, size_t season, double *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_FORECAST_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-price-forecast.h`](../include/mulib-price-forecast.h) |
| Implementation switch | `MULIB_IMPL_PRICE_FORECAST` |
| Repository-local includes | [mulib-price-timeseries](mulib-price-timeseries.md) |
| Complete example | [`examples/modules/price-forecast.c`](../examples/modules/price-forecast.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/price-forecast.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
