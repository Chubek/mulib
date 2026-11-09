# MuLIB priceopt

[Manuals](README.md) · [Library index](index.md) · [Pricing integration](mulib-pricing-suite.md)

Profit/revenue optimization under explicitly specified demand models and business constraints.

## Purpose and contract

No MuLIB dependency is required. Supports three deterministic demand families, exact finite candidate optimization, multi-scenario variance-penalized optimization, and UCB selection among historical price arms. All optimization functions return `1` for success and preserve output on failure.

## Algorithms and numerical semantics

| Capability | Formula / mechanism |
| --- | --- |
| Linear demand | `D(p)=max(0, scale-shape*p)` |
| Isoelastic demand | `D(p)=scale*(p/reference)^(-shape)`, p>0 |
| Logistic demand | `D(p)=scale/(1+exp((p-reference)/shape))`, shape>0 |
| Profit score | `(p-unit_cost)*D(p)` |
| Revenue score | `p*D(p)` |
| Scenario score | `E[score]-risk_aversion*sqrt(Var(score))` across scenarios |
| Continuous search | 129 inclusive knots, then 72 bounded golden-section refinement iterations around the best knot |
| Tick search | Exhaustively search all feasible whole ticks, up to one million candidates |
| Candidate search | Evaluate every given feasible price/demand pair exactly, lowest price wins ties |
| UCB | Untried arms first, then `mean_reward + exploration*sqrt(log(total_trials)/trials)` |

**Constraints.** `min_price`, `max_price`, `unit_cost`, and `min_unit_margin` are finite and nonnegative. The feasible lower bound is `max(min_price, unit_cost+min_unit_margin)`. Positive `previous_price` and positive `max_relative_change` add a movement band. `tick=0` selects continuous search; any positive `tick` restricts to the mathematical sequence `{k*tick}` (integer k). If the discrete feasible domain is empty or exceeds one million prices, search fails instead of truncating the domain.

**Caveats.** Continuous search is approximate; the 129-knot scan reduces but does not mathematically eliminate the possibility of missing a narrow or discontinuous optimum. The built-in demand models are continuous. Risk penalization is a **mean-minus-standard-deviation utility** rather than robust worst-case optimization. Scenario weights are nonnegative, have positive sum, and are normalized internally. UCB expects actual observed reward means and trial counts and **does not itself conduct an experiment**. Never treat predictions as causal elasticities without randomized experiments or strong identification assumptions.

Tick indices within roundoff of an integer are snapped (at most `1e-8` of a
tick); returned prices always remain inside the hard feasible interval. Indices
above `2^52-1` are rejected because adjacent ticks cannot reliably be resolved.
Candidate search validates every row but skips prices outside the feasible band
or tick grid. Zero-weight scenarios must still have valid model parameters, but
their demand is not evaluated (e.g. an unused isoelastic scenario does not
exclude price zero). The scenario result's `score` includes the risk penalty;
`expected_demand` is the probability-weighted demand. Variance uses centered
weighted updates rather than subtracting two large raw moments.

**Complexity.** A single-model continuous search is O(129+2×72) evaluations and constant memory. Tick mode O(number of candidate ticks). Scenario search is O(scenarios × evaluations). The one-step UCB selection is O(arms) memory O(1).

## Example

```c
#define MULIB_IMPL_PRICEOPT
#include "mulib-priceopt.h"
#include <assert.h>
int main(void) {
    const mulib_priceopt_demand_model_t demand = {
        MULIB_PRICEOPT_LINEAR, 100, 1, 0
    };
    const mulib_priceopt_constraints_t limits = {
        0, 100, 20, 0, 0, 0, 0
    };
    mulib_priceopt_result_t r;
    assert(mulib_priceopt_optimize(&demand, &limits,
        MULIB_PRICEOPT_PROFIT, &r));
    /* (price-20)*(100-price) peaks at price 60. */
    assert(r.price > 59.99 && r.price < 60.01);
    return 0;
}
```

Compile from repository root (using the additions copied into its directories):

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude example.c -lm -o example
```

The `-lm` link flag is for the system C math library, not an external third-party dependency.

## Integration and limitations

Implementation macro: `MULIB_IMPL_PRICEOPT`. Declare it in **one** translation unit before including the module. Required additional MuLIB implementation switches: None. Define all such switches before their headers are first included. The implementation must not be emitted in more than one translation unit. The code is ISO C11 and exposes C linkage for C++17 clients. For the full test and multi-translation-unit recipe, see [Pricing integration](mulib-pricing-suite.md).

Use `mulib_histprice_*` to estimate market conditions, but estimate demand using actual own-price/sales experiments where possible. The competitor benchmark should be treated as an explanatory feature, not as the optimized objective.

All algorithms are deterministic. Data collections are borrowed; their lifetimes must cover the function call. Return-code and output-mutation guarantees are documented above and in the public API declarations. None of these modules accesses networks or file systems.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-priceopt.h`](../include/mulib-priceopt.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-priceopt.h
 * @brief Self-contained continuous/discrete demand-based price optimization,
 * scenario-risk objectives, and deterministic UCB bandit selection.
 * Define MULIB_IMPL_PRICEOPT in exactly one translation unit; link -lm.
 * This is model-based optimization; data alone cannot identify demand causally.
 */
#ifndef MULIB_PRICEOPT_H
#define MULIB_PRICEOPT_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_priceopt_demand_kind {
  MULIB_PRICEOPT_LINEAR,          /* max(0,scale-shape*price) */
  MULIB_PRICEOPT_ISOELASTIC,     /* scale * (price/reference)^(-shape) */
  MULIB_PRICEOPT_LOGISTIC        /* scale/(1+exp((price-reference)/shape)) */
} mulib_priceopt_demand_kind_t;
typedef struct mulib_priceopt_demand_model {
  mulib_priceopt_demand_kind_t kind;
  double scale, shape, reference;
} mulib_priceopt_demand_model_t;
typedef enum mulib_priceopt_objective {
  MULIB_PRICEOPT_PROFIT, MULIB_PRICEOPT_REVENUE
} mulib_priceopt_objective_t;
typedef struct mulib_priceopt_constraints {
  double min_price, max_price, unit_cost, min_unit_margin;
  double previous_price, max_relative_change; /* previous=0 or max_change=0 disables */
  double tick; /* 0=continuous; >0 means exact tick-grid search, <=1M candidates */
} mulib_priceopt_constraints_t;
typedef struct mulib_priceopt_result {
  double price, score, expected_demand;
  size_t evaluations;
} mulib_priceopt_result_t;
/** Returns NAN for invalid model/price. Negative/overflow demand is invalid.
 * Isoelastic price must be >0; logistic uses overflow-stable sigmoid. */
double mulib_priceopt_demand(const mulib_priceopt_demand_model_t *m, double price);
/** Deterministic 129-knot scan + local golden search for continuous prices;
 * tick mode exhaustively scans all feasible ticks. Scores are model estimates. */
int mulib_priceopt_optimize(const mulib_priceopt_demand_model_t *model,
    const mulib_priceopt_constraints_t *constraints,
    mulib_priceopt_objective_t objective, mulib_priceopt_result_t *out);
/** Multi-scenario risk score = E[objective] - risk_aversion*stddev(objective).
 * Nonnegative scenario probabilities (not required to sum to one) and risk.
 * Demand is probability-weighted; score includes the risk penalty.
 * Zero-weight scenarios are validated but do not constrain the price domain. */
int mulib_priceopt_scenarios(const mulib_priceopt_demand_model_t *models,
    const double *weights, size_t n, double risk_aversion,
    const mulib_priceopt_constraints_t *constraints,
    mulib_priceopt_objective_t objective, mulib_priceopt_result_t *out);
/** Exact argmax among feasible candidates and forecasted demand. All rows are
 * validated; infeasible rows are skipped. Equal scores favor lower prices. */
int mulib_priceopt_candidates(const double *prices, const double *predicted_units,
    size_t n, const mulib_priceopt_constraints_t *constraints,
    mulib_priceopt_objective_t objective, mulib_priceopt_result_t *out);
/** UCB1 exploration: choose untried arm first, else mean_reward +
 * exploration*sqrt(log(total_trials)/trials). Deterministic ties. */
int mulib_priceopt_ucb(const size_t *trials, const double *mean_reward,
    size_t arms, double exploration, size_t *selected);
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
| Header | [`include/mulib-priceopt.h`](../include/mulib-priceopt.h) |
| Implementation switch | `MULIB_IMPL_PRICEOPT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/priceopt.c`](../examples/modules/priceopt.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/priceopt.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
