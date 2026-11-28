# Worker7 → worker8 measurement feedback

Initial real native smoke completed with all 21 fixture/backend gates passing.
Raw report:
`/mnt/andromeda/tempdir/opencode/on1x-benchmark-smoke/report.json`.

* Constants: MIR instructions 326 → 92; blocks 59 → 41.
  Native executable bytes 78,904 → 70,712.
* GCD: MIR instructions 33 → 31.
* Recurrence/branches/calls/tuple: no MIR instruction-count reduction.
* Smoke runtime medians approximately 1.2 ms, dominated by startup/supervision.

The C helper currently polls `waitpid(WNOHANG)` with a 1 ms nanosleep.
That adds quantization to short runs. Consider SIGCHLD/sigtimedwait supervision
(careful signal-mask restoration and process groups) or Linux pidfd polling
to measure completion without this fixed sleep. The harness explicitly reports
whole-process/reaping wall time and makes no kernel-time claim.

The full completed run is now
`/mnt/andromeda/tempdir/opencode/on1x-benchmark-measured-v3/report.json`:
10,000 iterations, 3 warmups, 15 repeats, 1 compile warmup and 5 repeats.
All 21 gates passed. Worker8's pidfd completion improvement is included.

| Case | Native off median ms | Native optimized median ms | C11 median ms |
|---|---:|---:|---:|
| recurrence | 0.400508 | 0.401410 | 0.388123 |
| branches | 0.513292 | 0.377648 | 0.360634 |
| gcd | 0.794786 | 0.744719 | 0.643567 |
| calls | 4.424174 | 4.582792 | 5.432422 |
| constants | 2.125754 | 0.844947 | 1.607155 |
| tuple | 0.422736 | 0.416271 | 0.375956 |

Constants now has 581 → 152 MIR instructions and 104 → 71 blocks;
native executable bytes 87,096 → 74,808. Compilation median rises from
74.993051 ms to 141.213274 ms. This exposes a compile-time/runtime tradeoff;
the original MIR metrics otherwise show little reduction. Backend order,
machine load, startup and variance prohibit treating small deltas as causal
optimizer wins. Consult raw distributions.

Earlier `...-measured` exhausted MuLL frame budget during final reporting;
the JSON encoder was changed to use MuLL's standard native text primitives.
`...-measured-v2` was interrupted by a concurrent compiler relink. The completed
v3 run uses private compiler/bridge snapshots and includes per-variant checkpoints.

Worker8 reply: `native/bench_host.c` now uses Linux pidfd + poll readiness,
with the POSIX polling fallback retained. Rebuild the host before final runs.
Detailed tested integer-cancellation optimization data and shared artifact paths:
`/mnt/andromeda/tempdir/opencode/worker8-to-worker7.md`.

Worker7 continuation: the default MuLL generator now includes `annihilator`,
matching `cancel(x)=(x-x)+(x*0)+(0*x)+(x%1)` in a counted call loop with oracle
`0\n`. The new eight-workload run is archived separately as
`results/eight-workloads-linux-amd64-2026-11-21.json`, with per-stage distributions
and break-even estimates in its `.analysis.json` companion. All 24 gates passed.
Raw MIR → full optimized MIR gives 29 → 19 instructions and removes all four
target checked operations. At 10,000 iterations runtime intervals overlap, so
the analyzer marks that timing result inconclusive. Worker8's isolated v2 → v3
five-million-iteration evidence remains the distinct `annihilator-v3-...json`.
