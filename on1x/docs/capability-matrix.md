# On1x capability matrix

Cancellation/deadline continuation at `1f027c48bc6363dce5752c00057830777325ff47`,
configured read-only LibStone `55755485acaded7c4ed804c0946bc8c7363f6763`:

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Monotonic poll deadline | `src/{parser,infer,sema,concurrency,codegen,ppx_ast}.cpp` | ABI v3 private timer-channel offer using existing atomic MuLIB select, CLOCK_MONOTONIC supervision; no kernel timeout change | One `after` per poll, timer-only choices, i64 milliseconds or ms/s units, optional default; located invalid-duration faults | `test_cancellation_deadlines.py`, `test_concurrency.cpp`, clock-jump/OOM fixtures, compiled example, scanner parity; exact evidence in `../VERIFICATION.md` | EXECUTABLE | No taskgroup `within`, typed timeout/cancel join outcomes, effectful MIR or other-target validation |
| Running/blocked fiber cancellation | `src/{concurrency,codegen}.cpp`, `runtime/on1x_concurrency.*` | ABI v3 private cancel offer, explicit cleanup returns, shielded defers, descendant cancel/join, existing process-group cancellation/reaping | Consuming affine `cancel` before dispatch, while running or blocked; same-parent checks, no forced native preemption | `test_cancellation_deadlines.py`, `test_concurrency.cpp`; loop/call/defer/RC/nested/byte waits, observer reuse, OOM, strict C11 and ASan/UBSan | EXECUTABLE | General effects/ownership MIR, native workers, independent SDK models and general async suspension |
| Stackless async/await | Existing scalar MIR and CSP HIR | No admitted future frame/runtime ABI | Not admitted | Unsupported-profile coverage only | ABSENT | Typed verified suspension/frame lowering; not stackful launch sugar |

Linux 6.18.52 x86_64/glibc 2.44, GCC 16.2.1 and Clang 22.1.8.
Full fallback/GCC: **52 passed, 1 failed, 1 skipped**; full generated/GCC:
**53 passed, 1 failed, 1 skipped**. The failure is the unchanged
`tools/embed_units.py` fixture fingerprint; Category skips with native PPX off.
Focused ASan/UBSan: **4/4**; strict Release: **2/2**; scanner parity: **302**.
See [the profile](cancellation-deadlines.md). This request is only partially
implemented: stackless async/await and taskgroup-wide deadlines remain absent.

G4 stream-poll continuation (baseline `d4cc478df76bfe9b5b8e6485f42d56fde81e8a68`):
atomic `poll` now accepts captured process byte endpoints, including mixed i64/u8
offers. Configured LibStone remains `55755485acaded7c4ed804c0946bc8c7363f6763`.
The example previously failed with `expected sender<i64>, got sender<u8>`.
This task does not implement deadlines, cancellation unwinding or the full syntax
specification. See [the exact profile](stream-poll.md) and `../VERIFICATION.md`
for Linux x86_64/GCC/Clang commands and test results.
Final full suites: fallback/GCC **51 passed, 1 failed, 1 skipped**;
generated/GCC **52 passed, 1 failed, 1 skipped**. The unchanged failure is the
stale `tools/embed_units.py` fingerprint; Category skips in native-PPX-off
profiles. Clang ASan/UBSan focused **3/3**, strict GCC Release focused **2/2**,
scanner parity **301 cases**, native identity PPX and standalone strict Clang
C11 examples execute. These results do not complete G4 or the syntax specification.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Mixed i64/process-byte atomic poll | `src/{infer,sema,concurrency,codegen}.cpp` | Existing MuLIB atomic selection and private ABI v2; exact uint8_t staging in `runtime/on1x_concurrency.c`, explicit widening/narrowing | Existing source `poll`, exact i64/u8 payloads/binders; 1..16 named offers with mandatory close arms and optional default | `test_stream_poll.py`, `test_concurrency.cpp`, `fixtures/stream_poll_boundary.c`, compiled `examples/cooperative_stream_poll.on1x`; seeded full-byte-domain oracle, mixed offers, 0/1/7 capacity, losing offers, EOF/default/cancel, wire/PPX, C/C++ TU callers, malformed HIR, runtime range/direction/OOM, strict C11 and ASan/UBSan | EXECUTABLE | No general byte-channel constructor, deadlines, running cancellation, general ownership/effect MIR, other targets or full-spec conformance |

Earlier G4/v3 evidence (before the stream-poll continuation above): compiler-parsed backtick command literals and captured
process byte streams now execute as affine `sender<u8>`/`receiver<u8>`
capabilities. They extend the existing bounded direct-exec external fibers and
consuming process cancellation, plus internal cooperative fibers,
unidirectional i64 channels and bounded atomic `poll`, using canonical
source/HIR/C11 and MuLIB kernels. Re-inspected HEAD
`8996396506901f792c0b697ebbf8d6a2a233f0f6`, configured
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763` (read-only). Linux
6.18.52 x86_64/glibc 2.44, GCC 16.2.1, Clang 22.1.8. Final fallback/GCC
suite: **50 passed, 1 failed, 1 skipped**; generated/GCC suite:
**51 passed, 1 failed, 1 skipped**;
fallback/Clang ASan+UBSan focused: **2/2**; scanner parity: **300** exact
cases. The sole full-suite failure is the pre-existing stale
`tools/embed_units.py` fixture fingerprint. Exact commands and unsupported
profiles are recorded in `../VERIFICATION.md`.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Internal cooperative fibers / task scopes | `src/{parser,infer,sema,concurrency,codegen}.cpp`, `include/on1x/concurrency.hpp` | `runtime/on1x_concurrency.[ch]`, existing MuLIB N:1 fiber scheduler, compiler-generated C11; one native thread | `taskgroup`, eager `launch { ... }`, affine `ifiber<i64>`, consuming `join` or immediate same-block pre-dispatch `cancel`, immutable i64/scoped endpoint captures | `test_concurrency.{cpp,py}`, compiled `examples/cooperative_channels.on1x`, repeated ready cancellation, negative ownership/HIR/wire, strict GCC/Clang, ASan/UBSan | EXECUTABLE | No cancellation after scheduling, deadlines, async/await or native worker boundary; no complete effect/ownership MIR or TSAN evidence |
| Typed uni channels | `src/{parser,infer,sema,concurrency,codegen}.cpp`, `grammar/On1x.g` | `runtime/on1x_concurrency.[ch]` and one-TU MuLIB channel/fiber/xthreads kernels; bounded FIFO/rendezvous | `->(1,1){payload:i64,capacity:0..256,mode:uni}`, consuming split, typed checked send/receive and close | `test_concurrency.{cpp,py}`, generated/fallback scanner parity, repeated rendezvous, buffered/drained close, closed-send result, deadlock/budget tests | EXECUTABLE | Only 1:1 i64 uni; no endpoint cloning/duplex, cancellation, monotonic deadline or general resource payload |
| Atomic `poll` / select | `src/{parser,infer,sema,concurrency,codegen}.cpp` | `runtime/on1x_concurrency.[ch]` calls MuLIB atomic multi-case select; 1..16 named uni/i64 offers | `recv`/`send`/mandatory `closed` and optional `default`, typed expression or statement arms; `after` rejected | `test_concurrency.py` compiled choice, rendezvous, closure, evaluation order and rejection; generated/fallback and ASan/UBSan focused tests | EXECUTABLE | No monotonic deadline, cancellation, general endpoint expressions or recoverable allocation errors; no effectful ownership/suspension MIR |
| External direct-exec `xfiber` / process cancellation | `src/{parser,infer,sema,concurrency,codegen,ppx_ast,type}.cpp`, `include/on1x/type.hpp` | `runtime/on1x_concurrency.[ch]` POSIX spawn, process group, nonblocking wait/reap, SIGTERM→SIGKILL escalation, 128 total launch budget; C11 generated program | `launch exec(...)` or single/triple backtick direct argv command (1..9 bounded words), affine `xfiber`, consuming `join` (signed POSIX status) or `cancel` (bool); process scoped to taskgroup | `examples/cooperative_external.on1x`, `test_concurrency.{cpp,py}`, scanner parity, quote/escape/operator negatives, cancellation ignoring SIGTERM, failure/budget and sanitizer tests | EXECUTABLE | No full shell expansion/operators/builtins, typed exit-status records or recoverable spawn errors; no running internal-fiber cancellation or effectful MIR |
| Captured process byte streams | `grammar/On1x.g`, `src/{lexer_fallback,parser,infer,sema,concurrency,codegen,ppx_ast,type}.cpp` | ABI v2 in `runtime/on1x_concurrency.[ch]`: three nonblocking POSIX pipes supervised through bounded MuLIB byte channels; per-write parent SIGPIPE mask restored; child SIGPIPE default | `launch capture` binds exactly `(xfiber,sender<u8>,receiver<u8>,receiver<u8>)`; checked stdin send/close and stdout/stderr `Option<u8>` receive; endpoints are affine and movable to reader fibers; byte `poll` rejects | `examples/cooperative_process_streams.on1x`, `tests/fixtures/process_stream_child.c`, `test_concurrency.{cpp,py}`; binary 0..255 round-trip, NUL, separate readers, capacity-0/1/7 backpressure, EOF/EPIPE, cancel/escalate/EOF cleanup, malformed HIR/wire/source, spawn/channel-budget failures | EXECUTABLE | Linux/glibc x86_64 only; no byte-channel constructor/select, deadline/cancellation-aware waits, typed stream/spawn errors, aggregate/effect MIR, full sanitizer/cross-platform matrix |
| Async/await, running/blocked internal cancellation, actors, native workers | `AGENTS.md`, `ON1X_CORE_SYNTAX_ADDITIONS.md` | No interruptible MuLIB channel waits or On1x cleanup edges for running internal cancellation | `cancel ifiber` rejects unless it immediately follows the launch in the same block | `test_concurrency.py` delayed/expression cancellation negatives | ABSENT | Effectful/suspending MIR, lifetime/memory model, deterministic C11 async frames and worker isolation |

M2 continuation: completed Go-style function-scoped defer and typed `Result` propagation
for the bounded C11 profile.
Re-inspected clean MuLIB `604db67c3160e7958345a3c1fefe757e5f499b95` and configured
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763` (unchanged/read-only).
Linux 6.18.52-1-lts x86_64, glibc 2.44, GCC 16.2.1, Clang 22.1.8,
CMake 4.4.3, Python 3.14.7. The initial WIP baselines were GCC/generated
47/49 and Clang/fallback 47/48 (stale defer-rejection/scanner expectations).
Final GCC/generated **51/51**, Clang/fallback **50/50**, SDK **60/60**;
focused Release/fallback/native-off **10/10**. SDK ASan/UBSan **60/60**, including
generated programs, with no findings. Exact commands and previous failures are
recorded in `../VERIFICATION.md`.
See `defer-errors.md` for the selected semantics;
this does not authorize concurrency, general foreign owners or trap unwinding.

Current M1 task: nonrecursive nominal records and parameterized enums, through
the canonical frontend and C11 value representation. Baselines at the revisions
below: GCC/generated 47/47, Clang/fallback 46/46 (`results-adts-baseline.xml`).
The nonrecursive nominal slice now executes real source programs through C11;
its final combined-tree validation is recorded in `../VERIFICATION.md`.
Recursive/owning ADTs, lists, defer/error propagation and concurrency are not
admitted by this task.

Nominal-data final combined-tree suites: GCC/generated **49/49**, Clang/fallback
**48/48**, SDK **58/58**, SDK ASan/UBSan **58/58** (including generated programs;
no findings). Focused Release/fallback/native-off **8/8**. Commands, historical
failures and unsupported profiles are preserved in `../VERIFICATION.md`.

Earlier value-loop continuation: re-inspected MuLIB
`7391d9e9d58e987ea234c64dbfd352c31f220537` and unchanged LibStone
`55755485acaded7c4ed804c0946bc8c7363f6763`. Scope: intrinsic expression
`loop`, typed `break with`, lexical targets and existing cleanup/C11/MIR paths.
Owning lists, defer/typed errors, concurrency and nominal ADTs remain pending;
this slice does not add syntax-only implementations or broader safety claims.

Value-loop combined-tree validation: GCC/generated **47/47**, Clang/fallback **46/46**,
SDK **56/56**, SDK ASan/UBSan **56/56** (generated programs instrumented; no
findings), focused GCC Release/fallback/native-off **6/6**. Exact configurations,
commands and historical failed attempts are preserved in `../VERIFICATION.md`.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Nonrecursive nominal declarations (M1 slice) | `src/{parser,type,nominal,infer,sema,loader,ppx_ast}.cpp`, `include/on1x/{ast,type,nominal}.hpp` | Existing bounded by-value C11 structs/tags; exact catalog/layout validation at backend ingress; no new runtime/foreign ABI | Named records/parameterized enums, inferred invariant arguments/phantoms, generic/forall functions, declared-catalog matching, imported type identity | `test_nominal_adts.{cpp,py}`, `examples/nominal_adts.on1x`; strict C11/C++ cross-TU, seeded oracles, hygiene/staging/PPX/imports, capture/OOM, malformed-HIR/wire and resource/budget negatives; exact results in `../VERIFICATION.md` | EXECUTABLE | Recursive/managed fields, owning/drop semantics, aggregate MIR/SDK models, separate compilation and other targets |
| Value-producing loops | `src/{parser,infer,sema}.cpp`, `src/canonical/{control,values,verify_values}.cpp`, source transforms | Existing C11 lexical RC/JSON cleanup and scalar/numeric tuple MIR slots/edges; no ABI change | `loop` expressions, labeled/typed `break with` joins; void on bare break; possible/definite initialization and backedge verification | `test_value_loops.{cpp,py}`, `examples/value_loops.on1x`; C11/MIR oracles, mutation/wire/import/generic/macro/PPX/staging/closure/JSON/fault tests; exact matrix in `../VERIFICATION.md` | EXECUTABLE | General bottom type, unique-resource transfer, aggregate/managed MIR and broader SDK models |
| Owning lists / iterator drop protocol | Existing persistent `sexpr` facades only | No native typed owning-list ABI/drop protocol | No intrinsic list type/construction/cons patterns | Existing unsupported-profile fixtures | ABSENT | Parametric/recursive layout and ownership/drop semantics; not implemented in this slice |
| Go-style function defer (M2) | `src/{parser,infer,sema,codegen}.cpp`, `src/canonical/{values,verify_values}.cpp`, `runtime/on1x_runtime.*` | Per-invocation typed LIFO records/captures in environment allocator domain; 4,096 records / 8 MiB immediate captures; located count/byte/OOM failures; C11 ABI guard | `defer call;`, registration-time callable/argument snapshots, ordinary immutable/RC captures; explicit/implicit/error returns drain before remaining lexical cleanup | `test_defer_errors.{cpp,py}`, `test_defer_runtime.c`, `examples/defer_errors.on1x`; source/HIR/wire negatives, loop/recursive/capture/outcome oracles, RC/JSON/fault/budget/C/C++ boundaries; exact profiles above and in `../VERIFICATION.md` | EXECUTABLE | No using, exception/trap/cancellation unwind, JSON captures, effectful MIR/independent SDK models, other-target certification |
| Typed recoverable Result propagation (M2) | `src/{parser,infer,sema,nominal}.cpp`, `src/canonical/{values,verify_values}.cpp`, source/PPX adapters | Existing bounded by-value nominal C11 tags; explicit checked early-return/cleanup edges; no foreign ABI change | Postfix `?` on nominal enum with exactly Ok(T)/Err(E); same declaration/error type on return, independent success type; explicit match recovery | `test_defer_errors.{cpp,py}`; exact errors/module identity, later-operand suppression, control regions, generics/hygiene/PPX, C11 O0/O2 seeded oracles and private-HIR mutations | EXECUTABLE | Managed/owning Result payloads, Option propagation, general exceptions/effect inference, aggregate MIR/SDK models, general foreign error adapters |
| Recursive / owning nominal ADTs | Nonrecursive declaration catalog only | No box/recursive allocation/drop ABI | Recursive declarations reject, including indirect/phantom cycles | `test_nominal_adts.{cpp,py}` recursive/managed rejection cases | ABSENT | Boxing, ownership/drop semantics and recursive layout/lifetime evidence |

## M2 continuation: immutable C-string admission

Baseline: MuLIB `7391d9e9d58e987ea234c64dbfd352c31f220537`, configured
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763`. This slice extends the
existing reviewed scalar admission, not the general ownership/FFI profile.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Reviewed immutable C strings | `tools/on1x-bind.cpp`, `src/safety.cpp`, `include/on1x/safety.hpp` | Exact `const char *`, bounded call-scoped inputs and non-null immutable static results; exit 72 on null/limit failure | Existing `string`; directional v4 admission evidence | `test_contract_strings.py`, `test_safety.cpp`; isolated GCC/generated 50/50, Clang/fallback 49/49, Clang/generated ASan/UBSan 50/50; commands in `../VERIFICATION.md` | EXECUTABLE | No independently reviewed standard bridge contracts; owned/retained buffers, allocation/error/effect lowering unsupported |
| General standard bridge safe exports | `stdlib/_bridge/`, `contracts-exoml/` | Existing bounded raw adapters | Existing raw externs/status tuples | Historical bridge tests below | PARTIAL | Reviewed contracts, owned-resource/effect lowering; not promoted by this slice |

Structural-match continuation: MuLIB
`9090e643e11051e000b3bcc68ebc777dfe07776a`, pinned LibStone
`55755485acaded7c4ed804c0946bc8c7363f6763` unchanged. Shared structural patterns,
guarded expression/statement matches and typed decision regions execute in C11,
with scalar/numeric tuple MIR differential tests. Previous loop/Category changes
remain intact. Full-specification concurrency/cleanup/list/value-loop capabilities
are not admitted by this slice. Exact evidence: `../VERIFICATION.md`.
Final matching suites: GCC/generated **45/45**, Clang/fallback **44/44**, SDK
**53/53**, SDK ASan/UBSan **53/53**; focused native-off Debug/Release **4/4** each.

Earlier statement-loop continuation: HEAD `9090e643e11051e000b3bcc68ebc777dfe07776a`,
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763`. Executable G1/G2 slice:
intrinsic statement loops, i64 ranges and lexical exits; existing Category work
is preserved. General match/iteration, value loops, cleanup and concurrency are
not admitted by this slice.
Final core-syntax suites: GCC/generated **43/43**, Clang/fallback **42/42**, SDK
**51/51**, SDK ASan/UBSan **51/51**; focused Debug and Release profiles **2/2**
each. Exact commands, instrumentation, migration limits and unsupported cases
are recorded in `../VERIFICATION.md` and `core-syntax.md`.

Category continuation baseline: MuLIB `9090e643e11051e000b3bcc68ebc777dfe07776a`,
LibStone unchanged at `55755485acaded7c4ed804c0946bc8c7363f6763`. CATEGORY M0/M1
now has a runnable local scalar profile: pinned native activation, balanced spans,
On1x grammar/coherence/substitution and ordinarily checked explicit selectors.
Constrained generics, cross-module evidence, associated types, functors/sealing
and auto protocols remain unimplemented. See `category.md` and the exact tests
in `../VERIFICATION.md`; this is not Category completion.
Category final suites: GCC/generated **41/41**, Clang/fallback **40/40**, SDK
**49/49**, SDK ASan/UBSan **49/49**. Standalone ordinary PPX suite: **30/30**.

Inspection baseline for this continuation: MuLIB `cbdb2e290a41e39a8e6447f39426b1407c3e73b0`,
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763` at
`on1x/third_party/libstone` (read-only). Linux x86_64; GCC 16.2.1,
Clang 22.1.8. This matrix distinguishes executable subsets from completion of
the general capabilities. Historical evidence remains in `../VERIFICATION.md`.
The two fresh pre-change baseline suites passed (GCC/generated 36/36,
Clang/fallback 35/35). Final continuation commands/results are recorded in
`../VERIFICATION.md`; the full-language mandate is not complete.

The next continuation implemented decimal f64 literals and MIR v2 for floats and
trivially copied tuples. Existing scalar artifacts remain v1. Foreign ownership
and concurrency are not admitted by these slices.

Data-model continuation implemented: structural records, closed tagged sums with
exhaustive elimination, bounded fixed arrays and owning snapshot slices. These
are a by-value C11 profile, not general borrowed/dynamic buffers or foreign ABI.

Safe-FFI continuation implemented reviewed scalar-only profiles: exact signed
integer, `_Bool` and binary64 `double` C ABI (void result), reviewed ExoML claims, all-or-nothing profile admission,
Chisel-checked generated usage, strict target compilation/execution and a
machine-readable evidence manifest. It is not general resource-safe FFI.
Post-change suites: GCC/generated **38/38**, Clang/fallback **37/37**, optional
SDK **46/46**, optional-SDK ASan/UBSan **46/46**.

M2 typed-scalar continuation: exact `_Bool`/binary64 `double` adapters, typed
Chisel admission and v2 evidence now execute C/C++/On1x boundary programs and a
seeded bit oracle. Integer-only v1 evidence is preserved without widening.
Post-continuation profiles and exact commands are recorded in `../VERIFICATION.md`.

Numeric continuation implemented `u8/u16/u32/u64` and IEEE binary32 `f32` through
the canonical HIR/C11 path, with explicit conversions/bit constructors (existing
unsuffixed literals remain i64/f64). Numeric semantics are specified in
`numerics.md`; at that stage MIR and independent structural verification still
rejected the new types (superseded by the v3 continuation below). Post-change profiles:
GCC/generated **39/39**, Clang/fallback **38/38**, optional SDK **47/47**,
SDK ASan/UBSan **47/47** (explicit generated-program sanitizer flags; see the
correction of earlier sanitizer scope in `../VERIFICATION.md`).

Numeric-limit continuation implemented scalar MIR v3 execution (unsigned/f32
operations and conversions), independent numeric HIR type obligations and
suffixed literals, preserving legacy MIR/evidence formats. Post-change suites:
GCC/generated **40/40**, Clang/fallback **39/39**, SDK **48/48**, SDK ASan/UBSan
**48/48**. Other-target floating support remains blocked on actual toolchains
and execution evidence, not deletion of platform/environment guards.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Generalized structural matching (G2 bounded profile) | `src/{parser,patterns,infer,sema}.cpp`, `src/canonical/{values,verify_values}.cpp`, loader/macro/PPX/staging adapters | Existing by-value/RC C11 and lexical cleanup; scalar/numeric tuple MIR | Guards, or/as, wildcard/rest, tuples, closed sums, records, exact arrays/snapshot slices; finite coverage/redundancy | `test_patterns.{cpp,py}`, source example, seeded MIR/C11 oracle, wire/mutation/fault/JSON tests | EXECUTABLE | Owning lists, refutable let/parameters, nominal/parametric/recursive constructors, aggregate/managed MIR and broader SDK models |
| Typed decision/initialization regions | `src/canonical/{values,verify_values}.cpp`, `src/{codegen,safety,type_safety}.cpp`, `src/backend/lower.cpp` | Existing C11 scopes/cleanup and MIR slots/branches/jumps; no runtime/ABI addition | Surface matches erased; independent initialized-local/type/tag-domination/region checks; private nodes not PPX constructors | HIR mutation rejection, transactional lowering, idempotence, guard traps, nested return/break/continue and for-latch regression | EXECUTABLE | Full effect/ownership/error/suspension Core; no general bottom type |
| Intrinsic statement loop control (G1/G2) | `src/{parser,sema,infer,codegen}.cpp`, `src/canonical/control.cpp`, `src/backend/lower.cpp`, SDK adapters | Existing C11 lexical ownership cleanup; explicit MIR branches/jumps | Statement loop/for-C, lexical labels/exits; checked targets/latches across macros/PPX/staging | `test_control.{cpp,py}`, closure allocator/JSON cleanup, MIR/oracles/wire/HIR negatives | EXECUTABLE | General iterator protocol and full effect/cleanup Core; expression-loop values are a subsequent slice above |
| Intrinsic i64 ranges | `grammar/On1x.g`, `src/lexer_fallback.cpp`, `src/{parser,canonical/control}.cpp` | Checked C11 i64; inclusive completion before increment | Once-evaluated ascending `..`/`..=` bounds; immutable iteration binding | Scanner parity, extrema/reversed/zero bounds, MIR/C11 seeded oracle | EXECUTABLE | Other scalar ranges, custom steps and general iterator/drop protocol |
| Category native protocols (M0/M1) | `src/extensions.cpp`, `src/{parser,loader,cli}.cpp`, `stdppx/native/protocols.on1x`, `stdlib/ppx/category/` | Native bootstrap processor, trusted registry/SHA256 pinning, ordinary HIR/MIR/C11 selectors | Module-local concrete scalar protocols/witnesses; generic balanced carriers; normal post-expansion typing | `test_category.py`, actual Eq/ordering clients, malformed packet/type/coherence/import/relocation tests | EXECUTABLE | Typed feedback, qualified generics, imported evidence/interfaces, associated types, functors/sealing and ownership auto protocols |
| Generated/fallback scanning | `grammar/On1x.g`, `src/lexer*.cpp` | Bounded token callback | Functional syntax, decimal floats and typed numeric suffixes | `test_scanner.cpp`, `scanner_parity.py` | EXECUTABLE | Broader malformed-input/target certification |
| Checked i64 | `src/{parser,infer,sema,codegen}.cpp` | Checked C11 runtime operations | i64 literals/operators | `test_compiler.py`, `test_mir.py` | EXECUTABLE | Broader target conformance |
| f64 values | `src/{type,parser,infer,sema,codegen,primitives,safety,type_safety}.cpp`, `runtime/on1x_runtime.*` | Linux x86_64 binary64; guarded raw/generated double ABI; bit transport/hex output | Arithmetic/comparisons, explicit conversions, tuples/closures/staging; Chisel and independent numeric typing | `test_f64.py`, `test_meta.cpp`, `test_types.cpp`, `test_type_safety.*`, `test_contracts_sdk.py` | EXECUTABLE | Other targets and full IEEE conformance certification |
| Numeric source literals | `src/{numeric,parser,lexer_fallback,macro_rules,ppx_ast,ppx_schema}.cpp`, `grammar/` | Exact integer and direct binary32/64 bit emission; 1024-byte typed literals | Decimal/hex unsigned suffixes; decimal f32/f64 suffixes; backward-compatible unsuffixed i64/f64; shared macro/PPX categories | `test_numeric_limits.py`, `test_f64.py`, `scanner_parity.py`, native/Python PPX tests | EXECUTABLE | Hex float syntax and broader compiler/host certification |
| Unsigned fixed-width words | `src/{type,parser,infer,sema,codegen,primitives,type_safety}.cpp`, `src/backend/`, `runtime/on1x_runtime.*` | Exact uint8/16/32/64 C11 ABI; checked arithmetic, wrapping/bits/conversions; scalar MIR v3 verifier/evaluator/emitter | Distinct u8/u16/u32/u64, suffixed literals; independent numeric typing; functions/tuples/value data/closures/staging (last kinds remain HIR-only) | `test_unsigned_f32.py`, `test_numeric_limits.py`, `test_contracts_sdk.py`; Python oracle, strict C/C++ and ASan/UBSan | EXECUTABLE | Broader targets; Basis word facades retain legacy encodings; general aggregate/closure MIR |
| f32 values | `src/{type,parser,infer,sema,codegen,primitives,safety,type_safety}.cpp`, `src/backend/`, `runtime/on1x_runtime.*`, PPX schemas | Linux x86_64 IEEE binary32; exact bits/hex/C float ABI; scalar MIR v3 independent evaluation and emission | Distinct f32 with directly rounded decimal literals and explicit conversions; no implicit promotion; independent typing | `test_unsigned_f32.py`, `test_numeric_limits.py`, `test_type_safety.*`, `test_contracts_sdk.py`; bit/rounding/failure oracles and ASan/UBSan | EXECUTABLE | Other targets and full IEEE conformance certification |
| Narrow signed / pointer-sized integers | `include/on1x/type.hpp` | None | None | None | ABSENT | Exact semantics and ABI for i8/i16/i32/isize/usize |
| Functions / closures | `src/{infer,sema,codegen}.cpp` | Single-thread RC environments | Structural functions, immutable captures | `test_functions.py`, `test_closures.py` | EXECUTABLE | Mutable/resource capture lifetimes |
| Tuples / irrefutable patterns | `src/{parser,infer,sema,codegen}.cpp` | Internal C11 by-value records | Shared name/tuple patterns; new matches add refutable patterns without changing `_` in old lets/parameters | `test_tuples.py`, `test_parameter_patterns.py`, `test_patterns.py` | EXECUTABLE | Foreign aggregate ABI; refutable let/parameter patterns |
| Polymorphism | `src/infer.cpp` | Bounded whole-unit specializations/bundles | Restricted schemes / forall | `test_poly*.py` | PARTIAL | General parameterized data types |
| Records / ADTs / exhaustive matching | `src/{parser,infer,sema,codegen,patterns}.cpp`, `src/canonical/`, `src/type.cpp` | Private versioned C11 structs; `ON1X_VALUE_DATA_ABI_VERSION` guard | Structural labeled records; closed sums with `variant`/exhaustive legacy or guarded structural `match`; unit record payload | `test_data.py`, `test_patterns.py`, `examples/{data_model,structural_match}.on1x` | EXECUTABLE | Nominal recursion, type parameters, derives, data equality/printing, aggregate MIR/SDK lowering |
| Arrays / slices | `src/{parser,infer,sema,codegen}.cpp`, `runtime/on1x_runtime.*` | By-value fixed arrays; owning snapshot slices with composed offsets; bounds runtime with located exit-70 errors | `array<T,N>`/`slice<T,N>` (extent <= 256), `array[...]`, `a[i]`, `a[]`, `a[lo:hi]`, slice-of-slice; no mutation/borrowing | `test_data.py`, `examples/data_model.on1x` | EXECUTABLE | Mutable arrays, zero-copy borrows, dynamic buffers, foreign ABI |
| Modules | `src/loader.cpp` | Whole-unit C11 | Namespaces, imports, aliases | `test_modules.py`, `test_loader.cpp` | PARTIAL | Interfaces, separate compilation, signatures/functors |
| Macros / PPX | `src/{macros,macro_rules,ppx,ppx_ast}.cpp` | Build-time processes; bounded wire format | Hygienic macros, external PPX | `test_macros.py`, `test_ppx.py`, `test_local_syntax.py` | EXECUTABLE | Process sandboxing; richer data derives |
| Staging | `src/staging.cpp` | AOT closure thunks | Closed code, quote/splice/lift/run | `test_staging.py` | EXECUTABLE | Broader staged data types |
| Managed data / text | `src/primitives.cpp`, `runtime/on1x_runtime.c` | RC sexpr/buffer; bounded text snapshots | Primitive managed types | `test_selfhost.py` | PARTIAL | Owning Unicode text / binary slices |
| Ownership / recoverable errors | `src/{sema,safety}.cpp`, `src/canonical/`, `runtime/` | Lexical JSON/RC cleanup and typed Result early returns; fatal failures remain terminal | Restricted resource flows; bounded nominal Result propagation and function-scoped defers | `test_json.py`, `test_safety.*`, `test_defer_errors.*` | PARTIAL | General moves/borrows, managed Result fields, full effect/cleanup MIR |
| Generic Option / Result | `src/nominal.cpp`, `stdlib/base/{option,result}.on1x` | Native bounded by-value nominal enums; older facade encodings preserved | User-declared Option/Result construction/exhaustive matching; Result `?` protocol; no Option propagation | Nominal-data/defer-error and existing library tests | PARTIAL | Canonical library generic facades and resource-bearing fields/drop |
| Exceptions / effect system | `src/backend/mir.cpp`, `runtime/` | Scalar effect chains; no unwind edges | No recoverable exceptions or inferred effect summaries | Scalar MIR tests only | PARTIAL | Typed failure/control-flow/cleanup model |
| Raw C FFI | `src/codegen.cpp`, `tools/on1x-bind.cpp` | Exact declared scalar escape hatch; generated signed/unsigned integer, boolean and binary32/64 adapters assert signature/type/size/alignment/format/range | extern; explicit parameter flattening | `test_contracts*.py`, `test_parameter_patterns.py`, `test_unsigned_f32.py` | PARTIAL | Enums, aggregates, callbacks and resources |
| Safe foreign admission | `tools/on1x-bind.cpp`, `src/{cli,safety}.cpp`, `include/on1x/safety.hpp` | `reviewed-scalar/v1`: signed integer/void; v2 adds bool/f64; v3 adds unsigned/f32. Generated C11 facade, strict target boundary run, `on1x.safe-ffi/v1` manifest and bounded versioned admitted-extern evidence | Ordinary `on1x_safe_*` functions; raw declarations remain explicit and Chisel requires exact declaration/callsite parameter/result evidence | `test_contracts_sdk.py`, `test_safety.*` | EXECUTABLE | Accountable real reviews for production bundles; pointers/resources/errors/aggregates/callbacks, Wetstone effects and broader targets |
| Boolean/binary64 foreign admission (M2 continuation) | `tools/on1x-bind.cpp`, `src/safety.cpp` | Exact C11 `_Bool`/`double` and typedefs; separate-TU C/C++ boundary, binary64 target/format/relaxed-mode guards | Typed `bool`/`f64` extern evidence, scalar Chisel lowering | `test_contracts_sdk.py`, `test_safety.*`, `tests/fixtures/contract_scalars.on1x`; Python oracle, ASan/UBSan | EXECUTABLE | Other floating formats/targets, floating environment effects and broader contract verification |
| Native kernel planning | `std/mulib/`, `tools/on1x-exotract.py` | One implementation TU plus companions | Metadata, raw adapters | `test_mulib.py`, `test_exotract.py` | EXECUTABLE | Not general safe language bindings |
| MIR / reference evaluator | `src/backend/{mir,lower,serialize,evaluate,emit_c}.cpp`, `floating.hpp`, `numeric*.hpp` | Legacy v1/v2 plus complete scalar numeric v3; allowlisted descriptors, exact typed payloads and independent C++ evaluation | Source/saved numeric MIR, trivial tuple destructuring/calls/slots/phis, checked optimizations | `test_mir.*`, `test_f64.py`, `test_unsigned_f32.py`, `test_numeric_limits.py` | PARTIAL | Managed aggregates, records/ADTs/arrays, closures, ownership, cleanup and async effects |
| Trivial tuple MIR | `src/backend/{mir,lower,serialize,evaluate,emit_c}.cpp` | By-value copies; no owned/borrowed fields; bounded recursive evaluator storage | Nested bool and all admitted numeric leaves (v2/v3); shared source patterns | `test_mir.*`, `test_numeric_limits.py` | EXECUTABLE | Resource-bearing aggregates, foreign layouts, separate compilation |
| MIR passes / relational analyses | `src/backend/{passes,relational}.cpp` | Conservative v1/v2/v3 passes; no float/v3 numeric folding; Novostone v1 only | Explicit opt-in, versioned pass descriptors and numeric-v3 target | `test_passes.*`, `test_relational.*`, `test_mir.py`, `test_numeric_limits.py` | PARTIAL | v2/v3 relational mapping, full-language legality and ownership descriptors |
| C11 AOT | `src/codegen.cpp`, `runtime/` | Standalone C11 for admitted subset | Default backend | Compiler/native suite | EXECUTABLE | Broader language/runtime representation |
| Native / VM backend | `BACKEND.md` | None | None | None | DESIGNED | Real equivalent code generator |
| Async/await and native structured concurrency | `AGENTS.md`, `docs/concurrency.md` | Cooperative G4 runtime only; no async frame/native worker ABI | Taskgroup only in the initial stackful single-thread profile | `test_concurrency.{cpp,py}` tests its narrower profile | PARTIAL | Effectful MIR, async frames, cancellation/ownership and native worker isolation |
| General channels / select / actors / workers | MuLIB headers; LibStone Plaid; `src/concurrency.cpp` | Bounded i64 1:1 uni channels and atomic `poll`, plus role-checked process byte streams; no actor/worker adapter | Checked send/receive/close and bounded i64 offers only | `test_concurrency.{cpp,py}`; general gate absent | PARTIAL | General message ownership, byte/general `poll`, duplex/quota, deadlines/cancellation, actors/workers |
| Diagnostics / repair | `src/diagnostics.cpp` | Optional Syren build-time adapter | Located errors; proposed edits only | `test_diagnostics.*` | PARTIAL | Full stable diagnostic taxonomy |
| Formatter / LSP / linter / packages | `tools/` | Build helper only | None for these tools | Existing installation tests | ABSENT | Implement tools and protocol guarantees |
| Standard library | `stdlib/`, `std/` | Persistent helpers, trusted scalar bridges | Partial functional modules | `test_libraries.py`, `test_stdlib_extensions.py` | PARTIAL | Coherent data/resource model and conformance |
| Self-hosting | `stdlib/{sexpr,ppx,visitor}/` | C11 native authoring tools | Compiler remains C++20 | `test_selfhost.py` | PARTIAL | Real compiler-stage migration/bootstrap |

No row is newly certified VERIFIED by source inspection alone. Optional SDKs,
cross-target ABI and TSAN are not implied by default-build tests.
