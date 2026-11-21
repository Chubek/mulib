# On1xMETA processor benchmark feedback

Worker5 owns this ongoing record. Previous reports and
`OPTIMIZATION_FEEDBACK.md` remain historical evidence.

**Completed stable2:** 84/84 measured fixture/backend combinations passed all
correctness gates; all four strict comparisons were compatible. The historical
eight-workload runtime intervals overlap throughout. Targeted neutral-identity
rewrites reduce runtime on this diagnostic, while dynamic lazy conditions show
a reproducible approximately 3% unoptimized-native regression and higher
optimized compilation cost. Details, uncertainty and both run orders follow.

## Measurement plan and scope

The real MuLL harness measures direct HIR→C11 (`c11`), unoptimized native
(`mir-archon`), and optimized native (`mir-archon-opt`). It does not currently
measure MIR→C11. Native assembly is emitted by the installed Archon toolchain
through the standalone bridge, not by a C lowering.

Baseline compiler: `/mnt/andromeda/tempdir/opencode/on1x-archon-build/on1xc`.
Candidate: `/mnt/andromeda/tempdir/opencode/on1x-meta-build/on1xc`, only after
parent confirmation of a stable build and worker4's full-CTest quiet-window
handoff. Neither shared build is rebuilt by this worker.
Installed native SDK: `/mnt/andromeda/tempdir/opencode/on1x-archon-toolchain/install`.

Run the eight existing fixtures at 10,000 iterations, seed 1729, runtime
3 warmups/15 samples and compilation 1 warmup/5 samples. Use identical C compiler,
`-O2`, runtime/ABI sources, bridge, target and harness for both runs. Every warmup
and measured run must pass exact output/error/status gates. Traps are
correctness-only. Report distributions and compilation separately; small
startup-dominated deltas are not optimizer speed evidence.

Opt-in additions: `processor-lazy` checks dynamic nested short-circuit conditions
and constant lazy unreachable division; `processor-identities` checks `+0`,
`-0`, `*1`, `/1`, double negation and comparison canonicalization. Their
independent periodic checksums allow longer runs without enlarging generated
source or oracle work. These are diagnostics, not representative applications.

## Coordination and initial findings

All four implementation workers and the parent were contacted directly.
Worker2 identified canonical lazy Select storage as blocking dead-branch
optimization and implemented bounded unique-predecessor LocalSlots forwarding.
Worker1 fixed generated-header include precedence; prior binaries can report
zero semantic hits and are not final comparison candidates. Worker3 recommends
separating the new verification/traversal compile cost from executable runtime.
Worker4 will hand off after the full compiler suite.

Initial source/rule hashes, build caches and dirty-tree patch are preserved in
`/mnt/andromeda/tempdir/opencode/on1x-meta-benchmark-provenance/`.
The repository HEAD is `94b27511328c61a219c90cf531ca68e952bc2a85` with working
changes; HEAD alone does not identify either compiler. The initial baseline
compiler SHA-256 is
`eccb3adec9906444c5118ca22dd1f7e257c08d16cd0fe6784219a396330d2c0b`.

A continuously busy `ezcap-daemon` and desktop/browser processes were observed.
The quiet window coordinates compiler builds/tests only; it is not an otherwise
idle, frequency-controlled or CPU-pinned machine. Record this residual
contention rather than attributing every timing delta to the compiler.

## Preparation gates and structural feedback

Private Release harness build and all nine harness CTests passed. Both compiler
snapshots passed all six new-fixture/backend combinations at 101 iterations;
those exploratory runs overlapped broad compiler testing and their timings are
not performance evidence.

Stable1 current compiler SHA-256:
`075474f2a8d4a53ffba973bf62e071bbe8be766ccd629a76239ed074be1b026c`.
The same bridge is used for both compilers:
`d2b97b2403663996686eef8d83d278e9fe8e5bffa1e5112717eb72343bc02673`.
Private copies, source/rule/installed-toolchain hashes and both CMake caches are
in the provenance directory's `stable1/` child. Both compilers are Debug GCC
builds, but baseline Chisel/Nerotypical are OFF and current ON. Compilation
deltas therefore describe the combined build change, not an isolated causal
processor overhead measurement.

Processor reports confirm active semantic checks, program hash
`ccdbc0f712e3352982f95f2ea129795d0930b5a534f8d666be7d4a9b118d4cbd`
and parser hash
`9e3877b37bf83675fb51e33d1d02105e84ee58d4054f527317a61fdd1d81d358`.

| Diagnostic, optimized MIR | Baseline | Stable1 |
|---|---:|---:|
| lazy: instructions / blocks | 31 / 17 | 44 / 20 |
| lazy: loads / stores | 5 / 4 | 7 / 10 |
| lazy: remaining checked divisions | 0 | 0 |
| identities: instructions | 35 | 30 |
| identities: stores | 4 | 6 |

The LocalSlots correction recovers dead checked-division removal, but dynamic
Select regions retain additional storage/control flow. Investigate verified
slot promotion or direct conditional lowering if long-run timing confirms a
regression. Do not forward across multiple predecessors without a dominance/
phi proof, or weaken effect, trap, loop or aliasing gates. These counts are
structural findings, not runtime speed claims; they were relayed to all workers.

Exact finite-cycle runner (execute only after the quiet-window handoff):

```sh
bash /mnt/andromeda/tempdir/opencode/on1x-meta-benchmark-provenance/run-stable1.sh
```

It invokes the real harness serially for baseline/current default eight cases,
then baseline/current opt-in diagnostics at one million iterations. Both use
`--config on1x/benchmark/config/measurement.json --cc /usr/bin/cc
--cflags '["-O2"]'`, private compiler/bridge snapshots, and fresh output folders
`on1x-meta-benchmark-stable1-{eight,targeted}-{baseline,current}`. Strict harness
comparison checks source/inputs/runtime objects/target/flags/harness identities.
Each run's command journal records every exact compile, link and execution argv.

## Final measurement snapshot

Worker4 explicitly handed off the quiet window after its full suite and focused
diagnostic-message rerun. The final compiler matrix was reported as 209 passed,
one skipped and one independently reproduced baseline addon failure out of 211;
the affected 11 tests and sanitizer profile also passed. A diagnostic wording
correction changed rule/compiler hashes after stable1 preparation, so stable1
remains exploratory and **was never used for final timed comparisons**.

Final stable2 compiler:
`06ec2d05af95a38c25b1aea5226f54dc7d140ae80265e24528e3bde64e335b86`.
Final processor program:
`0af30583209e3a231fa2ed2772646fe70de6479a03b02b420f25dfd2e8cb449b`.
Parser and baseline compiler/bridge hashes are unchanged.

The final cycle uses the same script above with `stable1` replaced by `stable2`:

```sh
bash /mnt/andromeda/tempdir/opencode/on1x-meta-benchmark-provenance/run-stable2.sh
```

## Completed measurements

Host: Linux `6.18.52-1-lts`, x86_64, Intel i7-11700K (8 cores/16 threads).
C compiler: `/usr/bin/cc`, GCC `16.2.1 20260810`,
target `x86_64-pc-linux-gnu`; Archon target `x86_64-unknown-linux-gnu`.
LibStone: `5d733d69f19fac1d8a05294fe54e4a4ec863fc4c`.
Runtime and ABI objects use `-std=c11 -O2`; application links use
`-std=c11 -O2 -no-pie -lm`. Strict comparison accepted identical source hashes,
input counts, seed, runtime/ABI object hashes, toolchain targets, flags, harness
hashes and machine identity in all four pairs.

The first cycle ran baseline then current: eight fixtures at 10,000 iterations
(24 combinations each), then two diagnostics at 1,000,000 (6 each).
The constants fixture has 33 generated branches; the other fixtures retain
bounded source sizes. Compilation: 1 warmup/5 measured samples. Runtime:
3 warmups/15 measured samples, plus the initial correctness execution.
The trap has no runtime distribution. All executions check exact stdout,
stderr and exit status.

The second cycle deliberately reversed compiler order, current then baseline,
for both targeted diagnostics and the recurrence/branches controls. This was
triggered by first-cycle runtime outliers and intermittent native-emission
delays, not a search for favorable results. All first-cycle samples are retained.
Total: **84/84** passing combinations across eight completed reports.

Reproduction of the confirmation cycle:

```sh
bash /mnt/andromeda/tempdir/opencode/on1x-meta-benchmark-provenance/run-stable2-confirm.sh
```

Equivalent individual measurement commands, from the repository root:

```sh
BASE=/mnt/andromeda/tempdir/opencode
H="$BASE/on1x-meta-benchmark-harness/on1x-benchmark"
T="$BASE/on1x-meta-benchmark-provenance/stable2"
# Repeat with baseline/current in that order; choose a fresh output directory.
"$H" run --config on1x/benchmark/config/measurement.json \
  --compiler "$T/baseline-on1xc" --archon "$T/on1x-archon-mir" \
  --cc /usr/bin/cc --cflags '["-O2"]' \
  --output "$BASE/on1x-meta-benchmark-stable2-eight-baseline"
# Targeted cycle: add these options to each baseline/current command:
# --cases processor-lazy,processor-identities --iterations 1000000
# Confirmation: current then baseline, with fresh ...-confirm-targeted-* outputs.
# Control confirmation: --cases recurrence,branches --iterations 10000
"$H" compare "$BASE/on1x-meta-benchmark-stable2-eight-baseline/report.json" \
  "$BASE/on1x-meta-benchmark-stable2-eight-current/report.json"
```

### Historical eight: whole-process runtime

Milliseconds, median [p05,p95]; delta is `(current / baseline - 1) * 100`.
`native` means unoptimized MIR, `native-opt` means `--optimize-ir`.
All 21 p05/p95 intervals overlap; median deltas range −4.1% to +2.1%.
These results establish no general runtime improvement or regression.
Short cases include substantial process-startup cost.

| Fixture | Backend | Baseline | Current | Delta | CV baseline/current |
|---|---|---:|---:|---:|---:|
| recurrence | C11 | 0.377 [0.359,0.442] | 0.384 [0.357,0.425] | +1.7% | 9.0% / 7.4% |
| recurrence | native | 0.403 [0.350,0.448] | 0.410 [0.380,0.479] | +1.7% | 8.5% / 9.2% |
| recurrence | native-opt | 0.411 [0.383,0.519] | 0.407 [0.384,0.461] | −0.9% | 12.6% / 6.8% |
| branches | C11 | 0.352 [0.325,0.386] | 0.360 [0.343,0.420] | +2.1% | 5.8% / 7.3% |
| branches | native | 0.371 [0.364,0.430] | 0.376 [0.339,0.450] | +1.3% | 6.2% / 8.8% |
| branches | native-opt | 0.371 [0.354,0.423] | 0.379 [0.360,0.429] | +2.0% | 6.9% / 6.8% |
| gcd | C11 | 0.631 [0.611,0.663] | 0.643 [0.625,0.711] | +1.9% | 3.1% / 7.3% |
| gcd | native | 0.786 [0.741,0.843] | 0.753 [0.726,0.907] | −4.1% | 4.7% / 9.0% |
| gcd | native-opt | 0.783 [0.705,0.891] | 0.783 [0.714,0.819] | −0.0% | 8.6% / 4.5% |
| calls | C11 | 5.308 [5.240,5.420] | 5.348 [5.231,5.428] | +0.8% | 1.2% / 1.2% |
| calls | native | 4.454 [4.382,4.680] | 4.471 [4.421,4.583] | +0.4% | 2.1% / 1.4% |
| calls | native-opt | 4.491 [4.397,4.539] | 4.494 [4.391,4.562] | +0.1% | 1.1% / 1.4% |
| constants | C11 | 1.644 [1.530,1.727] | 1.666 [1.601,1.734] | +1.4% | 3.9% / 3.1% |
| constants | native | 2.117 [2.044,2.206] | 2.096 [2.062,2.195] | −1.0% | 2.6% / 2.3% |
| constants | native-opt | 0.845 [0.804,0.886] | 0.832 [0.780,0.961] | −1.6% | 3.3% / 7.4% |
| annihilator | C11 | 0.418 [0.389,0.475] | 0.406 [0.398,0.427] | −2.9% | 7.0% / 2.7% |
| annihilator | native | 0.409 [0.382,0.480] | 0.417 [0.389,0.440] | +1.9% | 9.3% / 4.6% |
| annihilator | native-opt | 0.373 [0.325,0.435] | 0.375 [0.365,0.406] | +0.4% | 10.5% / 4.2% |
| tuple | C11 | 0.388 [0.365,0.435] | 0.376 [0.358,0.399] | −3.2% | 6.0% / 4.3% |
| tuple | native | 0.430 [0.408,0.453] | 0.421 [0.379,0.455] | −2.1% | 3.7% / 6.5% |
| tuple | native-opt | 0.425 [0.392,0.447] | 0.428 [0.408,0.468] | +0.6% | 5.0% / 5.3% |

### One-million-iteration diagnostics: both orders

Milliseconds, median [p05,p95]. “First” means baseline→current;
“reverse” means current→baseline. Raw sample populations are not pooled.

| Fixture | Backend | Order | Baseline | Current | Delta | CV baseline/current |
|---|---|---|---:|---:|---:|---:|
| lazy | C11 | first | 7.164 [7.101,7.267] | 6.842 [6.727,6.943] | −4.5% | 0.8% / 1.3% |
| lazy | C11 | reverse | 7.162 [7.092,7.283] | 7.377 [7.004,8.479] | +3.0% | 0.9% / 6.9% |
| lazy | native | first | 12.058 [11.880,12.409] | 12.446 [12.248,12.727] | +3.2% | 1.9% / 1.3% |
| lazy | native | reverse | 11.909 [11.821,12.128] | 12.324 [12.215,12.840] | +3.5% | 0.9% / 1.8% |
| lazy | native-opt | first | 10.917 [10.874,10.985] | 10.726 [10.663,10.866] | −1.7% | 0.3% / 0.7% |
| lazy | native-opt | reverse | 10.967 [10.815,11.087] | 10.691 [10.632,10.920] | −2.5% | 0.9% / 0.9% |
| identities | C11 | first | 6.509 [6.288,14.626] | 3.863 [3.824,3.969] | −40.7% | 71.1% / 1.4% |
| identities | C11 | reverse | 6.332 [6.236,6.445] | 3.854 [3.715,4.013] | −39.1% | 1.3% / 2.8% |
| identities | native | first | 10.691 [10.499,11.107] | 7.259 [7.065,7.421] | −32.1% | 2.0% / 1.7% |
| identities | native | reverse | 10.736 [10.399,12.520] | 7.476 [7.070,15.969] | −30.4% | 10.4% / 42.7% |
| identities | native-opt | first | 10.147 [9.919,10.457] | 6.943 [6.872,7.161] | −31.6% | 1.7% / 1.5% |
| identities | native-opt | reverse | 10.070 [9.946,10.212] | 7.020 [6.843,7.133] | −30.3% | 0.9% / 1.4% |

Interpretation:

* **Identities:** C11 and optimized native have separated intervals in both
  orders. Unoptimized-native medians improve similarly, but the reverse run has
  a large current outlier and overlapping intervals. Report that uncertainty.
  This is a deliberately rewrite-heavy diagnostic, not an application speedup.
* **Lazy native without optimization:** approximately 3% slower in both orders;
  reverse-order intervals separate. This is actionable regression evidence,
  consistent with additional Select slots/blocks, not proof of a single cause.
* **Lazy C11:** the delta changes sign; no robust improvement claim.
  Lazy optimized native is modestly faster by median in both orders but reverse
  intervals overlap; further isolated measurement is needed before claiming a
  reliable small gain.

### Compilation cost, kept separate from runtime

Total compile time includes source emission, native assembly generation where
applicable, and C compilation/assembly/linking; excludes common runtime setup.
First-cycle native emission intermittently took approximately **207–217 ms**
instead of **2–5 ms**, using the identical bridge binary. It affected both
compiler snapshots and several fixture/backend pairs. Its cause is unresolved;
the apparent 71–88% total compile reductions are **not processor speedups**.
All those samples remain in the reports and full tables.

Reverse-order recurrence/branches controls eliminated that delay and measured
approximately 1.7–3.6% higher compile medians for the current combined build.
The targeted lazy optimizer cost persists: MIR-lowering stage medians around
34→53 ms and total compile 52.631→71.981 ms. Current builds enable optional
Chisel/Nerotypical support, unlike baseline; this is combined-build evidence.

Milliseconds, median [p05,p95]; constants from first cycle, other rows from
reverse-order confirmation:

| Fixture | Backend | Baseline | Current | Delta |
|---|---|---:|---:|---:|
| recurrence | C11 | 36.001 [35.403,36.074] | 36.626 [36.277,37.088] | +1.7% |
| recurrence | native | 28.090 [28.064,28.256] | 28.601 [28.484,28.760] | +1.8% |
| recurrence | native-opt | 31.145 [30.246,31.634] | 31.728 [31.328,32.503] | +1.9% |
| branches | C11 | 36.990 [36.655,37.289] | 37.758 [37.533,38.022] | +2.1% |
| branches | native | 28.351 [28.262,29.405] | 29.372 [29.147,29.586] | +3.6% |
| branches | native-opt | 33.510 [33.384,40.297] | 34.360 [33.251,35.099] | +2.5% |
| constants | C11 | 72.443 [71.918,72.893] | 79.613 [79.336,80.367] | +9.9% |
| constants | native | 76.437 [75.539,76.773] | 84.214 [82.964,84.481] | +10.2% |
| constants | native-opt | 142.070 [141.789,142.691] | 150.764 [150.505,150.892] | +6.1% |
| lazy | C11 | 41.268 [41.040,41.510] | 45.694 [43.088,47.639] | +10.7% |
| lazy | native | 31.997 [31.592,32.145] | 35.502 [34.597,36.366] | +11.0% |
| lazy | native-opt | 52.631 [52.009,53.560] | 71.981 [71.699,73.601] | +36.8% |
| identities | C11 | 39.751 [39.137,40.586] | 39.658 [39.406,40.449] | −0.2% |
| identities | native | 30.098 [29.957,30.388] | 30.996 [30.881,31.235] | +3.0% |
| identities | native-opt | 41.286 [41.138,41.741] | 43.043 [41.681,56.750] | +4.3% |

### Rule activity and recommendations

`--dump-processor` was collected **after** the timed cycles for every fixture.
Per-rule counts with source locations are in `stable2/*.processor.json`;
`processor-counts.json` collects them. Representative counts:

| Fixture | Structural / typed-before / typed-after checks | Desugar / canonical rewrites |
|---|---:|---:|
| recurrence | 45 / 55 / 55 | 0 / 0 |
| branches | 59 / 73 / 73 | 0 / 0 |
| gcd | 86 / 107 / 115 | 2 / 0 |
| calls | 80 / 99 / 99 | 0 / 0 |
| constants | 1171 / 1446 / 1441 | 0 / 1 |
| annihilator | 91 / 115 / 115 | 0 / 0 |
| tuple | 68 / 84 / 84 | 0 / 0 |
| trap | 29 / 35 / 35 | 0 / 0 |
| processor-lazy | 122 / 153 / 177 | 5 / 1 |
| processor-identities | 94 / 118 / 100 | 2 / 5 |

Identities fires each checked neutral identity once plus double negation;
lazy fires two `lazy_and`, two `lazy_or`, one integer-not-equal and one
boolean-not-false. MIR counts remain as in the structural table above, including
both unreachable divisions eliminated from optimized lazy code.

1. Prioritize the dynamic Select storage/CFG overhead: it adds instructions and
   stores, reproduces a native-off runtime regression, and increases optimizer
   compilation cost. Consider direct conditional lowering or verified slot/phi
   promotion. Preserve unknown effects, operand order, checked traps, multi-
   predecessor joins, loops and bounded traversal; do not weaken existing gates.
2. Keep neutral-identity rewrites: this diagnostic shows useful C11/native
   runtime reductions. Retain positive, effect/trap and rejection tests rather
   than generalizing the speed claim to other workloads.
3. Profile extra validation/cloning/pass iterations separately. Zero-rewrite
   controls still perform semantic checks; constants performs over a thousand
   checks per phase. Do not remove required original/candidate verification to
   recover latency.
4. Investigate intermittent Archon emission delays independently before using
   total compilation ratios or amortization estimates to guide policy.

### Artifact index and completion

All files are beneath `/mnt/andromeda/tempdir/opencode/`:

* `on1x-meta-benchmark-stable2-{eight,targeted}-{baseline,current}/report.json`:
  first-cycle raw samples, source/executable hashes, distributions, MIR metrics.
* `on1x-meta-benchmark-stable2-confirm-{targeted,controls}-{baseline,current}/report.json`:
  reverse-order raw samples, including unfavorable/noisy results.
* Each run also has `config.json`, `manifest.json`, `commands.json`, `logs/`,
  generated fixtures, emitted C/MIR/assembly, private tools, executable artifacts,
  `analysis.json`, and `summary.md`.
* `on1x-meta-benchmark-provenance/stable2/{eight,targeted,confirm-targeted,confirm-controls}-comparison.json`:
  four successful strict comparisons.
* `on1x-meta-benchmark-provenance/stable2/tables.md`: **all** runtime and total
  compile distributions for all four pairs, including anomalous first samples.
* `on1x-meta-benchmark-provenance/stable2/manifest.json`: final compiler,
  processor source/rule and installed native-toolchain file hashes.
  `report-hashes.json` identifies all eight completed raw reports.
* `on1x-meta-benchmark-provenance/stable2/*.processes.txt`: contention snapshots;
  `*.start.txt` / `*.end.txt`: each measurement interval.
* `on1x-meta-benchmark-provenance/summarize-stable2.py`: ancillary report-to-table
  extraction only. All benchmark generation, execution and comparison policy
  ran in the real MuLL harness.

Results and actionable recommendations were sent directly to all four existing
workers and the parent after the first cycle and after reverse confirmation.
The quiet reservation is released. No production optimization was changed.
Further cycles are event-driven on a new stable relevant build, not polling.
