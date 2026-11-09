# MuLIB histprice

[Manuals](README.md) · [Library index](index.md) · [Pricing integration](mulib-pricing-suite.md)

Forward forecasts from equally spaced historical prices, using classical and robust time-series models.

## Purpose and contract

Requires `statsproc` only for Theil–Sen. Inputs must be finite, nonnegative prices sorted **oldest to newest** with uniform observation spacing. Forecast horizon is an integer number of observation intervals from the last observation. Most methods output `variance=NAN`, indicating that no predictive variance has been estimated.

All functions return `1` on success and `0` on invalid input, allocation failure,
or a negative/nonfinite prediction. Outputs are unchanged on failure and inputs
are borrowed and never modified. Kalman arithmetic uses long-double intermediates
and the posterior variance `r*gain` to avoid cancellation near unit gain.

## Algorithms and numerical semantics

| Model | Supported behavior and assumptions |
| --- | --- |
| `ema` | Smoothed level with parameter α∈[0,1]; no extrapolated trend |
| `holt` | Damped additive trend with α,β∈[0,1], φ∈(0,1]; initialized from first difference |
| `seasonal` | Additive Holt–Winters, complete ≥2 seasonal cycles, season length ≥2 |
| `kalman` | Scalar local-level state-space filter with observation variance `r`, process variance `q` |
| `ar1` | Univariate stationary AR(1) fitted by centered least squares; requires |slope|<1 |
| `robust_trend` | Theil–Sen median pairwise slope, predict `intercept + slope × (n-1+horizon)` |
| `ema_mae` | Prequential one-step mean absolute error of the EMA (no data leakage) |

**Holt:** Forecast `level + trend*sum(phi^j, j=1..h)`. `phi=1` yields ordinary linear extrapolation. Updates use `new_level=α×observed+(1-α)×(old_level+φ×trend)` and `new_trend=β×(new_level-old_level)+(1-β)×φ×old_trend`.

**Seasonal:** The model is additive and requires `season<=n/2`; initial seasonal effects are deviations from the first-cycle mean; initial trend is the difference between first two cycle means divided by season length. This implementation is deterministic, not maximum-likelihood fitted. Large or sparse seasonal data require better model selection and holdout evaluation.

**Kalman:** State is random walk. Update from predicted variance `v+q` with gain `(v+q)/(v+q+r)`; returned `variance=v+h*q+r` includes future observation noise. If `q=r=0` the model is undefined and rejected.

**AR(1):** Requires >=3 samples and nonconstant lag series. The method rejects nonstationary slopes (|β|>=1) and forecasts converge toward `a/(1-β)`. This is an unconstrained statistical forecast and can return failure on negative extrapolated prices.

**Runtime.** EMA/Holt/Kalman/AR(1) O(n), seasonal O(n+season) time/O(season) space, robust trend O(n² log n)/O(n²) up to 1024 samples. `horizon` must be 1–1,000,000, and predictions must be nonnegative and finite.

## Example

```c
#define MULIB_IMPL_STATSPROC
#define MULIB_IMPL_HISTPRICE
#include "mulib-histprice.h"
#include <assert.h>
int main(void) {
    const double daily[] = {100,101,103,104,107,108};
    mulib_histprice_forecast_t f;
    assert(mulib_histprice_holt(daily, 6, .6, .2, .9, 3, &f));
    assert(f.point >= 0);
    return 0;
}
```

Compile from repository root (using the additions copied into its directories):

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude example.c -lm -o example
```

The `-lm` link flag is for the system C math library, not an external third-party dependency.

## Integration and limitations

Implementation macro: `MULIB_IMPL_HISTPRICE`. Declare it in **one** translation unit before including the module. Required additional MuLIB implementation switches: `statsproc`. Define all such switches before their headers are first included. The implementation must not be emitted in more than one translation unit. The code is ISO C11 and exposes C linkage for C++17 clients. For the full test and multi-translation-unit recipe, see [Pricing integration](mulib-pricing-suite.md).

Backtest on rolling, time-ordered holdouts; compare against a naïve last-observation baseline. This module forecasts *observed market prices*, not the best price to charge or customer demand. Avoid fitting hyperparameters on the evaluation window.

All algorithms are deterministic. Data collections are borrowed; their lifetimes must cover the function call. Return-code and output-mutation guarantees are documented above and in the public API declarations. None of these modules accesses networks or file systems.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-histprice.h`](../include/mulib-histprice.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-histprice.h
 * @brief Univariate time-ordered price forecasts: EMA, Holt, seasonal Holt-
 * Winters, Kalman local level, AR(1), and robust Theil-Sen trend.
 * Define MULIB_IMPL_STATSPROC and MULIB_IMPL_HISTPRICE in one TU; link -lm.
 * Observations must be finite and nonnegative, equally spaced, oldest first.
 * Returned forecasts are mathematical extrapolations, not guaranteed prices.
 */
#ifndef MULIB_HISTPRICE_H
#define MULIB_HISTPRICE_H
#include <stddef.h>
#include "mulib-statsproc.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_histprice_forecast {
  double point;
  double variance; /* NAN unless model provides predictive observation variance */
} mulib_histprice_forecast_t;
/** EMA level at last timestamp; horizon>=1 (level stays constant). */
int mulib_histprice_ema(const double *prices, size_t n, double alpha,
    size_t horizon, mulib_histprice_forecast_t *out);
/** Damped Holt trend, alpha/beta in [0,1], phi in (0,1], horizon>=1. */
int mulib_histprice_holt(const double *prices, size_t n, double alpha,
    double beta, double phi, size_t horizon, mulib_histprice_forecast_t *out);
/** Additive Holt-Winters, complete >=2 seasonal cycles, length>=2.
 * alpha,beta,gamma in [0,1]; predicts seasonally repeating pattern. */
int mulib_histprice_seasonal(const double *prices, size_t n, size_t season,
    double alpha, double beta, double gamma, size_t horizon,
    mulib_histprice_forecast_t *out);
/** Scalar local-level Kalman filter, q=process variance, r=measurement
 * variance; predictive variance includes observation noise. */
int mulib_histprice_kalman(const double *prices, size_t n, double q,
    double r, size_t horizon, mulib_histprice_forecast_t *out);
/** Stationary AR(1) regression x_t=a+b*x_(t-1); |b|<1 required. */
int mulib_histprice_ar1(const double *prices, size_t n, size_t horizon,
    mulib_histprice_forecast_t *out);
/** Robust Theil-Sen trend, O(n^2), max 1024 observations. */
int mulib_histprice_robust_trend(const double *prices, size_t n,
    size_t horizon, mulib_histprice_forecast_t *out);
/** Out-of-sample one-step EMA MAE, uses prices[0] as first level. */
int mulib_histprice_ema_mae(const double *prices, size_t n, double alpha, double *out);
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
| Header | [`include/mulib-histprice.h`](../include/mulib-histprice.h) |
| Implementation switch | `MULIB_IMPL_HISTPRICE` |
| Repository-local includes | [mulib-statsproc](mulib-statsproc.md) |
| Complete example | [`examples/modules/histprice.c`](../examples/modules/histprice.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/histprice.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
