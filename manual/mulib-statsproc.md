# MuLIB statsproc

[Manuals](README.md) · [Library index](index.md) · [Pricing integration](mulib-pricing-suite.md)

Robust statistical procedures reused across the price-estimation modules.

## Purpose and contract

**Data contract.** Samples are finite real numbers. The core generic functions accept negative values (unlike price-specific APIs). Arrays are borrowed, never mutated. Every call returns `1` for success and `0` for invalid input, allocation error or nonrepresentable output; outputs remain unmodified on failure. `malloc`/`free` are used for sorted copies and Theil–Sen slope arrays. No source besides ISO C11 standard headers is used.

## Algorithms and numerical semantics

| Procedure | Algorithm / meaning | Complexity |
| --- | --- | --- |
| `quantile` | Hyndman–Fan type 7 interpolation over sorted samples | O(n log n), O(n) memory |
| `trimmed_mean` | Drop `floor(n * trim)` observations at each end | O(n log n), O(n) |
| `winsor_mean` | Clamp both tails to interior order statistics | O(n log n), O(n) |
| `weighted_mean` | Long-double weighted sum divided by total weight | O(n), O(1) |
| `weighted_quantile` | First sorted value reaching target cumulative weight | O(n log n), O(n) |
| `mad` | Median of `abs(x_i - median(x))` | O(n log n), O(n) |
| `huber` | Iteratively reweighted least squares with MAD-derived scale | O(n log n + iterations × n), O(n) |
| `kde_mode` | Evaluate Gaussian kernel density on an inclusive fixed grid | O(n × grid), O(1) |
| `theil_sen` | Median pairwise slope; intercept from median residual | O(n² log n), O(n²) |

**Statistical choices.** `weighted_quantile` intentionally uses a discrete CDF (not interpolated weighted order statistics). `kde_mode` searches a grid, so it is an approximate mode. Theil–Sen requires at least two unequal x values and limits input size to 1024; this simple median-pairwise version is designed for small histories and does not provide interval estimates. Huber defaults to 50 IRLS passes if the caller passes zero. A zero MAD returns the median. The MAD returned by `mad` is *unscaled*; multiplying by about 1.4826 estimates Gaussian sigma.

**Validation.** Weight arrays must be finite and nonnegative and have positive total mass. `trim` is finite and in `[0, 0.5)`, quantile probability in `[0,1]`, bandwidth is strictly positive, KDE grid size is 2–4096. Min/max and huge values may cause representability failures; the implementation does not silently saturate.

Quantile interpolation avoids overflowing the difference between opposite-sign
endpoints. Theil–Sen uses long-double intermediate differences and residuals,
rejects nonfinite slopes/residuals, and requires distinct slope/intercept output
pointers. Availability of wider `long double` arithmetic is platform-dependent.

## Example

```c
#define MULIB_IMPL_STATSPROC
#include "mulib-statsproc.h"
#include <assert.h>
int main(void) {
    const double xs[] = {90, 100, 100, 110, 900};
    double median, huber;
    assert(mulib_statsproc_quantile(xs, 5, .5, &median));
    assert(mulib_statsproc_huber(xs, 5, 1.345, 50, &huber));
    assert(median == 100);
    return 0;
}
```

Compile from repository root (using the additions copied into its directories):

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude example.c -lm -o example
```

The `-lm` link flag is for the system C math library, not an external third-party dependency.

## Integration and limitations

Implementation macro: `MULIB_IMPL_STATSPROC`. Declare it in **one** translation unit before including the module. Required additional MuLIB implementation switches: None. Define all such switches before their headers are first included. The implementation must not be emitted in more than one translation unit. The code is ISO C11 and exposes C linkage for C++17 clients. For the full test and multi-translation-unit recipe, see [Pricing integration](mulib-pricing-suite.md).

The functions do not assume the input represents a random sample. A robust estimator gives a stable reference price; it does not identify demand elasticity or a profit-maximizing action.

All algorithms are deterministic. Data collections are borrowed; their lifetimes must cover the function call. Return-code and output-mutation guarantees are documented above and in the public API declarations. None of these modules accesses networks or file systems.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-statsproc.h`](../include/mulib-statsproc.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-statsproc.h
 * @brief Robust, dependency-free statistical primitives for price estimation.
 * Define MULIB_IMPL_STATSPROC in exactly one translation unit; link -lm.
 * All inputs are borrowed. Functions return 1 on success and preserve outputs
 * on failure. Empty/nonfinite samples and invalid weights are rejected.
 * Generic statistics accept signed values; price wrappers reject negatives.
 */
#ifndef MULIB_STATSPROC_H
#define MULIB_STATSPROC_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Quantile using Hyndman-Fan type 7 interpolation, p in [0,1]. */
int mulib_statsproc_quantile(const double *values, size_t n, double p, double *out);
/** A symmetric trimmed mean; trim in [0,0.5), floor(n*trim) at either end. */
int mulib_statsproc_trimmed_mean(const double *values, size_t n, double trim, double *out);
/** Symmetric winsorized mean; replace tails at the corresponding order stats. */
int mulib_statsproc_winsor_mean(const double *values, size_t n, double trim, double *out);
/** Weighted arithmetic mean, strictly positive total weight. Zero weights permitted. */
int mulib_statsproc_weighted_mean(const double *values, const double *weights, size_t n, double *out);
/** Weighted nearest-CDF quantile, p in [0,1]. No interpolation within masses. */
int mulib_statsproc_weighted_quantile(const double *values, const double *weights, size_t n, double p, double *out);
/** Median absolute deviation (unscaled), defined about the sample median. */
int mulib_statsproc_mad(const double *values, size_t n, double *out);
/** Huber location IRLS with k>0; k=1.345 is a standard default.
 * At most max_iter iterations (0 selects 50), reltol=1e-9. */
int mulib_statsproc_huber(const double *values, size_t n, double k, size_t max_iter, double *out);
/** Gaussian kernel density mode on an inclusive uniform grid (2..4096 points).
 * Bandwidth >0; grid discretization is deliberate. */
int mulib_statsproc_kde_mode(const double *values, size_t n, double bandwidth, size_t grid, double *out);
/** Robust line y=intercept+slope*x via median pairwise slope (Theil-Sen).
 * Requires 2..1024 rows, with >=1 unequal x pair; O(n*n log n) time.
 * Slope and intercept outputs must be distinct. */
int mulib_statsproc_theil_sen(const double *x, const double *y, size_t n, double *slope, double *intercept);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_STATSPROC_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-statsproc.h`](../include/mulib-statsproc.h) |
| Implementation switch | `MULIB_IMPL_STATSPROC` |
| Repository-local includes | None |
| Complete example | [`examples/modules/statsproc.c`](../examples/modules/statsproc.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/statsproc.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
