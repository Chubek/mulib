# Reproducible On1x benchmarks in MuLL

The suite executes actual `mudsl/mull` programs through `mull::Runtime`, its
MuLLIER pipeline, and standard-library packages. All benchmark policy lives in
`main.mull` and `lib/*/Package.mull`: fixture generation and independent oracles,
configuration, process orchestration, JSON, MIR metrics, statistical summaries,
and report comparison. `native/main.cpp` is a generic embedding adapter;
`native/bench_host.c` is the POSIX process/timing boundary. No Python runner or
replacement MuLL interpreter is used.

## Build and test

From the repository root:

```sh
cmake -S on1x/benchmark -B /mnt/andromeda/tempdir/opencode/on1x-benchmark-build \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /mnt/andromeda/tempdir/opencode/on1x-benchmark-build --parallel 2
ctest --test-dir /mnt/andromeda/tempdir/opencode/on1x-benchmark-build --output-on-failure
```

The executable loads the checkout's authored `.mull` files at runtime. It needs
C++20, C11, Linux/POSIX, and the existing MuDSL headers. Neither an installed
`mull` command nor the optional MuLIB MuLL bridge is required. The native
benchmark backend additionally requires the real generated Archon toolchain,
`on1xc`, and `on1x-archon-mir`; see
[`../backends/mir-archon/README.md`](../backends/mir-archon/README.md).

## Run

```sh
/mnt/andromeda/tempdir/opencode/on1x-benchmark-build/on1x-benchmark run \
  --config on1x/benchmark/config/smoke.json \
  --output /mnt/andromeda/tempdir/opencode/benchmark-smoke
```

Defaults select the combined compiler/toolchain under
`/mnt/andromeda/tempdir/opencode/on1x-archon-build`. Override with:

```sh
/mnt/andromeda/tempdir/opencode/on1x-benchmark-build/on1x-benchmark run \
  --compiler /path/to/on1xc \
  --archon /path/to/on1x-archon-mir \
  --backend cross --optimization both \
  --iterations 10000 --seed 1729 --warmups 3 --repeats 15 \
  --compile-warmups 1 --compile-repeats 5 --timeout-ms 30000 \
  --cc cc --cflags '["-O2"]' \
  --output /mnt/andromeda/tempdir/opencode/benchmark-measurement
```

`--backend c11` and `--backend mir-archon` run solo. `--optimization off|on|both`
controls native MIR optimization; C11 uses the reference HIR emitter.
`--cases recurrence,branches,gcd,calls,constants,annihilator,tuple,trap` selects fixtures.
Optional processor diagnostics `processor-lazy,processor-identities` exercise
dynamic short-circuit conditions, constant unreachable traps, checked neutral
identities, and boolean/comparison canonicalization. They use bounded periodic
checksum oracles and can run at 1,000,000 iterations without scaling oracle work.
They are not part of the historical default eight-workload population.
`zero-explicit,zero-wrapped` are a separate opt-in pair. Run both in one
cross-backend report, then `on1x-benchmark zero-cost RUN_DIR` for the
correctness and machine-code identity gate. See [ZERO_COST.md](ZERO_COST.md)
for the exact scope, normalization, timings and reproduction command.
`generate` accepts the same options and writes `.on1x`, config, and manifest
without running a compiler. Configuration JSON is merged in argument order;
later command-line options override earlier values. Unknown keys, duplicate
cases, fractional counts, and out-of-budget values reject.

Choose a fresh output directory for each run. Existing run logs reject, including
incomplete runs. Generated fixture-only directories may be used by `run`.
Do not share an output directory between concurrent processes.
Compiler and bridge executables are copied into `tools/` before use and those
copies are hashed, protecting measurements against concurrent developer relinks.
Pass compiler/bridge filesystem paths, not PATH-only names. Start after the
selected executables have finished building.

## Workloads and gates

* **recurrence:** checked arithmetic and loop-carried dependence.
* **branches:** modulo-dependent branch and accumulator updates.
* **gcd:** nested Euclidean loops and direct calls.
* **calls:** recursive Fibonacci call tree, bounded depth 10.
* **constants:** configurable generated foldable expressions and dead branches.
* **annihilator:** optimizer diagnostic for unknown-parameter signed-i64 `x-x`,
  `x*0`, `0*x`, and `x%1` in a counted call loop. The independent oracle is zero.
  This targets identity folding; it is not a representative application.
* **tuple:** tuple-return ABI and destructuring with loop-carried values.
* **trap:** correctness-only ordered output followed by checked division failure.

MuLL independently computes exact expected integer results; intermediates fit
binary64's exact integer range. Every execution, including warmups and measured
repeats, must match exact stdout, stderr, and exit status. Signals and timeouts
fail. Cross-backend equivalence follows from passing the same oracle, including
the trap trace. Failed gates yield no runtime performance summary and the suite
exits nonzero. No unavailable backend silently falls back.

The pipelines are:

* C11: `on1xc --emit-c`, then C compiler/linker.
* Native: `on1xc --emit-ir [--optimize-ir]`, `on1x-archon-mir --ir-input`,
  then assembler/linker with the real scalar ABI and runtime.

Runtime/ABI objects are built once in setup. Compilation samples include source
emission and executable linking, excluding that shared setup and metric probes.
Per-stage timings and full argv remain available. The finalized
[native driver/helper](../backends/mir-archon/DRIVER.md) also supports native
executable builds and `--sdk-emit mir-archon --sdk-target TRIPLE --optimize-ir`.
This harness deliberately invokes the supported standalone stages directly so
their individual costs can be measured.

## Reports and analysis

Each run writes:

* `report.json`: versioned machine-readable results, all measured samples,
  distributions, sizes, source/executable hashes, tool versions and hashes,
  repository/LibStone revisions, dirty state, and machine provenance.
* `commands.json`, `logs/N.{json,stdout,stderr}`: exact command journal, exit
  status, timeout/signal outcomes, nanoseconds, and diagnostic output.
* `manifest.json`, `config.json`, `fixtures/*.on1x`: reproducible inputs/oracles.
* `artifacts/*.before.mir`, `*.after.mir`, `*.passes.sexpr`: actual optimization
  inputs/outputs and compiler pass provenance. Metrics count functions, blocks,
  edges, instructions and opcodes while excluding quoted source locations.
* `summary.md`, `analysis.json`: human table, correctness-gated C11/native median
  ratios, per-compilation-stage distributions, and optimization cost tradeoffs.
* `artifacts/*.result.json`: completed per-variant records retained even if a
  later host failure interrupts the run.

```sh
/mnt/andromeda/tempdir/opencode/on1x-benchmark-build/on1x-benchmark analyze \
  /path/to/run/report.json
/mnt/andromeda/tempdir/opencode/on1x-benchmark-build/on1x-benchmark compare \
  /path/to/baseline/report.json /path/to/current/report.json
```

Analysis recomputes summaries from raw samples. Cross-run comparison requires
matching fixture **source SHA-256**, iteration count, seed, C flags, warmup policy,
deadline, machine/CPU model, C compiler target/version, native target/ABI,
compiled runtime/ABI object hashes, runtime flags, link flags, harness source
hashes, and measurement methodology. Live CPU frequency in `lscpu` is excluded
from machine identity. Different output directories are allowed: digest values,
not hash-command filenames, identify sources and objects.

The comparison v2 JSON gives per-row rejection reasons and emits no ratio for
incomparable rows. Duplicate fixture/backend rows, missing metadata, unavailable
identity probes, failed gates, and changed workloads reject; exit status is 1
if the comparison is incompatible. Compiler/bridge binary changes may be the
experiment being measured, so they are explicitly disclosed rather than
automatically rejected once target/ABI and other controls match. Sample counts
may differ; inspect raw distributions before assigning causality.

Older archived reports without the new `comparison_context` remain analyzable
but **cannot pass strict cross-run comparison**, even against themselves.
Historical reports are not rewritten to invent missing target evidence.

Optimization analysis validates per-stage durations against each compilation's
recorded total, then compares unoptimized/optimized native variants. It reports
additional compile time, runtime savings, stage-by-stage median changes, and
`ceil(max(0, extra_compile_time) / runtime_saving)` break-even **whole-program
executions**. Break-even is null without a runtime improvement. Fewer than three
samples or overlapping p05/p95 intervals yield an inconclusive assessment.
Interval separation is descriptive, not a hypothesis test or an automatic
recommendation to enable a pass.
The JSON reader accepts the emitted report profile: raw UTF-8 and ASCII `\u`
escapes, not escaped non-ASCII Unicode/surrogate pairs.

`CLOCK_MONOTONIC` timings include process startup and reaping. Workloads run
serially in fixture/backend order, without affinity, cache flushing, or frequency
control. Short workloads are often startup/noise dominated. Quantiles use linear
interpolation; summaries include sample standard deviation, coefficient of
variation, median absolute deviation, p05/p95, min/max, and mean. Ratios are
observations, not statistical significance claims or kernel-only timings.
The C helper uses Linux pidfd readiness when available; other POSIX hosts/older
kernels use a 1 ms polling fallback, which can dominate short samples. The
embedding runtime bounds execution at 50 million evaluator steps, three million
cumulative frames, and 16 MiB per host file. Exhaustion is an error, not a sample.

## Measured evidence and source split

The [archived runs](results/README.md) preserve the historical seven-workload
report and add a new eight-workload report with **24 passing combinations**.
[Optimization feedback](OPTIMIZATION_FEEDBACK.md)
records useful runtime/code-size improvements and their compilation-cost tradeoff.
[Processor feedback](PROCESSOR_FEEDBACK.md) records the separate baseline versus
On1xMETA comparison, including all three backend variants, long-running opt-in
diagnostics, reverse-order confirmation, regressions and timing uncertainty.
[Second-round optimization evidence](OPTIMIZATION_ROUND2.md) records the
canonical boolean SSA-lowering follow-up: matched compiler configurations,
84 passing benchmark combinations, reversed-order native improvements,
byte-identical C11 controls, and complete regression/compile-cost evidence.

At delivery, physical implementation lines (`wc -l`) were:

| Language | Lines |
|---|---:|
| Authored MuLL, including tests | 1,415 |
| C process helper | 115 |
| C API header | 19 |
| C++ genuine-runtime embedding adapter | 123 |
| Shared C helper tests (`../tests/test_bench_host.c`) | 96 |

MuLL is **80.03%** of these 1,768 lines; C/C++ including tests is **19.97%**.
This includes the shared C test outside this directory, rather than hiding its
cost. Within `benchmark/` alone, MuLL is **84.6%**. Excluding all three MuLL test
packages and the shared C test gives 1,223 MuLL / 257 native lines: **82.6% MuLL**
for the implementation and embedding boundary.

Excluding blank and whole-line comments across all these files gives
1,393 MuLL / 332 native source lines (**80.8% MuLL**). The extra MuLL implements
comparability enforcement, stage-cost analysis, opcode deltas, the annihilator
diagnostic, and their positive/negative tests. Documentation, JSON evidence/
configuration, CMake, generated On1x/MIR/C/assembly, and the existing MuDSL
implementation are excluded. C++ embedding remains explicitly disclosed.
