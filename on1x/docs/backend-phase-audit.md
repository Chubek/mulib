# Backend phase audit

Starting revision: `2082afa9f85c4030cbf3889dcc5429431291c4c9`.
There were no dirty files at startup. This is a partial audit, not a
construct-coverage or release sign-off.

## Driver boundaries at the starting revision

| Entry point | Path to the product |
|---|---|
| `on1xc --emit-c` (also the default) | `load_unit` → PPX rewrite → `check` → checked HIR → SDK `backends::run_c11` or, with SDK disabled, direct `emit_c`. Both call the same checked-HIR emitter. This route intentionally does not produce MIR. |
| `on1xc --emit-ir-c`, `--run-ir`, `--emit-ir`, `--ir-cfg` | Same source checks → `mir::lower` in `hir/lower_mir.cpp` → MIR verification → optional MIR passes → corresponding SDK built-in adapter or direct implementation. |
| `on1xc --emit-vm`, `--run-vm` | Same source checks → `hir::lower_vm` (including private, rechecked closure conversion when needed) → MIR verification → optional MIR passes and VM specialization → bytecode encoder; `--run-vm` invokes the VM engine on that image. This route does not select the SDK adapters. |
| `on1xc --sdk-emit mir-archon --sdk-target TRIPLE` | Same source checks → `mir::lower` → MIR verification → optional MIR passes → target-aware `sdk_driver::run_native` → verified-MIR SDK provider. |
| `on1xc --sdk-{emit,analyze} NAME` without a native target | Same source checks → `mir::lower` → MIR verification → optional MIR passes → `sdk_driver::run`. This is a linked/external SDK hook interface, not a selector for a built-in C11 HIR provider. |
| `on1xc ... --ir-input` | `mir::deserialize` parses wire data and calls `mir::verify` before returning. No source loader, HIR passes, or source lowering. Text preserves portable MIR; executable consumers specialize for their product. |
| `on1x-build.py` and `on1x-native-build.py` | The project wrapper invokes `on1xc` in the product's mode; VM additionally checks the saved image with `on1x-vm --verify`. The native wrapper uses `--emit-ir` for source, or `--emit-ir --ir-input` for serialized input, then passes that MIR to the Archon tool. |
| `on1x-archon-mir --ir-input` | Decodes and verifies MIR, specializes portable directives for the exact selected target, then lowers to native assembly. |
| `on1x-archon-mir --timer-source` | Parses its restricted source and calls `check`. `compile_timer_hir` clones and rechecks the HIR, projects scheduler actions in the backend, and lowers only the projected scalar helpers to verified MIR. Scheduler actions still bypass the shared MIR representation. |
| `on1x-vm [--verify] FILE.o1vm` | Starts at the bytecode loader and verifier; no source or MIR-text input. |

Call sites: [`on1x/src/cli.cpp`](../src/cli.cpp),
[`on1x/src/sema.cpp`](../src/sema.cpp) (`check_unit`),
[`on1x/hir/lower_mir.cpp`](../hir/lower_mir.cpp),
[`on1x/src/backend/serialize.cpp`](../src/backend/serialize.cpp),
[`on1x/sdk/src/Driver.cpp`](../sdk/src/Driver.cpp),
[`on1x/tools/on1x-build.py`](../tools/on1x-build.py),
[`on1x/tools/on1x-native-build.py`](../tools/on1x-native-build.py),
[`on1x/backends/mir-archon/Tool.cpp`](../backends/mir-archon/Tool.cpp),
and [`AsyncPlan.cpp`](../backends/mir-archon/AsyncPlan.cpp).

MIR wire versions remain 1 (legacy scalar), 2 (tuples and `f64`), 3 (extended
numeric), and 4 (portable directives). VM images retain magic `O1VM` with wire
revision 1 and accept executable MIR versions 1–3 after specialization. This
audit does not establish compatibility for additional target triples.

## Discovery inventory

The scanner grammar is [On1x.g](../grammar/On1x.g); full productions are in
[`parser.cpp`](../src/parser.cpp). The checker and canonical verification in
[`sema.cpp`](../src/sema.cpp) distinguish accepted programs from syntax that is
merely parsed. The rows below are starting families, not individual passing
cells; the linked inputs are evidence sources, not claims of backend parity.

| Construct family | Source reference |
|---|---|
| Numeric literals, checked arithmetic, bool, short circuit, direct calls | [`mir.on1x`](../examples/mir.on1x), [`unsigned_f32.on1x`](../examples/unsigned_f32.on1x) |
| Statements, ranges, finite and value loops, labels, nonlocal exits | [`test_control.py`](../tests/test_control.py), [`finite_iteration.on1x`](../examples/finite_iteration.on1x), [`value_loops.on1x`](../examples/value_loops.on1x) |
| Tuples and parameter patterns | [`tuples.on1x`](../examples/tuples.on1x), [`parameter_patterns.on1x`](../examples/parameter_patterns.on1x) |
| Nominals, records, arrays, slices, tags, structural match and guards | [`data_model.on1x`](../examples/data_model.on1x), [`structural_match.on1x`](../examples/structural_match.on1x), [`nominal_adts.on1x`](../examples/nominal_adts.on1x) |
| Generics, instantiation, inherent and cliche members | [`polymorphism.on1x`](../examples/polymorphism.on1x), [`core_impl.on1x`](../examples/core_impl.on1x), [`core-cliche.md`](core-cliche.md) |
| Hygienic and structural macros, PPX, staging and erasure | [`test_macros.py`](../tests/test_macros.py), [`structural_macros.on1x`](../examples/structural_macros.on1x), [`ppx.on1x`](../examples/ppx.on1x), [`staging.on1x`](../examples/staging.on1x) |
| First-class functions, captures and escaping closures | [`closures.on1x`](../examples/closures.on1x), [VM source tests](../backends/mir-vm/tests/source.py) |
| Shared/exclusive places, `ref_replace`, owner moves and drop | [`scoped_loans.on1x`](../examples/scoped_loans.on1x), [`test_mir_borrow.py`](../tests/test_mir_borrow.py) |
| Strings, characters, bytes, interpolation, regex | [`literals.on1x`](../examples/literals.on1x), [`test_core_regex.py`](../tests/test_core_regex.py) |
| Defer, traps, typed `Result` propagation | [`defer_errors.on1x`](../examples/defer_errors.on1x), [pervasive types](../stdlib/core/Pervasive.on1x) |
| Modules, imports, interface obligations and prelude | [`import.on1x`](../examples/import.on1x), [`test_interfaces.py`](../tests/test_interfaces.py), [pervasive types](../stdlib/core/Pervasive.on1x) |
| Raw foreign calls, portable recipes, admitted kernel calls | [`external.on1x`](../examples/external.on1x), [`test_stdffi_products.py`](../tests/test_stdffi_products.py), [scanner grammar](../grammar/On1x.g) |
| Channels, select/poll, asynchronous timers, cancellation, external process resources | [`test_concurrency.py`](../tests/test_concurrency.py), [`cooperative_typed_channels.on1x`](../examples/cooperative_typed_channels.on1x), [`cancellation_deadlines.on1x`](../examples/cancellation_deadlines.on1x) |

## Regression commands

The scalar test is `ctest --test-dir BUILD -R '^on1x\.backend_phase_routes$'
--output-on-failure` after building `on1xc` and `on1x-vm`. It checks C execution
for both emitters, reference evaluation, canonical text round-trip, CFG DOT
(parsed with Graphviz when installed), and verified saved/in-process VM
execution. It also compares backend diagnostics with source checking, and
checks decoder and verifier rejection before artifact publication.

Starting behavior: `on1xc --run-ir --emit-vm examples/mir.on1x -o FILE` returned
success and replaced `FILE` with VM bytecode. This was argument-order dependent.
The regression checks that conflicting compiler modes fail in both orders,
including source inspection modes that otherwise could override a backend.

The standalone native wrapper also dropped the `--optimize-ir` opt-in after
its compiler subprocess. The portable MIR fixture in
`on1x.backends.mir_archon_native_build` proves that the native wrapper now
passes the opt-in to the Archon specialization boundary: unoptimized assembly
has two checked negations; with the permitted, verifier-checked rewrite it
has one. `--speculate` is the specialization-only switch used when the caller
has already run compiler MIR passes.

## Verified slice

The scalar input is [`examples/mir.on1x`](../examples/mir.on1x), with output
`1, 1, 2, 6, 24, 120, false` (one value per line). The previously unsupported
input is `tuple_loan` in [`test_mir_borrow.py`](../tests/test_mir_borrow.py),
with output `9, 9, 5, 7, false, true, 15`. It exercises a nested exclusive
reborrow and exchange, left-to-right RHS effects, and an early return. The
companion `tuple_loan_trap` preserves output `4, 0` before trapping. The
[`HIR test`](../tests/test_mir_borrow.cpp) checks a single typed root slot and
source locations on the resulting operations. The baseline rejected the
checked tuple loan in MIR; `ddd99e393c63fe3d3c69e574c0688b89db6ce8a2` admits it
without adding an opcode or changing an artifact version.

| Product | Scalar fixture | Tuple-loan fixture |
|---|---|---|
| `c11` | `--emit-c`, compile/link/run: expected output | Same via `backend_phase_routes` (scalar) and `mir_borrow_products` (tuple) |
| `mir-c11` | `--emit-ir-c` from source and saved MIR, compile/link/run | Saved MIR, compile/link/run in `mir_borrow_products` |
| `mir-eval` | `--run-ir` from source and saved MIR: expected output | `--run-ir --ir-input` in `mir_borrow_products` |
| `mir-text` | `--emit-ir` and `--emit-ir --ir-input`: identical bytes | Same canonical round-trip in `mir_borrow_products` |
| `mir-cfg` | `--ir-cfg` from source and saved MIR: DOT with edges, parsed by `dot -Tdot` | `--ir-cfg` from source and `dot -Tdot`: parsed; topology not compared edge by edge |
| `mir-vm` | `--emit-vm`/`--run-vm` from source and saved MIR; `on1x-vm --verify` and standalone execution | Source execution and saved-image execution from ordinary MIR in `mir_borrow_products` |
| `mir-archon` | SDK source assembly and `on1x-build.py --backend mir-archon --optimize-ir`, link/run on AMD64 | Same, plus standalone saved-MIR assembly/link/run in `mir_borrow_products` |

The compiler configurations were GCC/G++ 16.2.1 and CMake 4.4.3, using
LibStone `76d24a7f8b19294bab2b713dc69f799e412b62d2` and a build-host Cephyr
library exporting `cephyr_emit_mir`. Both clean compiler builds used
`-DON1X_USE_FALLBACK_SCANNER=ON -DON1X_BUILD_DYNAMIC_SDK_PLUGINS=OFF
-DON1X_WITH_SYREN=OFF -DON1X_WITH_WETSTONE=OFF -DON1X_WITH_PROJECT_QMR=OFF`.
The SDK-enabled native build used `-DON1X_BUILD_MIR_ARCHON=ON
-DON1X_ARCHON_ENABLE_SDK_ADAPTER=ON
-DON1X_ARCHON_TARGET_TRIPLE=x86_64-unknown-linux-gnu` and an explicit
`-DArchonBackend_DIR:PATH=...`. The package advertised MachineIR and backend
ABI version 1. The separate SDK-disabled native build used Archon with its
SDK adapter disabled and MEMGraph optional rules off.

- SDK off: `ctest --test-dir BUILD -R 'backend_phase_routes|(^on1x\.(mir|vm|ownership_backends))' --output-on-failure`
  passed 21/21 after building the selected test executables.
- SDK off with standalone Archon: `ctest --test-dir BUILD -R 'backend_phase_routes|mir_borrow_(hir|products)|mir_archon_timer' --output-on-failure`
  passed 5/5, including the legacy timer-native route.
- SDK on with Archon: `ctest --test-dir BUILD -R 'backend_phase_routes|mir_borrow_(hir|products)|^on1x\.backends\.|^on1x\.sdk\.native_backend' --output-on-failure`
  passed 19/19, including the native wrapper, scoped dispatch, ABI, limits,
  and differential execution suites.
- `python3 on1x/tools/on1x-build.py --on1xc BUILD/on1xc --backend BACKEND on1x/examples/mir.on1x -o FILE`
  produced matching executables or VM output for `c11`, `mir-c11`, `mir-vm`,
  and `mir-archon`. This tests the one-file wrapper, not every On1xfile mode.
- Both Python fixture suites passed with generated C and the linked C runtime
  built using `--cflag=-fsanitize=undefined --cflag=-fno-sanitize-recover=all`.
  This does not instrument the compiler or native assembly.

## Remaining gaps

This evidence does not establish full language coverage. The common HIR
lowerer still rejects checked parameter loans, managed/unique values and
cleanup, raw foreign calls, and cooperative concurrency. Finite aggregate
and closure conversion is restricted to the VM HIR profile; dynamic
closures, imports, and resumable execution have no cross-backend proof.
The standalone Archon timer action projection remains outside shared MIR.
Additional advertised native targets, the full project/SDK mode matrix,
the requested independent worker reviews, and the release-wide test matrix
have not been verified.
