# Canonical boolean SSA lowering: second optimization round

**Completed:** 84/84 benchmark combinations passed, and all four strict
cross-run comparisons were compatible. On the one-million-iteration lazy
diagnostic, native-off runtime fell **7.1–9.8%**, native-opt **4.6–6.1%**,
and optimized total compilation **22.7–26.0%**, confirmed in reversed order.
These are targeted microbenchmark results, not general application speedups.

## Baseline and scope

This round compares the current On1xMETA compiler against a narrow canonical
boolean-region lowering improvement. The earlier processor measurements remain
in [PROCESSOR_FEEDBACK.md](PROCESSOR_FEEDBACK.md); their pre-On1xMETA baseline is
**not** this round's baseline.

Before implementation/build changes, the benchmark worker captured the stable
compiler, bridge, harness, compiler cache, processor sources/generated tables,
source/toolchain hashes and dirty-tree patch at:

`/mnt/andromeda/tempdir/opencode/on1x-opt2-provenance/baseline/`

Capture: 2026-11-21 06:58:25 UTC. Repository HEAD:
`94b27511328c61a219c90cf531ca68e952bc2a85`, with pre-existing uncommitted work.

| Artifact | SHA-256 |
|---|---|
| Baseline `on1xc` | `06ec2d05af95a38c25b1aea5226f54dc7d140ae80265e24528e3bde64e335b86` |
| Candidate `on1xc` | `beb339dbf2417c55239789eba68dfbc88a61ce3971672c07451a6d71ec4aba3e` |
| Candidate `src/backend/lower.cpp` | `95d8392fad47d29f7adbdc3477ce62cb15e0dfb731878e5013c2e6a601167edc` |
| Shared Archon bridge | `d2b97b2403663996686eef8d83d278e9fe8e5bffa1e5112717eb72343bc02673` |
| Genuine MuLL harness | `12e6a460cd4f2708bffa19c6f7b2cdf88cd0c88b6d50b4417be908b7a54aab9d` |
| Baseline processor program | `0af30583209e3a231fa2ed2772646fe70de6479a03b02b420f25dfd2e8cb449b` |

The private harness's nine CTests pass; the full log is
`on1x-opt2-provenance/harness-ctest.log`. The parent owns candidate builds and
compiler regression tests; the benchmark worker owns timing and oracle gates.
The shared stable compiler build and all previous reports are preserved.

Relevant CMake options match exactly (`cache-differences.json` is empty).
Processor rules and `passes.cpp` hashes are unchanged. The production change
is confined to MIR lowering. Direct HIR-C11 is a negative control; C11 timing
differences with byte-identical output cannot be attributed to this change.

## Regression gates

The full candidate CTest run completed all 213 tests in 173.67 seconds:
**209 passed, one skipped, three failed**. The known baseline
`on1x.addon_chatgpt` failure is `Noncanonical kernel fields`;
`on1x.container_pod_cgroup` remains the intentional environment skip.

The other two failures (`on1x.passes_hir`, `on1x.passes`) shared a shape-dependent
test assumption: a lazy source expression had to retain storage long enough to
require later-round CFG cleanup. Its existing output, dead-trap elimination
and fixed-point assertions already passed with SSA lowering.

The repaired test retains those original source assertions and the exact
later-round CFG-commit obligation, moving the latter onto an explicit mutable
boolean fixture. It checks the initial load/stores/division, confirms that
excluding LocalSlots retains the division, and requires the complete pipeline
to remove the load/division, preserve output, and reach an idempotent result.
No production compiler change was made for this repair.

The two affected tests then passed **2/2 in 26.79 seconds**. The resulting
tested union is **211 passed, one skipped, one known baseline failure**;
this is not a second full-suite run. Logs:
`on1x-opt2-provenance/{full-ctest-complete,passes-rerun}.log`.
An initial one-second harness-timeout attempt is separately retained as
`full-ctest.log`; it is not counted as a completed suite.

Agent2 also reported both new focused decision tests passing, including 21
C11/MIR/evaluator/native execution profiles, baseline-failing structural gates,
and ASan/UBSan for the changed lowering translation unit and focused C++ tests.
Its full command/artifact evidence is maintained by the implementation owner.

### Emitted MIR structure

Counts are baseline→candidate. The source fixtures are unchanged.

| Fixture | MIR | Instructions | Blocks | Slots | Loads / stores |
|---|---|---:|---:|---:|---:|
| lazy | unoptimized | 63→46 | 23→18 | 6→2 | 10/13→6/5 |
| lazy | optimized | 44→32 | 20→16 | 6→2 | 7/10→5/4 |
| identities | unoptimized | 31→26 | 10→8 | 3→2 | 6/6→5/4 |
| identities | optimized | 30→25 | 10→8 | 3→2 | 5/6→4/4 |

Raw emitted MIR and preliminary counts/argv:
`on1x-opt2-provenance/structural/`. Final benchmark reports independently
confirm the instruction/block/load/store counts using the genuine MuLL
metrics package. Both unreachable lazy divisions are still removed by the
optimizer.

## Matched measurement protocol

The existing real MuLL harness runs direct HIR-C11, native MIR without
optimization, and native MIR with optimization. Exact independent stdout,
stderr and status gates apply to every execution.

* Historical eight workloads: 10,000 iterations.
* `processor-lazy` and `processor-identities`: 1,000,000 iterations, bounded
  independent periodic checksum oracles.
* Seed 1729; GCC `/usr/bin/cc`, `-O2`; runtime 3 warmups/15 samples;
  compilation 1 warmup/5 samples.
* First paired cycle: baseline then candidate. Confirmation: candidate then
  baseline for targeted diagnostics and recurrence/branches controls.
* Fresh `on1x-opt2-*` output directories, private compiler snapshots, identical
  bridge/runtime/ABI/fixture/harness inputs, and strict MuLL comparisons.
* Timing windows exclude coordinated compiler builds and tests. Whole-process
  samples retain startup/reaping and ordinary host contention; no CPU affinity
  or frequency control is imposed.

Reproduction runner:
`/mnt/andromeda/tempdir/opencode/on1x-opt2-provenance/run-paired.sh`.

The paired measurements ran after the regression and sanitizer handoffs.
The complete result set is eight reports: 24+24 historical combinations,
6+6 initial targeted combinations, 6+6 reversed targeted combinations, and
6+6 reversed recurrence/branches control combinations. No populations were
pooled or unsuccessful samples discarded.

Host: Linux `6.18.52-1-lts`, Intel i7-11700K, x86-64; GCC
`16.2.1 20260810`, C target `x86_64-pc-linux-gnu`, Archon target
`x86_64-unknown-linux-gnu`. LibStone:
`5d733d69f19fac1d8a05294fe54e4a4ec863fc4c`.
Runtime/ABI objects use `-std=c11 -O2`; links use
`-std=c11 -O2 -no-pie -lm`.

## Runtime results

Milliseconds, median [p05,p95]. Delta is `(candidate/baseline - 1) * 100`.
“First” is baseline→candidate; “reverse” is candidate→baseline.

| Diagnostic | Backend | Order | Baseline | Candidate | Delta | CV baseline/candidate |
|---|---|---|---:|---:|---:|---:|
| lazy | C11 | first | 6.781 [6.692,7.087] | 6.754 [6.675,6.859] | −0.4% | 2.2% / 1.0% |
| lazy | C11 | reverse | 6.812 [6.724,6.916] | 6.727 [6.630,6.774] | −1.3% | 1.0% / 0.8% |
| lazy | native-off | first | 12.639 [12.254,13.032] | 11.398 [11.288,11.603] | −9.8% | 2.0% / 1.1% |
| lazy | native-off | reverse | 12.316 [12.182,12.643] | 11.442 [11.282,11.727] | −7.1% | 1.3% / 1.4% |
| lazy | native-opt | first | 10.695 [10.570,10.955] | 10.205 [10.070,10.496] | −4.6% | 1.5% / 1.5% |
| lazy | native-opt | reverse | 10.809 [10.602,11.136] | 10.148 [10.052,10.303] | −6.1% | 1.6% / 0.9% |
| identities | C11 | first | 3.780 [3.677,3.861] | 3.826 [3.646,3.922] | +1.2% | 1.8% / 2.2% |
| identities | C11 | reverse | 3.854 [3.805,3.910] | 3.849 [3.747,3.920] | −0.1% | 1.0% / 1.5% |
| identities | native-off | first | 7.243 [7.108,7.474] | 6.723 [6.621,6.826] | −7.2% | 1.6% / 1.2% |
| identities | native-off | reverse | 7.159 [7.015,7.438] | 6.796 [6.625,6.876] | −5.1% | 1.9% / 1.2% |
| identities | native-opt | first | 6.900 [6.783,7.027] | 6.382 [6.249,6.486] | −7.5% | 1.3% / 1.4% |
| identities | native-opt | reverse | 6.954 [6.741,7.143] | 6.320 [6.266,6.500] | −9.1% | 1.9% / 1.3% |

All eight targeted native comparisons have separated intervals; all four
targeted C11 intervals overlap. This supports the narrow native improvement
and shows no targeted runtime regression. Interval separation is descriptive,
not a significance test.

### Negative controls and variability

All **14 paired direct-C11 source files and 14 paired executables** are
byte-identical. C11 timing differences are host/process variation, not an
improvement attributable to MIR lowering. All historical-eight executables
(including native-off/native-opt) and reversed recurrence/branches control
executables are also byte-identical. Only the targeted native executables
change. Exact hashes are in `c11-controls.json` and `executable-controls.json`.

Every historical-eight runtime interval overlaps; median deltas range
−4.7% to +3.4%. Every reversed-control interval overlaps too. Notable retained
noise includes historical candidate native `calls` (CV 225.3%, p95 32.363 ms
against a 4.456 ms median) and reversed candidate recurrence C11
(+17.9% median despite the identical executable, overlapping intervals).
The data establish no general runtime improvement or regression.

## Compilation and per-stage costs

Compilation includes source emission, native assembly generation and linking,
excluding common runtime setup and structural metric probes.

| Diagnostic | Backend | Order | Baseline total ms [p05,p95] | Candidate total ms [p05,p95] | Delta |
|---|---|---|---:|---:|---:|
| lazy | native-off | first | 34.641 [33.842,35.666] | 32.614 [32.033,32.979] | −5.9% |
| lazy | native-off | reverse | 35.124 [34.928,35.598] | 32.834 [32.137,33.053] | −6.5% |
| lazy | native-opt | first | 72.803 [71.516,73.478] | 53.903 [53.405,54.548] | −26.0% |
| lazy | native-opt | reverse | 71.154 [70.719,72.323] | 55.035 [52.612,55.548] | −22.7% |
| identities | native-off | first | 30.699 [30.032,31.186] | 29.942 [29.404,30.543] | −2.5% |
| identities | native-off | reverse | 30.590 [30.285,31.420] | 30.115 [29.700,30.337] | −1.6% |
| identities | native-opt | first | 40.695 [40.187,40.849] | 38.165 [37.990,38.793] | −6.2% |
| identities | native-opt | reverse | 41.092 [40.889,41.190] | 38.685 [38.191,39.731] | −5.9% |

The primary optimized-lazy improvement is in the `MIR lower` stage, which
includes the requested optimizer:

| Order | Stage | Baseline ms [p05,p95] | Candidate ms [p05,p95] |
|---|---|---:|---:|
| first | MIR lower + optimization | 53.542 [52.247,53.898] | 35.023 [34.943,35.800] |
| reverse | MIR lower + optimization | 52.383 [51.857,53.231] | 35.220 [34.610,36.047] |
| first | Archon emit | 4.212 [4.086,4.532] | 3.756 [3.700,3.798] |
| reverse | Archon emit | 4.141 [4.110,4.296] | 3.760 [3.698,4.594] |
| first | assemble/link | 15.047 [14.839,15.445] | 15.022 [14.716,15.075] |
| reverse | assemble/link | 14.620 [14.248,15.514] | 15.200 [14.210,15.780] |

This recovers much of the earlier processor lazy-optimization cost without
changing the processor rules, optional-checker settings or pass implementation.
The new baseline has matched optional-checker configuration, unlike the old
pre-On1xMETA comparison.

The previously observed intermittent Archon-emission delay recurred during the
historical-eight run: for example, recurrence native-off emission medians were
2.798→214.379 ms despite the pinned identical bridge and identical resulting
executable. Those total compile ratios do not measure a lowering slowdown.
Reversed controls avoided the ~210 ms delay but recurrence native-off still
had noisy compilation (28.369→31.438 ms, candidate CV 31.4%, spread across
lowering, emission and linking). These samples are retained; no blanket
compile-time non-regression claim is made. Full per-stage distributions for
every fixture/backend/order are in `tables.md` and each `analysis.json`.

## Reproduction and artifact index

All external artifacts below are beneath `/mnt/andromeda/tempdir/opencode/`.

```sh
# Run only in a fresh namespace with stable snapshots and a quiet handoff.
bash on1x-opt2-provenance/run-paired.sh first
bash on1x-opt2-provenance/run-paired.sh reverse
python3 on1x-opt2-provenance/summarize.py
```

The scripts use absolute paths internally; existing measurement directories
reject rather than overwriting reports. For another experiment, choose a fresh
prefix. `summarize.py` only renders existing MuLL results; it does not generate
fixtures, run benchmarks, compute oracle gates or implement comparison policy.

* `on1x-opt2-{first-eight,first-targeted,reverse-targeted,reverse-controls}-{baseline,candidate}/`:
  eight full reports, raw samples, exact command journals/logs, generated
  fixtures, C/MIR/assembly/executables, private tools and MuLL analysis.
* `on1x-opt2-provenance/*-comparison.json`: four compatible strict comparisons.
* `on1x-opt2-provenance/{tables.md,report-hashes.json,c11-controls.json,executable-controls.json}`:
  complete runtime/compile/stage/structure tables and artifact identities.
* `on1x-opt2-provenance/{baseline,candidate}/`: private snapshots, source/build
  provenance and patches; `final-test-evidence/` retains the final focused and
  pass-fixture test sources/hashes.
* `on1x-opt2-provenance/*.start.txt`, `*.end.txt`, `*.processes.txt`:
  measurement intervals and host contention snapshots.
* `on1x-opt2-provenance/{full-ctest-complete,passes-rerun,harness-ctest}.log`:
  regression and harness evidence.

Both shared compiler SHA-256 values were rechecked unchanged after all eight
reports. The quiet reservation is released. Further cycles should be triggered
by a new relevant candidate or unresolved failure, rather than repeating until
a preferred timing appears.
