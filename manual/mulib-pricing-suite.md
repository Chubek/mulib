# MuLIB pricing and solver additions

[Manuals](README.md) · [Library index](index.md) · [Integration guide](integration.md)

This directory adds five pricing/statistics modules and two **optional**
solver modules to MuLIB. The source is ISO C11 with C++17-compatible `extern
"C"` declarations, follows the existing `MULIB_IMPL_*` convention, and uses
only the C runtime and C standard math library (link `-lm`). The source code
contains no third-party code, network clients, or commercial solvers.

## Component overview

| Module | Role | Requires |
| --- | --- | --- |
| [`statsproc`](mulib-statsproc.md) | Median, quantiles, MAD, Huber, KDE, Theil–Sen | — |
| [`statsprice`](mulib-statsprice.md) | Robust market price reference estimation | statsproc |
| [`competprice`](mulib-competprice.md) | Competitor-weighted price positioning with safeguards | statsproc |
| [`histprice`](mulib-histprice.md) | EMA, damped Holt, additive seasonality, Kalman, AR(1), robust trend | statsproc |
| [`priceopt`](mulib-priceopt.md) | Constrained demand-model optimization, scenarios, UCB arm selection | — |
| [`sat`](mulib-sat.md) | Bounded exact CNF satisfiability, assumptions, Boolean model | — |
| [`smt`](mulib-smt.md) | Guarded Boolean + real difference-logic constraints | sat |

The SAT and SMT implementations are **not dependencies** of the pricing
modules. They are independent, narrowly scoped peripheral facilities; this
avoids introducing solver complexity into continuous price calculations.

## Integration

All seven modules are registered in [`MANIFEST.yaml`](../MANIFEST.yaml), with
standalone examples in [`examples/modules/`](../examples/modules/). The focused
C tests and mixed C/C++ linkage test are integrated into CMake/CTest (the latter
when a C++ compiler is available).

Use one implementation translation unit. The supplied
[`tests/price_implementations.c`](../tests/price_implementations.c) shows the
full combination. It defines the seven implementation switches and includes
all seven headers. Other .c or .cpp consumers include the headers **without**
defining those switches. For a single-pricing-module program, define the
corresponding switch and its dependency switches only.

```sh
# From the root of the checkout:
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
   -c tests/price_implementations.c -o price_implementations.o
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Iinclude \
   tests/test_pricing_cpp.cpp price_implementations.o -lm -o pricing_cpp
./pricing_cpp
sh tests/run_pricing_tests.sh
CC=clang CXX=clang++ SANITIZERS=1 sh tests/run_pricing_tests.sh
```

The `run_pricing_tests.sh` driver creates temporary build outputs and cleans
them up when it exits. It accepts `CC`, `CXX`, `CFLAGS`, `CXXFLAGS`, `LDFLAGS`,
`TMPDIR`, and `SANITIZERS=1`; assertions remain enabled. It does not modify the
existing repository contents.

## Model design and interpretation

**Cross-sectional estimates** (`statsprice`) characterize a present-day
market snapshot. They can be used without sales history but do not identify a
profit-optimal price. A competitor benchmark is an **input** to a pricing
policy, not evidence of willingness to pay.

**Competitor policies** (`competprice`) apply explicit rules such as weighted
median with 5% undercutting. Configure a cost/margin-based hard floor and an
upper ceiling. If a previous price and a positive movement limit are supplied,
recommendations are confined to the movement band. An infeasible intersection
returns an error rather than violating hard constraints.

**Historical forecasts** (`histprice`) estimate where recorded market prices
may go. Observations must be uniform-time-spaced, sorted oldest first. Rolling
backtesting and comparison against a last-observation baseline are essential.
The included error diagnostic is prequential one-step EMA MAE. It does not
perform a full cross-validated hyperparameter search.

**Price optimization** (`priceopt`) requires a demand model or calibrated
candidate demand predictions and costs. Three demand families provide
tractable initial capabilities. Prefer fitting demand with randomized price
experiments or other credible causal designs; simply regressing past revenue
on price may encode selection bias. `priceopt_scenarios` applies a risk penalty,
not formal distributionally robust optimization. The UCB helper chooses an
arm by historical trial/reward counts; your application must run the trial,
log results, and update rewards.

### Financial safeguards

- Treat all monetary inputs as **the same currency and unit**, and distinguish
  tax-inclusive and tax-exclusive prices consistently.
- Set unit cost and minimum margin to include direct costs and applicable
  fulfillment/transaction costs; the module cannot infer these.
- Constrain excessive pricing moves and disallow loss-making quotes when
  that is your business requirement.
- Prices are floating point; for accounting and exact money arithmetic,
  store integer minor units in the application and convert at its boundary.
  `priceopt` can restrict feasible prices to an explicit tick grid.
- Account for stock, holidays, promotions, competitor stockouts, and product
  comparability before interpreting model outputs. There is no hidden feed
  ingestion, forecasting retraining, or automated publication of prices.
- Validate the resulting price manually and against applicable rules for your
  market before publishing it. The algorithms do not implement a jurisdiction-
  specific compliance engine.

## Solver scope

`mulib-sat.h` implements bounded DPLL, not industrial CDCL; `mulib-smt.h`
implements **only** guarded real difference constraints of the form
`x_i - x_j <= c` plus Boolean CNF, enumerating SAT models and checking them
with Bellman–Ford. No SAT/SMT functionality depends on any external library,
and no absent SMT-LIB grammar or non-linear arithmetic support is claimed.

## Testing coverage

The supplied tests exercise statistical oracle values and invalid inputs,
competitor floor/ceiling and movement bands, time series baseline cases,
linear-demand analytic optima, scenario selection, SAT/UNSAT and assumptions,
Boolean/arithmetic SMT satisfiability, exhaustive truth-table comparison for
random small CNFs, metamorphic pricing properties and a C++ caller linked
against a C11 implementation.

Additional suites compare 500 small SMT systems against exhaustive Boolean
enumeration plus an independent integer Floyd–Warshall oracle, fail successive
allocations to verify rollback and output preservation, and cover extreme-value
interpolation, floating tick endpoints, scenario variance cancellation, solver
model invalidation, and unrepresentable SMT witnesses. The MuLL bridge has a
native-call fixture in `mudsl/tests/mull/mulib/pricing.mull`.

The test script validates compilation under `-Wall -Wextra -Wpedantic -Werror`
with assertions enabled. These tests are not a substitute for independent
large-scale validation, fuzzing under sanitizers, performance benchmarking or
field-data backtesting.
