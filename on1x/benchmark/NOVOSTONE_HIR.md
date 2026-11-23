# Novostone checked-HIR differential

This direct-C11 diagnostic uses **one** `on1xc` binary with Novostone HIR and
MEMGraph bounds support. It runs `on1xc --emit-c`, compiles and links that C with
the real On1x runtime, and executes the binary. No Archon or Cephyr emission
is involved.

Build `on1x-benchmark` as in [README.md](README.md), then run:

```sh
/path/to/on1x-benchmark novostone-hir /path/to/on1xc /usr/bin/cc \
  /path/to/fresh-output-directory
```

The four combinations independently select `--enable-opt` or `--disable-opt`
for `novostone-hir` and `memgraph-bounds`. All four use `--opt-profile debug`
to isolate the two passes from other optional transformations. The runner
also checks that omitting the Novostone selector in this profile produces
byte-identical C to turning it off explicitly for the fold fixture.

| Fixture | Exact runtime oracle | Emitted-C check |
|---|---|---|
| [`fold`](novostone-hir/fold.on1x) | `22`, `101`, `11`, `2147483648`; success | Novostone removes the checked multiply and subtract in `a[(2 * 3) - 5]`. MEMGraph alone controls removal of the proven in-range check. The call-based index keeps its check, and `i64` arithmetic does not wrap at `i32` width. |
| [`overflow`](novostone-hir/overflow.on1x) | `17`, then `addition overflow`; status 70 | The checked addition remains even though reassociation could yield a small value. |
| [`division`](novostone-hir/division.on1x) | `17`, then `invalid division`; status 70 | The checked divide remains even if its denominator folds to zero. |
| [`bounds`](novostone-hir/bounds.on1x) | `17`, `23`, `24`, then a source-located `index out of bounds`; status 70 | Both effectful index operands run once, left to right. The dynamic check remains even with both passes selected. |

The runner compares exact stdout, stderr and exit status to the oracle on
every execution. It also records `--dump-processor` JSON outside measurement
and requires a positive `novostone_hir.applied` for the enabled fold fixture.
Every compile, link, report and execution has a 30-second deadline. The
output directory retains the pinned compiler, source copies, C, executables,
processor reports and per-command logs. A complete `report.json` is written
only when all gates pass; it has a separate schema from `benchmark run`.

For the fold fixture, the report retains five compile samples per combination
after one warmup. Novostone off/on order alternates within each MEMGraph
setting. Compare `emission_ns` medians at the same MEMGraph setting to see
the observed frontend overhead; `compile_link_ns` and `total_compile_ns`
are recorded separately and exclude the shared runtime object build. These
are tiny diagnostics and wall-clock samples include process startup and
scheduling noise. There are no runtime speed measurements or performance
promises; observing fewer checks in C does not establish a native speedup.

## Verification snapshot

On Linux x86-64 (`x86_64-pc-linux-gnu`), GCC 16.2.1 and Clang 22.1.8
each passed all 16 configurations (36 executable checks per compiler,
including warmups). The Release `on1xc` was built with Novostone and
MEMGraph enabled and Cephyr disabled. The work used MuLIB `87cbf7c3` with
working changes, LibStone `4c350776`, and compiler SHA-256
`bedb3aafc6c7d1068d55ae22ba91bab98fb15e311291aa5671134e6555a4cc7c`.
The fold's processor report showed `applied: 1`, `obligations: 6`, and
`smt_proved: 0`; this run is not evidence of SMT discharge.

Fold compilation medians (milliseconds, five samples per combination):

| C compiler | MEMGraph | On1x emission, Novostone off → on | Total compile, Novostone off → on |
|---|---|---:|---:|
| GCC | Off | 3.12 → 9.23 | 31.72 → 36.54 |
| GCC | On | 3.05 → 9.34 | 31.65 → 36.40 |
| Clang | Off | 3.15 → 9.29 | 39.36 → 45.02 |
| Clang | On | 3.11 → 9.21 | 39.51 → 45.39 |

The measured compiler paid about 6 ms more in On1x emission on this fixture.
These serial, unpinned runs are not an estimate of cost on larger programs.
