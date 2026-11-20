# KernelBind: bounded forecasts and price optimization

These reviewed C11 profiles add four callable canonical MuLIB exports on
native Linux x86-64. The operator must supply an independent signed review,
as in [the scalar review workflow](kernel-review.md). The shipped ExoML
sidecars are unreviewed annotations; no production trust anchor or approval
is included.

| Native import | Callable members | On1x interface |
|---|---|---|
| `import kernel mulib.histprice as History;` | `ema`, `holt` | `ema(samples, alpha, horizon)` and `holt(samples, alpha, beta, phi, horizon)`, returning `History.Result<History.Forecast, History.Error>` |
| `import kernel mulib.priceopt as Pricing;` | `optimize`, `candidates` | `optimize(model, constraints, objective)` and `candidates(prices, predicted_units, constraints, objective)`, returning `Pricing.Result<Pricing.Decision, Pricing.Error>` |

Slice inputs are existing immutable **snapshot** `slice<f64,N>` values with
known capacity `0..256`. Capacity is checked at compile time. The adapter
checks start, length, and extent before doing pointer arithmetic or converting
to `size_t`; it borrows internally for the duration of the call. Candidate
spans may have different capacities, but must be equally long and nonempty.
There is no exposed pointer, mutable span, escaping borrow, or heap owner.
Horizon is signed `i64` at the On1x boundary and checked against `1..1000000`
before conversion. Samples must be finite and nonnegative; `alpha` and `beta`
are in `[0,1]`, and `phi` is in `(0,1]`. Holt requires at least two samples.

`History.Forecast` has `point: f64` and `variance: f64`; unavailable variance
remains IEEE NaN. `Pricing.Decision` has `price`, `score`, and
`expected_demand` as `f64`, and `evaluations` as `u64`. `Pricing.DemandModel`
holds `kind: Pricing.DemandKind` and `scale`, `shape`, `reference` as `f64`.
Construct `Pricing.DemandKind` and `Pricing.Objective` with the existing
nominal syntax, for example
`variant<Pricing.DemandKind>(Linear, record{})` and
`variant<Pricing.Objective>(Profit, record{})`. The other cases are
`Isoelastic`, `Logistic`, and `Revenue`. Constraints are `f64` fields named
`min_price`, `max_price`, `unit_cost`, `min_unit_margin`, `previous_price`,
`max_relative_change`, and `tick`, with the meanings in the canonical C API.

The adapter maps nominal enum cases to named C constants, not to assumed
integer identities. It constructs C records field by field, calls the real
prototype, and copies primitive results into separate temporaries only on
status `1`. Failure (including rejected bounds) becomes `Err(Rejected())`;
it never publishes partial output. `?` works in a function returning the
corresponding module's nominal `Result<T,E>`. These kernel-specific `Result`
declarations are distinct; propagating between them requires explicit
translation. No error detail is fabricated where the C API only returns `0`.

## Running the fixture

`on1x/examples/kernel/pricing/main.on1x` imports an ordinary On1x forecasting
module and the optimization kernel. It validates and converts embedded minor
units, separates training, chronological selection, and a holdout observation,
executes tick-grid optimization and discrete candidate evaluation, and tests
the mismatched-length failure. A separate C11 fixture checks the same workflow.
This is **not** general application integration: it still uses embedded
samples and does not perform admitted JSON/file I/O or production data handling.

Generate a review subject using the [same command and trust
procedure](kernel-review.md#reproducible-workflow), replacing the input with
`on1x/examples/kernel/pricing/main.on1x`. The candidate is then an
`on1x.kernel-review-subject-set/v1`; sign the **whole** subject through an
independent authority. Approval of one profile cannot authorize the two-profile
build. The bundle has one implementation object with
`MULIB_IMPL_STATSPROC`, `MULIB_IMPL_HISTPRICE`, and `MULIB_IMPL_PRICEOPT`.
Its `interface.json` is an `on1x.kernel-interface-set/v1` and names each
selected and rejected member. Its generated adapter object is the same object
linked into the executable boundary probe and the application.

The SDK integration test is executable without a production review (using an
ephemeral **test** authority):

```sh
ctest --test-dir build/kernelbind-gcc \
  -R 'on1x.kernel_history_sdk' --output-on-failure
```

The probes compare the actual C functions and adapters, including NaN
variance, bounds, enum rejection, invalid limits, output-not-published cases,
and link the same one-TU archive. Production review must cover the exact
canonical C source, semantics, target, and build inputs; the probes do not
prove arbitrary C code safe. Unsupported members, unsupported targets,
general resource-bearing calls, callbacks, MIR and independent checker modes
remain closed.
