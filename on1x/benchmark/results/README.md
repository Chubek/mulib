# Observed Linux AMD64 run, 2026-11-21

`linux-amd64-2026-11-21.json` is the actual complete report copied from:
`/mnt/andromeda/tempdir/opencode/on1x-benchmark-measured-v3/report.json`.

Full command journals, generated `.on1x`, executable snapshots, MIR/pass artifacts
and logs remain in that run directory. Absolute artifact paths in the archived
report refer to those original files. The MuLL `analyze` command reads this
archived report independently of those files.

Exact invocation:

```sh
/mnt/andromeda/tempdir/opencode/on1x-benchmark-build/on1x-benchmark run \
  --config on1x/benchmark/config/measurement.json \
  --output /mnt/andromeda/tempdir/opencode/on1x-benchmark-measured-v3
```

Provenance:

* MuLIB HEAD: `94b27511328c61a219c90cf531ca68e952bc2a85` with concurrent working
  changes explicitly recorded by the report; the revision alone does not
  identify the built compiler.
* LibStone HEAD: `5d733d69f19fac1d8a05294fe54e4a4ec863fc4c`.
* Compiler snapshot SHA-256:
  `45ab5ff29b115f2bcc0a800ab29997fa9d3fa404210994fb623618dc9ba936c0`.
* Native bridge snapshot SHA-256:
  `d2b97b2403663996686eef8d83d278e9fe8e5bffa1e5112717eb72343bc02673`.
* GCC `16.2.1 20260810`, C flags `-O2`, Linux `6.18.52-1-lts`, x86_64.
* 10,000 iterations, seed 1729; runtime 3 warmups/15 samples; compilation
  1 warmup/5 samples. Linux pidfd process-completion measurement.
* All 21 fixture/backend combinations passed, including the correctness-only
  trap. Six workloads have runtime distributions; no runtime samples for traps.

Constants native runtime median: **2.125754 ms unoptimized → 0.844947 ms
optimized**; MIR instructions **581 → 152**, executable **87,096 → 74,808 bytes**.
Compilation median increased **74.993051 → 141.213274 ms**. This is an observed
tradeoff on one machine, not a universal speedup claim. See
[`../OPTIMIZATION_FEEDBACK.md`](../OPTIMIZATION_FEEDBACK.md) and the raw
distributions for the other workloads.

Verification also completed:

* Five CTest tests: MuLL JSON/statistics/oracles/MIR metrics; real host process
  argv/redirection/nonzero/timeout/spawn-error behavior; three rejection tests.
* Solo C11 recurrence/trap and solo optimized native recursive-calls runs.
* Missing C tool rejects with exit 1 and a report containing a failed setup.
* MuLL re-analysis of the full report and self-comparison (18 zero-change pairs).

Earlier interrupted directories are not complete benchmark results:
`...-measured` hit the MuLL cumulative frame limit during final serialization;
the encoder was fixed to use standard-library text primitives.
`...-measured-v2` encountered a concurrent compiler relink; tools are now
snapshotted before measurements.

## Eight-workload continuation

`eight-workloads-linux-amd64-2026-11-21.json` and its `.analysis.json` companion
are new reports from `/mnt/andromeda/tempdir/opencode/on1x-benchmark-eight-final`.
They do not replace or change the historical seven-workload data above or
worker8's separate `annihilator-v3-linux-amd64-2026-11-21.json`.

```sh
/mnt/andromeda/tempdir/opencode/on1x-benchmark-build/on1x-benchmark run \
  --config on1x/benchmark/config/measurement.json \
  --output /mnt/andromeda/tempdir/opencode/on1x-benchmark-eight-final
```

The default suite now includes a MuLL-generated annihilator workload with an
independent zero oracle. All **24 combinations passed**, with runtime 3 warmups/
15 samples and compilation 1 warmup/5 samples. It records explicit C and native
targets/ABI, compiled runtime/ABI hashes, runtime/link flags, CPU identity, and
harness hashes for strict future comparison.

For the diagnostic, **unoptimized MIR → the full optimized pipeline** reduced
instructions **29 → 19**, `sub.checked` **1 → 0**, `mul.checked` **2 → 0**, and
`mod.checked` **1 → 0**. Native runtime medians were **0.402388 → 0.369982 ms**;
compile medians **29.503125 → 36.490702 ms**. Runtime p05/p95 intervals overlap:
the analyzer correctly labels this short diagnostic **inconclusive** despite
its deterministic instruction reductions. Its median-only break-even estimate
is 216 whole-program executions, not a guaranteed amortization point.

Constants had nonoverlapping runtime intervals in this run:
**2.112766 → 0.815287 ms**, compile **76.545288 → 140.225095 ms**, estimated
break-even **50 executions**. Per-stage distributions and deltas are in the
analysis companion. These off/on results must not be confused with worker8's
isolated **optimized v2 → optimized v3** experiment at five million iterations.

Nine CTests now pass, including comparability source/target/runtime/machine/
population rejections, malformed stage totals, rejection of legacy reports
without target context, and the C process-group cleanup tests. The first
eight-workload attempt completed samples but exhausted the previous two-million
frame budget while writing its analysis; the documented finite limit is now
three million, and the fresh complete run above passed.

A second independent run at
`/mnt/andromeda/tempdir/opencode/on1x-benchmark-eight-comparison` used the same
command/config with `--repeats 3 --compile-repeats 2`. All 24 gates passed.
Strict cross-run comparison against `...-eight-final/report.json` returned
`compatible: true`, 21 timed workload/backend pairs, no rejections, unchanged
compiler/bridge identities, and exit 0. The resulting
`eight-workloads-strict-comparison.json` is preserved here. Counts may differ;
all required source, target/ABI, runtime, flags, harness and machine identities
matched. These are same-compiler repeatability observations, not an optimization
revision speedup experiment.
