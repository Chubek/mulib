# MuLIB competprice

[Manuals](README.md) · [Library index](index.md) · [Pricing integration](mulib-pricing-suite.md)

Competitor-aware reference-price and positioning policies with explicit hard limits.

## Purpose and contract

Requires `statsproc`. Each active rival carries price, credibility/market-share weight, and a positive quality score if quality adjustment is chosen. Inactive rivals are ignored. The algorithm builds a benchmark and applies a multiplicative and additive gap, then intersects hard price and movement bounds.

Returns `1` on success and `0` on invalid input, allocation failure, infeasible
bounds, or nonrepresentable intermediate/output. Outputs remain unchanged on
failure; rival/configuration arrays are borrowed and never modified.

## Algorithms and numerical semantics

| Policy | Benchmark |
| --- | --- |
| `WEIGHTED_MEDIAN` | Weighted discrete 50th percentile |
| `WEIGHTED_MEAN` | Sum(weight×price)/sum(weight) |
| `QUANTILE` | Unweighted type-7 quantile at configured percentile |
| `LOWEST` | Minimum active competitor price |
| `LEADER` | Price of active rival with largest weight; first wins ties |
| `QUALITY_ADJUSTED` | Weighted 50th percentile of `price × (own_quality / rival_quality)^exponent` |

The pricing rule before safeguards is

`unbounded_price = benchmark * (1 + relative_gap) - absolute_gap`.

Negative `relative_gap` undercuts the benchmark. A positive `absolute_gap` subtracts a fixed currency amount. `relative_gap > -1` is required, so the multiplicative part cannot reverse the price sign. The final feasible interval is `[min_price,max_price]`, intersected with `[previous_price×(1-max_relative_change), previous_price×(1+max_relative_change)]` only if both previous price and maximum relative change are **positive**. An empty intersection is an error, never an invitation to violate a bound. The result reports benchmark, unconstrained quote, final quote, number of active rivals, and whether clamping occurred.

**Caution.** Quality is a supplied dimensionless normalized score. There is no learned hedonic model; calibrate the exponent empirically. Selecting a particular competitor or rank can trigger retaliatory price changes. No feature assumes competitor agreement or coordination.

**Complexity.** Weighted percentile O(n log n), other approaches O(n); O(n) temporary memory.

## Example

```c
#define MULIB_IMPL_STATSPROC
#define MULIB_IMPL_COMPETPRICE
#include "mulib-competprice.h"
#include <assert.h>
int main(void) {
    const mulib_competprice_rival_t rivals[] = {
        {90,1,1,1}, {100,3,1,1}, {150,1,2,1}
    };
    mulib_competprice_config_t c = {0};
    mulib_competprice_result_t result;
    c.policy = MULIB_COMPETPRICE_WEIGHTED_MEDIAN;
    c.relative_gap = -0.05;
    c.min_price = 80; c.max_price = 200;
    assert(mulib_competprice_recommend(rivals, 3, &c, &result));
    assert(result.price == 95);
    return 0;
}
```

Compile from repository root (using the additions copied into its directories):

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude example.c -lm -o example
```

The `-lm` link flag is for the system C math library, not an external third-party dependency.

## Integration and limitations

Implementation macro: `MULIB_IMPL_COMPETPRICE`. Declare it in **one** translation unit before including the module. Required additional MuLIB implementation switches: `statsproc`. Define all such switches before their headers are first included. The implementation must not be emitted in more than one translation unit. The code is ISO C11 and exposes C linkage for C++17 clients. For the full test and multi-translation-unit recipe, see [Pricing integration](mulib-pricing-suite.md).

Supply a realistic `min_price` (including cost and minimum acceptable margin) and `max_price`. The function does not infer these values. `max_relative_change=0` disables the change cap by design; use a positive bound to enable it.

All algorithms are deterministic. Data collections are borrowed; their lifetimes must cover the function call. Return-code and output-mutation guarantees are documented above and in the public API declarations. None of these modules accesses networks or file systems.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-competprice.h`](../include/mulib-competprice.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-competprice.h
 * @brief Deterministic competitor-aware pricing policies with hard safeguards.
 * Define MULIB_IMPL_STATSPROC and MULIB_IMPL_COMPETPRICE in one TU; link -lm.
 * No demand modeling is implied by following a competitor benchmark.
 */
#ifndef MULIB_COMPETPRICE_H
#define MULIB_COMPETPRICE_H
#include <stddef.h>
#include "mulib-statsproc.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_competprice_rival {
  double price;   /* >= 0 */
  double weight;  /* >= 0; credibility / market share */
  double quality; /* > 0 for quality-adjusted method */
  int active;     /* zero skips rival */
} mulib_competprice_rival_t;
typedef enum mulib_competprice_policy {
  MULIB_COMPETPRICE_WEIGHTED_MEDIAN,
  MULIB_COMPETPRICE_WEIGHTED_MEAN,
  MULIB_COMPETPRICE_QUANTILE,
  MULIB_COMPETPRICE_LOWEST,
  MULIB_COMPETPRICE_LEADER,
  MULIB_COMPETPRICE_QUALITY_ADJUSTED
} mulib_competprice_policy_t;
typedef struct mulib_competprice_config {
  mulib_competprice_policy_t policy;
  double percentile;        /* [0,1] for quantile */
  double relative_gap;      /* -0.05 = 5 percent under benchmark; > -1 */
  double absolute_gap;      /* subtracted from post-relative quote */
  double own_quality;       /* >0 for quality-adjusted */
  double quality_exponent;  /* [0,4] for quality-adjusted */
  double min_price, max_price; /* inclusive hard bounds */
  double previous_price;    /* >=0; 0 disables movement constraint */
  double max_relative_change; /* [0,1]; 0 = no limit */
} mulib_competprice_config_t;
typedef struct mulib_competprice_result {
  double benchmark, unbounded_price, price;
  size_t competitors_used;
  int clamped;
} mulib_competprice_result_t;
/** Infeasible intersection of price bands fails without touching output.
 * Active rivals must have finite nonnegative price/weight; their zero weights
 * are accepted but cannot support weighted policies. */
int mulib_competprice_recommend(const mulib_competprice_rival_t *rivals, size_t n,
    const mulib_competprice_config_t *options, mulib_competprice_result_t *out);
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
| Header | [`include/mulib-competprice.h`](../include/mulib-competprice.h) |
| Implementation switch | `MULIB_IMPL_COMPETPRICE` |
| Repository-local includes | [mulib-statsproc](mulib-statsproc.md) |
| Complete example | [`examples/modules/competprice.c`](../examples/modules/competprice.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/competprice.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
