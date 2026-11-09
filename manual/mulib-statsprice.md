# MuLIB statsprice

[Manuals](README.md) · [Library index](index.md) · [Pricing integration](mulib-pricing-suite.md)

Robust distribution-based market price inference from contemporaneous competitor prices.

## Purpose and contract

Requires `statsproc` implementations. Prices must all be finite and nonnegative. The API returns `1` on success, leaves outputs untouched on failure, and does not modify input. It is cross-sectional: samples should describe comparable products in a comparable market and time window.

## Algorithms and numerical semantics

| Estimator | Implementation |
| --- | --- |
| `MEDIAN` | 50th percentile from sorted sample |
| `TRIMMED` | Symmetric trimmed mean with user-selected fraction |
| `WINSORIZED` | Symmetric winsorized mean |
| `HUBER` | Huber M-estimator (MAD scale, k=1.345) |
| `QUANTILE` | Type-7 conditional position within observed price distribution |
| `KDE_MODE` | Grid-based Gaussian kernel mode, optionally using bandwidth derived from IQR/SD |
| `weighted` | Market weighted mean with optional 3-MAD outlier removal |
| `summarize` | Mean, median, Q25/Q75, MAD and Huber estimate |

**Bandwidth.** Pass `parameter=0` to KDE to select a robust Silverman-style rule-of-thumb bandwidth, otherwise specify a positive bandwidth in price units. It is a heuristic; test sensitivity to it for multimodal data. Quantile uses `parameter` in `[0,1]`; trimmed/winsorized use `parameter` in `[0,0.5)`. All other methods ignore the parameter.

**Outlier filtering.** The robust weighted estimator identifies an outlier as `abs(price - unweighted_median) > 3 * MAD`; if MAD is zero it applies no filtering. This is a pragmatic robust option, not a statistically calibrated confidence interval. It still validates all weights, including filtered rows.

**Complexity.** Sorting-based estimators take O(n log n) time and O(n) space; KDE uses O(256×n) time with automatic grid size. The unweighted mean inside `summarize` uses long-double accumulation.

## Example

```c
#define MULIB_IMPL_STATSPROC
#define MULIB_IMPL_STATSPRICE
#include "mulib-statsprice.h"
#include <assert.h>
int main(void) {
    double rivals[] = {90,95,100,110,150};
    mulib_statsprice_summary_t summary;
    double reference;
    assert(mulib_statsprice_summarize(rivals, 5, &summary));
    assert(mulib_statsprice_estimate(rivals, 5,
        MULIB_STATSPRICE_HUBER, 0, &reference));
    return reference > 0 ? 0 : 1;
}
```

Compile from repository root (using the additions copied into its directories):

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude example.c -lm -o example
```

The `-lm` link flag is for the system C math library, not an external third-party dependency.

## Integration and limitations

Implementation macro: `MULIB_IMPL_STATSPRICE`. Declare it in **one** translation unit before including the module. Required additional MuLIB implementation switches: `statsproc`. Define all such switches before their headers are first included. The implementation must not be emitted in more than one translation unit. The code is ISO C11 and exposes C linkage for C++17 clients. For the full test and multi-translation-unit recipe, see [Pricing integration](mulib-pricing-suite.md).

A price distribution alone cannot predict purchases or margins. Use this module to derive a reference price and combine it with `competprice` and `priceopt` for an executable selling-price decision.

All algorithms are deterministic. Data collections are borrowed; their lifetimes must cover the function call. Return-code and output-mutation guarantees are documented above and in the public API declarations. None of these modules accesses networks or file systems.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-statsprice.h`](../include/mulib-statsprice.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-statsprice.h
 * @brief Robust cross-sectional market price estimators. Requires statsproc.
 * Define MULIB_IMPL_STATSPROC and MULIB_IMPL_STATSPRICE in one C11 TU; -lm.
 */
#ifndef MULIB_STATSPRICE_H
#define MULIB_STATSPRICE_H
#include <stddef.h>
#include "mulib-statsproc.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_statsprice_method {
  MULIB_STATSPRICE_MEDIAN = 0,
  MULIB_STATSPRICE_TRIMMED = 1,
  MULIB_STATSPRICE_WINSORIZED = 2,
  MULIB_STATSPRICE_HUBER = 3,
  MULIB_STATSPRICE_QUANTILE = 4,
  MULIB_STATSPRICE_KDE_MODE = 5
} mulib_statsprice_method_t;
typedef struct mulib_statsprice_summary {
  double median, mean, mad, q25, q75, huber;
} mulib_statsprice_summary_t;
/** All prices must be finite and >=0. Parameter: trim fraction for trimmed /
 * winsorized, quantile p for QUANTILE; ignored for MEDIAN and HUBER; positive
 * bandwidth for KDE (0 uses a robust automatic bandwidth). */
int mulib_statsprice_estimate(const double *prices, size_t n,
    mulib_statsprice_method_t method, double parameter, double *out);
/** Weights >=0, total >0. If robust=1, discard samples more than 3*MAD
 * above/below median; if MAD=0 no samples are filtered. */
int mulib_statsprice_weighted(const double *prices, const double *weights,
    size_t n, int robust, double *out);
/** Market snapshot, no assumptions about time, sales, or demand. */
int mulib_statsprice_summarize(const double *prices, size_t n,
    mulib_statsprice_summary_t *out);
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
| Header | [`include/mulib-statsprice.h`](../include/mulib-statsprice.h) |
| Implementation switch | `MULIB_IMPL_STATSPRICE` |
| Repository-local includes | [mulib-statsproc](mulib-statsproc.md) |
| Complete example | [`examples/modules/statsprice.c`](../examples/modules/statsprice.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/statsprice.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
