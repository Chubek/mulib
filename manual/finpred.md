# FinPRED — Bounded financial and hospitality kernels

The 17 kernels are **partial implementation candidates**, not audited trading
facilities. Manifest registration and CDecl extraction describe the C APIs;
neither grants On1x call admission. No broker connector, authorization grant,
price publisher, or production review is included.

`make check-finpred` uses canonical MuLIB Money and runs two synthetic
examples and the C11/C++17 and oracle suites. The hospitality example checks
a versioned four-guest, five-night stay, compares a family room, two doubles
and an apartment, and computes P50 18,500 and P40 18,300 in EUR minor units per
night. The difference is strictly below 500. An independent floor, ceiling
and tick constrain the result. Adding a revision not known at the decision
does not change the result or trace fingerprint. This is a C11 integration
fixture, not the blocked On1x application or an authenticated audit record.

| Kernel | Implemented | Unit-tested | Oracle / property evidence | Remaining blocker |
| --- | --- | --- | --- | --- |
| [Contract](mulib-contract.md) | Yes | Yes | Canonical digest vector | Schema evolution, dataset/snapshot semantics, source trust |
| [RateQuote](mulib-ratequote.md) | Yes | Yes | 120,000 exact minor-unit checks | Itemized service charges and external provenance |
| [StayMatch](mulib-staymatch.md) | Yes | Yes | 21,632 legacy and 2,400 dated scenarios | Cot/bed and connecting-room constraints; real inventory benchmarks |
| [CompSet](mulib-compset.md) | Yes | Yes | Ordering, deduplication and estimator fixtures | Platform identity verification and weight calibration |
| [PriceCurve](mulib-pricecurve.md) | Yes | Yes | Sampled monotonicity, continuity and endpoints | Caller-side hysteresis and calendar scheduling |
| [AsOf](mulib-asof.md) | Yes | Yes | Brute-force selection and replay checks | Persistent indexing, trusted clocks, authenticated snapshots |
| [DecisionTrace](mulib-decisiontrace.md) | Yes | Yes | Canonical byte and digest vectors | Exact policy/data binding, signatures and storage |
| [RuleEval](mulib-ruleeval.md) | Yes | Yes | 20,280 tick-grid cases | Stable policy IDs, rejection evidence and compiler integration |
| [Backtest](mulib-backtest.md) | Yes | Yes | Timing and holdout fixtures | Training and fold orchestration; verified costs and provenance |
| [MarketQuote](mulib-marketquote.md) | Yes | Yes | Boundary cases | Venue tick and lot schedules, corrections, conditions |
| [TimeSeries](mulib-timeseries.md) | Yes | Yes | Timing and ordering fixtures | Persistence, corrections and resampling |
| [CorporateActions](mulib-corporate-actions.md) | Yes | Yes | Python exact integer oracle | Multi-event ordering and fractional-share policies |
| [FXRate](mulib-fxrate.md) | Yes | Yes | Python exact integer oracle | Bid/ask, triangulation and trusted provenance |
| [Portfolio](mulib-portfolio.md) | Yes | Yes | Checked arithmetic cases | Lots, realized P&L and reference-currency valuation |
| [Risk](mulib-risk.md) | Yes | Yes | Threshold and tie fixtures | Net and sector constraints, scenario models |
| [OrderBook](mulib-orderbook.md) | Yes | Yes | 4,096 model-checked updates | Venue recovery, checksums and throughput |
| [OrderIntent](mulib-orderintent.md) | Yes | Yes | Python exact integer oracle | Venue tick/lot schedules; separate execution approvals |

The [real On1x extraction run](../on1x/VERIFICATION.md#finpred-declaration-discovery--2026-11-21)
established the following separate gates:

| Kernel | On1x discovered | On1x callable unsafe | On1x safe independently admitted | Production audited |
| --- | --- | --- | --- | --- |
| Contract | Yes | No | No | No |
| RateQuote | Yes | No | No | No |
| StayMatch | Yes | No | No | No |
| CompSet | Yes | No | No | No |
| PriceCurve | Yes | No | No | No |
| AsOf | Yes | No | No | No |
| DecisionTrace | Yes | No | No | No |
| RuleEval | Yes | No | No | No |
| Backtest | Yes | No | No | No |
| MarketQuote | Yes | No | No | No |
| TimeSeries | Yes | No | No | No |
| CorporateActions | Yes | No | No | No |
| FXRate | Yes | No | No | No |
| Portfolio | Yes | No | No | No |
| Risk | Yes | No | No | No |
| OrderBook | Yes | No | No | No |
| OrderIntent | Yes | No | No | No |

The release TODO markers remain for the unverified work above. The On1x
application is blocked on independently reviewed callable contracts; a raw
`extern` declaration is not an alternative.

## Reproduce the C verification

Linux x86-64, GCC/G++ 16.2.1 and Clang/Clang++ 22.1.8. MuLIB starting
revision `b27903544793828c8cbe44db39da40126f186261`, checked on
`eeedc2aed7db341be86c6c281582d7f46b1a564b` plus the FinPRED changes.
Canonical Money blob `9517307b284437325adf1698cd33c1dd4fe8eb66`.

```sh
export MULIB_ROOT="$PWD"
CC=gcc CXX=g++ sh tests/finpred-tests/run.sh
CC=clang CXX=clang++ sh tests/finpred-tests/run.sh
CC=gcc CXX=g++ SANITIZE=1 sh tests/finpred-tests/run.sh
CC=clang CXX=clang++ SANITIZE=1 sh tests/finpred-tests/run.sh
```

All four runs passed strict C11/C++17 tests, including the one-implementation
TU fixture, and 11,000 Python arbitrary-precision Money cases. The optional
test-only fallback does not provide canonical financial oracle evidence.
