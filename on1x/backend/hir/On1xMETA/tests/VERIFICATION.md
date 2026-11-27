# Verification record — 2026-11-21

Working tree based on MuLIB `94b27511328c61a219c90cf531ca68e952bc2a85`,
LibStone `5d733d69f19fac1d8a05294fe54e4a4ec863fc4c`. Linux x86-64,
kernel `6.18.52-1-lts`; GCC `16.2.1`, Clang `22.1.8`.

Build: `/mnt/andromeda/tempdir/opencode/on1x-meta-build`, Debug, generated
scanner, real installed AMD64 SysV Archon backend and native SDK adapter,
Chisel and Nerotypical enabled. The parent session owns configuration/build
commands recorded in the main verification document.

## Final evidence

* All **9 processor CTests pass**: generator, snapshots, include precedence,
  add/subtract/forbid authored-table source tests, engine, semantic validator,
  and source integration.
* Source integration executes **129 profiles**: 18 scalar cases each through
  seven paths (HIR-C11, MIR-C11, optimized MIR-C11, evaluator with/without
  optimization, real native with/without optimization), plus three C11-only
  hygiene/closure-cleanup/unique-owner cases. **46 negative checks** pass.
* The same 129 executions and 46 rejections pass with Clang ASan/UBSan on
  generated C11, runtime and allocation fixtures. This run preceded the final
  diagnostic-text-only compatibility fix. The compiler, prebuilt native ABI
  archive and native assembly are not instrumented by that command.
* The generator suite rejects **47** malformed/unsafe/resource/collision inputs,
  preserves existing outputs, regenerates the real LLSynth parser twice,
  compiles its tables, and checks input relocation/order and manifest hashes.
  Standalone GCC and Clang table-compilation runs pass.
* Full CTest ran **211 tests in 168.78 seconds**: 207 passed, one skipped,
  three failed. Two failures were the same newly introduced empty-match
  diagnostic wording regression. After restoring the established diagnostic
  wording in the authored semantic rule, the affected selection passed
  **11/11 in 12.62 seconds**. The resulting tested matrix is **209 passed,
  one skipped, one known baseline failure**; this is not a claim of a second
  complete 211-test run.
* The remaining failure is `on1x.addon_chatgpt`: its repository smoke test
  rejects existing MANIFEST fields with `Noncanonical kernel fields`.
  The same failure was reproduced against the unchanged prior
  `on1x-archon-build`; 27 other Python methods pass. The intentional skip is
  `on1x.container_pod_cgroup`.

The full run includes canonical/ownership/control checks, macros and staging,
numeric and concurrency source tests, generated/fallback scanner parity,
independent type/memory checkers, installed/relocated SDK consumers, package
installation, native differential execution and native resource limits.
It does not establish a separate complete fallback-compiler configuration or
native execution beyond the admitted AMD64 MIR subset.

## Commands and retained artifacts

```sh
ctest --test-dir /mnt/andromeda/tempdir/opencode/on1x-meta-build \
  --output-on-failure --parallel 4 \
  --output-log /mnt/andromeda/tempdir/opencode/on1x-meta-full-ctest.log

python3 on1x/tests/test_processor.py \
  /mnt/andromeda/tempdir/opencode/on1x-meta-build/on1xc \
  /mnt/nvme/mulib/on1x \
  /mnt/andromeda/tempdir/opencode/worker4-meta-sanitized clang \
  --cflag=-fsanitize=address,undefined --cflag=-fno-omit-frame-pointer

ctest --test-dir /mnt/andromeda/tempdir/opencode/on1x-meta-build \
  --output-on-failure --parallel 4 \
  -R 'on1x\.processor|^on1x\.(patterns|enum_connective)$' \
  --output-log /mnt/andromeda/tempdir/opencode/on1x-meta-final-focused-ctest.log

ctest --test-dir /mnt/andromeda/tempdir/opencode/on1x-archon-build \
  --output-on-failure -R '^on1x.addon_chatgpt$'
```

The integration output directory contains `processor-results.json`,
`processor-report.json`, and the generated C, MIR, assembly and executables.
The final report has actual semantic check counts **55 / 71 / 65**
(structural / typed-before / typed-after), **2 desugaring rewrites** and
**3 canonicalization rewrites**. Tests verify each activated rule's authored
file/line/column and compare program/parser/source hashes to the build manifest.

Final identities:

```text
on1xc:
06ec2d05af95a38c25b1aea5226f54dc7d140ae80265e24528e3bde64e335b86
program:
0af30583209e3a231fa2ed2772646fe70de6479a03b02b420f25dfd2e8cb449b
parser:
9e3877b37bf83675fb51e33d1d02105e84ee58d4054f527317a61fdd1d81d358
```

## Regressions exposed during integration

1. A stale generated header could shadow the current semantic table and leave
   all semantic rule counts at zero. Include precedence and dedicated shadow
   tests now cover this; source report tests require actual semantic hits.
2. Source logical expressions now use canonical decision regions. Two old MIR
   tests assumed a particular source-lowered phi layout. Their exact verifier
   and uniform-constant-phi obligations now use independently authored MIR;
   source logical execution assertions remain.
3. Region slots initially prevented existing dead-division elimination.
   LocalSlots v3 restores the existing optimization assertions, with
   predecessor/effect/join/loop tests and focused sanitizer evidence maintained
   by the engine worker. No no-division assertion was removed.
4. Early semantic checking changed empty-match diagnostic wording. The
   authored rule now preserves the established diagnostic substring; existing
   pattern and enum-connective tests pass without relaxing their assertions.

After the final focused gate, compiler/test workloads stopped for the
benchmark worker's quiet measurement window. Timing conclusions are recorded
separately in [`on1x/benchmark/PROCESSOR_FEEDBACK.md`](../../../benchmark/PROCESSOR_FEEDBACK.md).
