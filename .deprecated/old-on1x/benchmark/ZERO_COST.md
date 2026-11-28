# Zero-cost gate: scalar identity wrappers vs explicit On1x

**Accepted on all three backends with the default typed processor.** The
wrapped spelling emits the same C11 and native machine code as the explicit
program. The previous compiler failed all three gates. Native wrapped runtime
fell 21.9–25.2% across the two orders; C11 wrapped medians instead rose
4.2–4.7%. Compiler work still costs time even when output is identical.

**Question.** Does spelling the same scalar computation with these wrappers add
any generated code? Equal checksums or indistinguishable timings are not
sufficient. Compare two programs under the **same** compiler and backend.
Then compare the old and new compilers without changing either program.

## What is paired

`zero-explicit` computes `r = (i + seed) % 7`, calls `accept(r)` (`r < 3`),
assigns `accept(r) == (i % 2 == 0)` to a mutable `ok`, then branches on `ok`.
One arm adds `r`; the other subtracts 1. `zero-wrapped` uses the same functions,
storage, control flow and checksum loop, changing just three expressions:

* `((((i + seed) % 7 + 0) - 0) * 1) / 1`
* `true && (!!((!accept(r) == !(i % 2 == 0)) == true) || false)`
* `(false || !!(ok != false)) && !false`

The last neutral `&& true` is **not syntactically literal**; its right side
must first canonicalize. The `!left == !right` comparison has two dynamic
operands. The programs deliberately both contain the predicate call: the
claim concerns wrapping its result, not removing the cost of arbitrary calls.

The genuine MuLL harness generates both from `lib/Fixtures/Package.mull`.
Every complete 14 iterations visits each residue once on each parity: its two
contributions sum to `r - 1`, so a complete cycle contributes 14. The oracle
adds the remainder from two fixed seven-element tables. It does not execute
the generated predicate or repeat the On1x loop. With `n=1000000, seed=1729`
the exact gate is stdout `999998\n`, empty stderr, exit 0, no signal or
timeout, for *every* execution including warmups. The tail matters: reversing
the decision gives `999999`; dropping the predicate gives `1000000`; dropping
the parity test gives `-142857`. SHA-256 of the sources:

```
9e76b0cb9e5d15c7e2dfea3258d5a55fc228969bee302dae8ef187b7614ceb3e  zero-explicit.on1x
6334f3edbbdff1e24bbd4310cba12e46e3567f8753de68ff9525462e32ed22cf  zero-wrapped.on1x
```

## Acceptance and reproduction

1. Build the normal [MuLL harness](README.md#build-and-test) and run its
   CTests. Use an x86-64 Linux host, real scalar Archon bridge, GCC, GNU
   `objcopy`, `sed`, `sha256sum`, and `jq`. Run from the repository root.
2. Generate and run the two opt-in cases in a *single* cross-backend report.
   A fast gate independent of the timing study:

   ```sh
   "$HARNESS" run --config on1x/benchmark/config/smoke.json --compiler "$ON1XC" \
     --archon "$ARCHON" --cc /usr/bin/cc --cflags '["-O2"]' \
     --iterations 1000000 --seed 1729 --cases zero-explicit,zero-wrapped \
     --output "$FRESH_RUN"
   "$HARNESS" zero-cost "$FRESH_RUN"
   ```

3. `zero-cost` returns 0 **only** on a successful report with one valid,
   measured row per side per backend and these simultaneous equalities:

   | Backend | Input to downstream compiler | Linked image |
   |---|---|---|
   | Direct HIR → C11 | entire emitted C byte-identical | normalized ELF byte-identical |
   | MIR → AMD64, no `--optimize-ir` | location-normalized MIR **and** entire emitted assembly byte-identical | normalized ELF byte-identical |
   | MIR → AMD64, `--optimize-ir` | same comparisons for separately optimized MIR/assembly | normalized ELF byte-identical |

   Hashes use SHA-256. The gate rechecks the actual generated source against
   the MuLL generator and source/executable hashes against the report; it
   executes each pinned binary again against the independent oracle. For these
   scalar fixtures the only MIR normalization is
   `(at "path" byte-offset line column)` → `(at)`; SSA IDs, function names,
   block edges/arguments, instruction/effect order, and constants are intact.
   Each normalized ELF is made with
   `objcopy --strip-all --remove-section=.note.gnu.build-id`: this removes
   differing static `STT_FILE` source names and linker build IDs without
   rewriting code, dynamic bindings, or runtime data. Raw executables and
   their hashes are preserved too. `zero-cost/comparison.json` in the report
   directory retains all pair hashes; no instruction counts or timing
   thresholds substitute for equality. A tool failure or missing row rejects.
4. To reproduce the complete historical-red / candidate-green measurement:

   ```sh
   bash on1x/benchmark/zero-cost.sh \
     "$HARNESS" "$BASELINE_ON1XC" "$CANDIDATE_ON1XC" "$ARCHON" "$FRESH_DIR"
   ```

   Four serial reports: baseline then candidate, followed by candidate then
   baseline with fixture order reversed too. The prior `processor-lazy` and
   `processor-identities` diagnostics run alongside the new pair because five
   of their six executables changed in the structural smoke. Each report uses
   3 runtime warmups + 15 samples and 1 compile warmup + 5 samples. Runtime
   includes process launch/reaping; compile includes emission, assembly/C
   compilation and link, excluding setup of the shared runtime/ABI. No CPU
   pinning, clock control or cache flushing. Strict MuLL `compare` checks
   cross-compiler comparability for each order; each run separately applies
   the within-compiler paired identity gate. The script requires an actual
   three-backend historical identity rejection and candidate acceptance.
   The acceptance criterion is code identity, **not** zero compiler time.
   Both the historical eight-case default and two earlier opt-in
   `processor-*` diagnostics keep their original generators.
   Regenerating both populations and comparing with the preceding round
   gives identical complete manifests (including expected outputs) and
   identical sources. Manifest SHA-256: default eight (10,000 iterations)
   `cfb8310ea93b65c9a64cdc470627c9542b855322de7db3d166234968479ef12c`;
   previous two (1,000,000 iterations)
   `ec4e6e35b5bee12078c85b6acd3f330e96783b1dd6b6cd237f8d14f6ca99623f`.

   Inputs used on this host (`p=/mnt/nvme/on1x-zero-benchmark`,
   `b=/mnt/andromeda/tempdir/opencode`):

   ```sh
   bash on1x/benchmark/zero-cost.sh \
     "$p/harness/on1x-benchmark" "$b/on1x-opt2-build/on1xc" \
     "$p/final-tool/on1xc" "$b/on1x-opt2-provenance/baseline/on1x-archon-mir" \
     "$p/quiet"      # choose a fresh destination to repeat
   ```

## Baseline: code differed before the change

The round began at MuLIB `866b57a9e581a622e8a2bceb3cf9bdecb9de09a0`,
LibStone `5d733d69f19fac1d8a05294fe54e4a4ec863fc4c`. The baseline compiler
is the **candidate from [round 2](OPTIMIZATION_ROUND2.md)**, SHA-256
`beb339dbf2417c55239789eba68dfbc88a61ce3971672c07451a6d71ec4aba3e`.
Artifacts here are beneath `/mnt/nvme/on1x-zero-benchmark/`.
The reproduced harness binary is
`12e6a460cd4f2708bffa19c6f7b2cdf88cd0c88b6d50b4417be908b7a54aab9d`;
the shared native bridge is
`d2b97b2403663996686eef8d83d278e9fe8e5bffa1e5112717eb72343bc02673`.
Linux 6.18.52-1-lts, Intel i7-11700K, GCC 16.2.1, GNU binutils 2.47.
Native triple `x86_64-unknown-linux-gnu`; C target `x86_64-pc-linux-gnu`.
GCC `-std=c11 -O2 -no-pie`, link `-lm`; shared runtime object SHA-256
`2b7b694a9190bc4050345a6282462cf13273217c4806a52ddc4eed8653f3dc0d`;
native scalar ABI `97e45bc679d8383001a28638c441a6cbe186b6c3433eb4e2930db8b89ea2f6ff`.
`baseline-red/` is an exploratory simpler pair without dynamic negated
equality or the derived right literal. Its C11 machine code already matched;
native code did not. It is not a measurement of the final fixture. The
stronger pair above was fixed and run against the baseline in
`baseline-final-red/` **before testing a candidate**.

All six final baseline oracle gates passed, but all three identity gates
failed. Whole-program counts for explicit vs wrapped:

| Baseline | MIR instructions | MIR blocks | Emitted AMD64 instructions |
|---|---:|---:|---:|
| Native-off | 39 / 43 | 9 / 15 | 197 / 271 |
| Native-opt | 37 / 40 | 9 / 15 | 187 / 230 |

The optimized wrapped program still has 2 `not` and 7 `load` vs 0 and 6.
The baseline wrapped processor applied generic `lazy_and` ×2 and `lazy_or`
×2; neither generic rule fires on the explicit control. C11's `kernel`
machine code differs in branch layout, `sete` vs `setne`, and arithmetic
arm placement even with the same kernel instruction count. Equal executable
file sizes likewise concealed the native cost. Native instruction counts are
static instructions (`awk '/^\t[a-z]/{n++} END{print n}' *.s` separately
per file), not dynamically executed instructions or cycles.

## Structural smoke before quiet measurement

Private intermediate compiler `18bb926593a405eea087d1aa2a45f7192839957c45321771bed4a923c4163304`,
processor `4049525ac80f15e1e4a4831e0c2027ba748a42f5ba9f898013f8a77834287c60`:
all six oracle gates, all three code-identity gates. `intermediate/pair/zero-cost/`
also retains both verified processor rule reports. Each of the four neutral
lazy rules and `boolean_equal_negated_operands` fired **once** on the wrapped
source (no generic lazy-region expansion). Other wrapped hits include
`boolean_not_equal` ×1, `boolean_double_not` ×3, `boolean_not_false` ×1,
all four applied i64 identities ×1 and the two literal equality rules ×1.
The explicit source needed zero rewrites. The baseline C11 kernel had 61
instructions on each side, but different instructions; the candidate has
identical code, not just identical counts.

Normalized linked ELF SHA-256. The baseline explicit control **also** has
the respective matching candidate hash for every backend:

| Backend | Baseline wrapped | Candidate explicit **and** wrapped |
|---|---|---|
| C11 | `54e6eb5bb9ff321454222822bab8e8d2d6ddd5beab571803713787255964fe3e` | `3abd9731507addb68bb80de705f2d5de2f22c64d143e38a5e8b0514b59abacc2` |
| Native-off | `e4351396135bc6787703557c25c793446f8b3244acb23933e74f9500b4905c3b` | `30e3d2d4b1a7e2b27a44f28f3bc48a3f4e87679bb486993295f097c081ff66c2` |
| Native-opt | `0b55b929bd53e112bea667a49efd69e117c13a4c324a383e61dd6e11eb825c2f` | `53bd49fa7fc205ae192a850c95f2b4e3c328354241dcffd1a5caf234570a2a46` |

Candidate C11 is 3,759 bytes both ways. Native-off/opt respectively are
39/37 MIR instructions, 9/9 blocks and 197/187 static assembly instructions
for **either** spelling. Both native pairs also have identical *raw* linked
executables; direct C11 has differing static filename/build-ID metadata.

Also compared `b && !false`, `b && (true != false)`, `b && (true || true)`
and `b && !(false && false)` vs `b`, and `!a != b` / `a != !b` vs `a == b`.
All six matched byte-for-byte in direct C11 and AMD64 assembly off/on (18
structural comparisons, `composition-probe/` sources, `intermediate/` outputs).
`x + -0` vs `x` matched in direct and MIR-emitted C11 as well.
These small probes were not timed.

**Profile boundary:** compiling each frozen source with `--profile debug` and
`--profile release` and executing direct C11 + native assembly passed 8/8
checksum gates (`intermediate/profiles/`). Release emitted the exact default
C11 and exact optimized assembly (187 instructions for each source). Debug
intentionally retains wrappers: 3,759/5,188 emitted C bytes and 197/369
static assembly instructions. “Native-off” in this report means **MIR passes**
off, not “disable the typed processor”; it is not the debug profile.

The intermediate historical-eight rerun passes 24/24 oracle gates; all 24
raw executables equal the previous round's candidate. Both previous
diagnostics pass all six oracles but only `processor-identities` direct C11
has the same raw executable. Lazy MIR blocks decrease 18/16→17/15
(off/optimized); identities 8/8→7/7, with unchanged MIR instruction counts.
See `intermediate/{eight,old-pair}/report.json` and `old-executables.txt`.

## Final paired measurement

Final compiler SHA-256 `7187a0edadf292bbe3412d16f1c811d6abe48e984ab849eea2dce860671c1bba`,
same processor as the structural smoke. All **48/48 rows** across four reports
passed: 720 measured runtimes, 240 compilation samples, warmups excluded.
Both strict cross-compiler comparisons were compatible. In each order the
baseline rejected 3/3 code pairs and the candidate accepted 3/3. Both
orders reproduced the hashes above; `first.green.json` and `reverse.green.json`
are identical (`9d4285e290ae327087d36fdf25ce854fe6a0c5dae3352b0f2da9f2a19c09703c`).
Final window 2026-11-21 07:54:42–07:55:12 UTC after test/build handoff;
normal host services remained active.

**Whole-process medians, milliseconds.** E/W = explicit / wrapped on the
same backend. “First” ran E before W and the baseline compiler first;
“reverse” ran W before E and the candidate compiler first. No pooling.

| Scope | Backend | First baseline E/W | First candidate E/W | Reverse baseline E/W | Reverse candidate E/W |
|---|---|---:|---:|---:|---:|
| Runtime | C11 | 5.547 / 5.408 | 5.716 / 5.665 | 5.605 / 5.408 | 5.605 / 5.637 |
| Runtime | Native-off | 8.703 / 11.640 | 8.769 / 9.012 | 8.632 / 11.584 | 8.711 / 8.666 |
| Runtime | Native-opt | 8.304 / 10.727 | 8.410 / 8.373 | 8.424 / 11.076 | 8.415 / 8.407 |
| Compile | C11 | 41.683 / 43.629 | 42.000 / 45.831 | 42.362 / 43.045 | 41.462 / 42.290 |
| Compile | Native-off | 30.480 / 33.025 | 30.314 / 31.727 | 30.821 / 32.201 | 30.089 / 30.970 |
| Compile | Native-opt | 42.052 / 47.927 | 42.119 / 43.340 | 42.262 / 47.818 | 42.025 / 43.139 |

* **Native:** compared with the old wrapped code, new wrapped runtime fell
  22.6/25.2% (off, first/reverse) and 21.9/24.1% (opt); optimized total
  compile time fell 9.6/9.8%. On the same candidate, emitting wrapped vs
  explicit still costs about 1.1–1.2 ms extra at the optimized compile medians.
* **Noise control:** the two candidate native-off ELFs are byte-identical,
  yet the first wrapped run was 2.8% slower than explicit (runtime p05/p95
  8.742–15.141 ms, CV 22.1%; compile p05/p95 30.872–51.427 ms, CV 29.9%).
  In reverse it was 0.5% faster (runtime p05/p95 8.555–8.854 ms). Baseline
  wrapped native-opt reverse also had a 21.849 ms runtime p95 against an
  11.076 ms median. Samples are retained; no speedup threshold is used.
* **C11:** the old wrapped branch layout measured faster than the new one
  at both medians. Old/new p05/p95 overlap: 5.288–5.680 / 5.534–5.880 ms
  first; 5.261–5.523 / 5.513–5.901 ms reverse. This is a possible performance
  tradeoff, not proof that the change caused a regression. The narrower
  identity claim is exact: the new wrapped and explicit images match.

The two previous diagnostics also used one million iterations. Below,
`B→C` is **compiler baseline → candidate**, not explicit vs wrapped. Native
runtime intervals separated in both orders for both diagnostics; C11 runtime
intervals overlapped. These are descriptive intervals, not significance tests.

| Older fixture | Backend | Runtime first B→C | Runtime reverse B→C | Compile first B→C | Compile reverse B→C |
|---|---|---:|---:|---:|---:|
| lazy | C11 | 6.771→6.855 | 6.736→6.839 | 42.725→43.120 | 42.851→42.784 |
| lazy | Native-off | 11.339→10.804 | 11.275→10.747 | 32.708→32.380 | 32.299→31.927 |
| lazy | Native-opt | 10.155→9.712 | 10.053→9.674 | 52.194→51.287 | 52.208→51.086 |
| identities | C11 | 3.835→3.847 | 3.800→3.834 | 40.204→39.613 | 39.851→39.651 |
| identities | Native-off | 6.812→6.132 | 6.780→6.166 | 29.935→29.743 | 29.494→29.289 |
| identities | Native-opt | 6.340→5.872 | 6.271→5.881 | 38.412→37.434 | 38.454→37.401 |

[Portable evidence](results/zero-cost-linux-amd64-2026-11-21.json) (SHA-256
`7154174bfb98148c7249b6af97f1221f07ccf94a95f306ae66a8d0b5c918aa97`)
contains every measured runtime/compile/per-stage duration, MIR counts, and
all code-pair hashes. Full MuLL reports, raw command journals, processor hit
reports, exact compiler/bridge copies, generated IR/C/assembly and executables
are in `quiet/{first,reverse}-{baseline,candidate}/`. `quiet/*.cross.json`
hold strict comparisons; `*.red.json` / `*.green.json` hold the pair gates.
The rebuilt benchmark harness passed 9/9 CTests. The compiler regression
handoff recorded 211 passes, three failures and one skip in the sole full 215
run; both SDK failures passed after repair in an isolated 2/2 rerun, leaving
one previously known addon failure and the cgroup skip in the tested union.

## Beyond the pair

The broader [round-2 report](OPTIMIZATION_ROUND2.md) is a **different
population**. At 10,000 iterations its native `constants` case went from
579 to 118 MIR instructions and 104 to 71 blocks with optimization, but
compilation took 82.0 vs 148.5 ms, for 2.078 vs 0.819 ms runtime (estimated
53 runs to break even). This is a large residual compile/run tradeoff.
`calls` (27→27 instructions), `tuple` (26→26) and `branches` (26→26) show
no MIR instruction reduction; the short tuple runtime distributions overlap.
In that run's linked `-O2` direct-C11 binaries, `objdump -d` still shows
three static calls to `on1x_env_release` inside generated functions/adapters
in `calls`, and one each in `tuple`, `gcd` and `constants`. The native-opt
assembly files contain 75, 108, 78 and 58 `(%rsp)`-referencing instruction
lines respectively for `calls`, `tuple`, `branches`, and `recurrence`.
These are concrete costs to investigate, not measured opportunities to
remove every call or stack access.
These controls have different semantics, ABI and startup costs; they are not
paired proofs about recursive calls or tuple abstraction. The ~210 ms
intermittent bridge delay documented in round 2 affected some *unchanged*
artifacts. It cannot be assigned to an On1x source wrapper.

Checked arithmetic calls, native stack traffic, loop storage, direct C11
adapter/environment release, runtime/link overhead and startup can remain
in **both** sides. For example, the frozen C11 loop still calls
`on1x_env_release(NULL)` once per iteration on this GCC build (`xor %edi,%edi`
before `call <on1x_env_release>`); the explicit optimized native assembly
contains 106 static `(%rsp)`-referencing instructions across its functions.
This gate tests no general inlining, heap allocations,
closures, non-scalar data, async, foreign calls, other architectures, or
whole-language zero-abstraction theorem.
