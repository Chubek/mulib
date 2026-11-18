# Shylock readiness — scalar MIR continuation

**SHYLOCK-EXECUTABLE-READY: NO. SHYLOCK-SAFE-FFI-READY: NO.**

Current continuation: [bounded numerical KernelBind](kernel-numerics.md)
now executes canonical EMA, Holt, optimize, and candidates calls through an
independently signed review gate. The multi-module pricing example checks
embedded monetary inputs, selection/holdout, optimization, and direct C
equivalence. No production review is shipped and §9.5 still lacks admitted
JSON/file I/O and Shylock parity. The scalar MIR baseline below is historical.

This continuation implements an executable scalar MIR foundation (B1 partial).
It does not implement a pricing application or change foreign-contract admission.
The full executable and safe-import gates in `COMPLETION.md` remain open.

## Baseline and dependencies

- Parent MuLIB baseline: `ce5bf41e1b08ca9d9e5ce23d37952fadc5826b5b`.
- Configured LibStone checkout: `on1x/third_party/libstone`, HEAD and index gitlink
  `91293264a531603dfd2319e64b3c1aa40a609783`; clean and used read-only.
- `git submodule status` currently fails because `.gitmodules` has no mapping for
  this gitlink. Direct `git -C ... rev-parse HEAD` and `git ls-files --stage ...`
  establish the local identities; no upstream synchronization is claimed.
- Pre-change existing GCC generated+ExoML/libclang suite: 24/24; independent GCC
  fallback: 22/22. Current results and exact commands are in `VERIFICATION.md`.
- CMake now prefers the supplied nested checkout, respects the environment/cache
  override, and writes configure-time `on1x-build-info.txt` with compiler, scanner,
  checkout revision and implemented backend capabilities. This is build context,
  not source/ABI/contract admission evidence.

## Selected canonical kernel boundary

These are source/manual inspections, not ExoML discovery or independent review.
Each row is **unsupported by the current On1x typed pricing API**: pricing CDecl
snapshots/contracts, ABI lowering, ownership proof, recoverable error adapters
and On1x kernel execution tests are all missing. Existing JSON/Vec/Parsec contract
tests are not evidence for these functions.

| Canonical function | Header | Required boundary |
|---|---|---|
| `mulib_statsprice_estimate` | `include/mulib-statsprice.h` | Borrowed double array + size_t, method enum, double parameter, checked double output |
| `mulib_histprice_ema` | `include/mulib-histprice.h` | Double array + size_t, alpha, horizon, forecast record output |
| `mulib_histprice_holt` | `include/mulib-histprice.h` | EMA boundary plus beta/phi; at least two samples |
| `mulib_price_forecast` | `include/mulib-price-forecast.h` | Double array, size/horizon/season, algorithm enum, alpha, scalar output |
| `mulib_price_ensemble_blend` | `include/mulib-price-ensemble.h` | Paired borrowed arrays, count, two distinct writable outputs |
| `mulib_price_demand_fit` | `include/mulib-price-demand.h` | Paired prices/units, count, ridge, fitted-model output |
| `mulib_priceopt_optimize` | `include/mulib-priceopt.h` | Demand/config record pointers, objective enum, result with double fields and size_t |

Paths above are parent-MuLIB-relative; corresponding manuals use the same basename
under `manual/`. `MANIFEST.yaml` specifies `statsproc` for statsprice/histprice,
no module dependency for demand/ensemble/priceopt, and the transitive
money → observation → history → timeseries → forecast chain. Documented recipes
define each selected `MULIB_IMPL_*` in one C11 TU and link libm as required.
The existing On1x build-plan tool has no pricing profile yet.

Important behavioral differences to preserve in future adapters: histprice EMA
accepts alpha zero, uniform forecast EMA does not; histprice's unavailable
variance is NAN; ensemble dispersion is not a prediction interval. Demand-fit
and priceopt model records are different C types and are not interchangeable.
Outputs must only be published after actual kernel success.

## Next acceptance blockers

1. `f64`, checked size/enum conversion, bounded numeric buffers and record/error
   representation, tested with the actual C prototypes.
2. Canonical ExoML v2 pricing inventories and source/ABI pins; explicit trusted
   adapters while independently authorized review and usage proofs are absent.
3. Resource lifetimes, output initialization, failure cleanup and effects for each
   admitted callsite; general MIR ownership/foreign descriptors are still absent.
4. A multi-module On1x fixture-driven forecasting/backtesting/demand/optimization
   application and direct-C numerical differential, sanitizers and failure injection.

Native code generation and optional optimizers are not prerequisites for those
steps. The scalar MIR example in `docs/backend-mir.md` is reproducible today;
there is no Shylock build/run command until an actual application exists.
