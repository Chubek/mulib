# On1x verification — 2026-11-18

## G4/v3 command literals and captured process streams — 2026-11-18

This continuation uses MuLIB HEAD
`8996396506901f792c0b697ebbf8d6a2a233f0f6` and configured, unchanged
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763`. Platform: Linux
6.18.52 x86_64, glibc 2.44, GCC 16.2.1, Clang 22.1.8, CMake 4.4.3 and
Python 3.14.7.

The generated/fallback scanners now recognize bounded single- and
triple-backtick command literals with exact token/diagnostic parity. The source
parser decodes one absolute-path argv command (whitespace, single/double quotes
and escapes) and rejects expansion, assignment, command lists, pipelines and
redirection rather than delegating to a host shell. `launch capture` must bind
exactly `(xfiber, sender<u8>, receiver<u8>, receiver<u8>)`. Runtime ABI v2 uses
three nonblocking POSIX pipes and existing bounded MuLIB channels; stdin has
checked send/close, stdout/stderr return `Some(u8)` then `None`, and endpoint
capabilities may move into separate cooperative reader fibers. `join`/`cancel`
close stdin. The adapter blocks SIGPIPE only around each parent pipe write,
consumes the newly generated signal on `EPIPE`, and restores the previous thread
mask before rescheduling; it does not replace the caller's process-wide
disposition. Spawned children use the default SIGPIPE disposition, so a closed
child stdin becomes checked channel closure rather than killing generated code.

`tests/test_concurrency.py` executes all byte values (including NUL) twice,
capacity-7 backpressure, capacity-zero rendezvous streams, separate stdout and
stderr reader fibers, EOF and EPIPE, captured SIGTERM→SIGKILL cancellation and
post-cancel stream closure, quote/escape preservation, direct inherited streams,
spawn/channel-budget failures, affine direction/alias/join negatives, and
explicit byte-`poll` rejection. `tests/test_concurrency.cpp` covers PPX wire
round-trip, exact captured tuple types, canonical C11 calls, malformed-HIR
rejection and MIR refusal. `tests/fixtures/process_stream_child.c` is compiled as
a separate strict-C11 process boundary. The source example is
`examples/cooperative_process_streams.on1x`.

Fresh commands from the repository root:

```sh
cmake -S on1x -B /tmp/opencode/on1x-csp-check -DBUILD_TESTING=ON \
  -DON1X_BUILD_NATIVE_PPX=OFF -DON1X_USE_FALLBACK_SCANNER=ON
cmake --build /tmp/opencode/on1x-csp-check --parallel 4
ctest --test-dir /tmp/opencode/on1x-csp-check --output-on-failure --parallel 4

cmake -S on1x -B /tmp/opencode/on1x-csp-generated -DBUILD_TESTING=ON \
  -DON1X_BUILD_NATIVE_PPX=OFF -DON1X_USE_FALLBACK_SCANNER=OFF
cmake --build /tmp/opencode/on1x-csp-generated --parallel 4
ctest --test-dir /tmp/opencode/on1x-csp-generated \
  --output-on-failure --parallel 4

CC=clang CXX=clang++ cmake -S on1x -B /tmp/opencode/on1x-csp-sanitize \
  -DBUILD_TESTING=ON -DON1X_BUILD_NATIVE_PPX=OFF \
  -DON1X_USE_FALLBACK_SCANNER=ON \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build /tmp/opencode/on1x-csp-sanitize --parallel 4 \
  --target on1xc on1x_concurrency_hir_tests
ctest --test-dir /tmp/opencode/on1x-csp-sanitize \
  -R 'on1x\.concurrency' --output-on-failure

python3 on1x/tests/scanner_parity.py \
  /tmp/opencode/on1x-csp-generated/on1x_generated_tokens \
  /tmp/opencode/on1x-csp-generated/on1x_reference_tokens \
  on1x/examples /tmp/opencode/on1x-final-parity
python3 on1x/tools/on1x-build.py \
  on1x/examples/cooperative_process_streams.on1x \
  --on1xc /tmp/opencode/on1x-csp-generated/on1xc --cc clang \
  -o /tmp/opencode/on1x-process-streams-clang --cflag=-O2
/tmp/opencode/on1x-process-streams-clang
ldd /tmp/opencode/on1x-process-streams-clang
```

Results: fallback/GCC full suite **50 passed, 1 failed, 1 skipped**
(52 registered); generated/GCC full suite **51 passed, 1 failed, 1 skipped**
(53 registered); fallback/Clang ASan+UBSan focused **2/2**, including generated
process-stream executables; exact scanner parity **300 cases**. This includes
adjacent empty/single/triple delimiters and rejects an unterminated triple opener
at that opener rather than reinterpreting it as empty single-backtick tokens.
The sole full-suite failure remains the pre-existing stale
`tools/embed_units.py` fixture fingerprint in `on1x.mulib`; `on1x.category`
skips with native PPX disabled. Strict GCC and Clang C11 examples both print
`79`, `110`, `49`, `120`, `0`. `ldd` lists only libc and the Linux loader, not
LibStone, On1x/compiler, Lua or C++ runtime libraries. TSAN is not applicable to
this one-native-thread scheduler/process profile.

Status remains **EXECUTABLE**, not VERIFIED or complete M4 concurrency. The
full portable command grammar (environment expansion/mutation, command lists,
pipelines, redirection and builtins), typed exit/spawn errors, byte `poll`,
running/blocked internal cancellation, effectful ownership MIR, async/await,
native workers and non-Linux targets remain unsupported and diagnose or are
outside this profile.

## G4/v2 external process fibers and process cancellation — 2026-11-18

This continuation uses MuLIB HEAD `aa36e9042d06cf85006b4f27c363b2226a1119e2`
and configured, unchanged LibStone `55755485acaded7c4ed804c0946bc8c7363f6763`.
Linux/glibc x86_64; GCC 16.2.1 and Clang 22.1.8. Existing uncommitted
concurrency/defer and unrelated MuLIB work was preserved. No submodule update.

`examples/cooperative_external.on1x` cancels a never-dispatched internal fiber,
runs `/bin/true`, cancels `/bin/sleep` and observes `/bin/false` via generated
C11, with no C++ or LibStone runtime.
This is a **bounded POSIX direct-exec** path: literal argv, absolute executable,
inherited environment and stdout/stderr, fresh process group, nonblocking
`waitpid` supervisor, consuming join/cancel with reaping, SIGTERM then SIGKILL
after 250 monotonic milliseconds. Source-positive, source-negative, parent
ownership, double-consumption, literal argument preservation, repeated reap,
spawn/budget failures, terminal-failure child termination, and an independently
compiled child that ignores SIGTERM
exercise the boundary. PPX wire round-trip and canonical C11 emission are
covered. The additional immediately following standalone `cancel ifiber;`
consumes and joins a not-yet-dispatched child without running its body;
repeated cancellation and nonadjacent/expression forms reject. Running or
blocked internal-fiber cancellation, portable shell literals, captured typed
streams, recoverable spawn errors and effectful MIR remain unsupported.

Commands from the repository root (the build directories were configured as
recorded in the original G4 section below):

```sh
cmake --build /tmp/opencode/on1x-csp-check --parallel 4
ctest --test-dir /tmp/opencode/on1x-csp-check --output-on-failure --parallel 4
cmake --build /tmp/opencode/on1x-csp-generated --parallel 4 \
  --target on1xc on1x_concurrency_hir_tests
ctest --test-dir /tmp/opencode/on1x-csp-generated \
  -R 'on1x.(concurrency|scanner_parity)' --output-on-failure
cmake --build /tmp/opencode/on1x-csp-sanitize --parallel 4 \
  --target on1xc on1x_concurrency_hir_tests
ctest --test-dir /tmp/opencode/on1x-csp-sanitize \
  -R 'on1x.concurrency' --output-on-failure
python3 on1x/tools/on1x-build.py on1x/examples/cooperative_external.on1x \
  --on1xc /tmp/opencode/on1x-csp-check/on1xc --cc clang \
  -o /tmp/opencode/on1x-external-strict --cflag=-std=c11 \
  --cflag=-Wall --cflag=-Wextra --cflag=-Werror
/tmp/opencode/on1x-external-strict
ldd /tmp/opencode/on1x-external-strict
```

Results: fallback/GCC full suite **50 passed, 1 failed, 1 skipped**
(52 registered); generated/GCC focused **3/3**; fallback/Clang ASan+UBSan
focused **2/2** including generated executables and the SIGKILL escalation
fixture. The full-suite `on1x.mulib` failure remains the pre-existing stale
`tools/embed_units.py` fixture fingerprint; `on1x.category` skips in the
native-PPX-off configuration. Strict GCC and Clang C11 were exercised by the
focused programs. TSAN is not applicable to the one-native-thread On1x
profile; external processes have separate address spaces. These results do
not make the full concurrency or external shell specification VERIFIED. The
strict example prints `0`, `true`, `1`; `ldd` lists libc and the Linux loader,
not LibStone, On1x compiler libraries or a Lua/C++ runtime.

## G4 cooperative fibers/channels — bounded source/C11 continuation

Follow-up (same HEAD and configured SDK): bounded `poll` now lowers to one
atomic MuLIB multi-case selection with mandatory close arms, optional default,
and checked i64/uni send/receive offers. `tests/test_concurrency.py` adds
compiled buffered, rendezvous, closed-send/receive, evaluation-order and
negative diagnostics; `tests/test_concurrency.cpp` adds source-wire and C11
emission checks. From the repository root:

```sh
cmake --build /tmp/opencode/on1x-csp-check --parallel 4 --target on1xc on1x_concurrency_hir_tests
ctest --test-dir /tmp/opencode/on1x-csp-check -R 'on1x.concurrency' --output-on-failure
cmake --build /tmp/opencode/on1x-csp-generated --parallel 4 --target on1xc on1x_concurrency_hir_tests
ctest --test-dir /tmp/opencode/on1x-csp-generated -R 'on1x.(concurrency|scanner_parity)' --output-on-failure
cmake --build /tmp/opencode/on1x-csp-sanitize --parallel 4 --target on1xc on1x_concurrency_hir_tests
ctest --test-dir /tmp/opencode/on1x-csp-sanitize -R 'on1x.concurrency' --output-on-failure
python3 on1x/tools/on1x-build.py on1x/examples/cooperative_poll.on1x \
  --on1xc /tmp/opencode/on1x-csp-check/on1xc --cc clang \
  -o /tmp/opencode/on1x-poll-example --cflag=-std=c11 --cflag=-Wall \
  --cflag=-Wextra --cflag=-Werror
/tmp/opencode/on1x-poll-example
```

Follow-up results: fallback/GCC full suite **50 passed, 1 failed, 1 skipped**
(52 registered); generated/GCC focused **3/3**; fallback/Clang ASan/UBSan
focused **2/2**, including the new poll programs; strict Clang C11 example
compiled and ran (`42`, `1`). The sole full-suite failure remains the
pre-existing stale `tools/embed_units.py` profile fingerprint in `on1x.mulib`;
`on1x.category` skips without native PPX. No poll deadlines or cancellation
protocol is implemented. External
processes still need a portable shell grammar, process ownership/streams and
safe scheduler integration; neither is represented as an `ifiber<i64>`.

Scope: preserve and validate the existing uncommitted cooperative-fiber/channel
implementation, fix contextual syntax regressions, and add executable source,
canonical-HIR and failure-path coverage. This does **not** complete
`ON1X_CORE_SYNTAX_ADDITIONS.md` or the M4 concurrency milestone. Re-inspected
MuLIB HEAD `aa36e9042d06cf85006b4f27c363b2226a1119e2`, configured read-only
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763`; the working tree
already contained uncommitted concurrency/defer and unrelated MuLIB work, which
was preserved. Linux x86_64, GCC 16.2.1, Clang 22.1.8, Python 3.14.7.

The admitted `taskgroup`, eager `launch`/consuming `join` and i64 1:1 uni channel
path uses the canonical parser/inference/checker, explicit checked CSP HIR,
the C11 emitter and the existing MuLIB fiber/channel/xthreads kernels in a
single implementation TU. The build helper links the adapter automatically.
`examples/cooperative_channels.on1x` executes a zero-capacity acknowledged
rendezvous and receives a drained close. `tests/test_concurrency.py` exercises
strict generated C11 with GCC and Clang, `-O2`, buffered close and closed send,
64 repeated rendezvous, nested group, function return and fiber defer cleanup,
runtime deadlock/task-budget failures, compile-time affine/capture/arity/quota/
effect/unsupported-profile negatives and failed-output preservation.
`tests/test_concurrency.cpp` tests source-wire round-trip, canonical erasure,
malformed HIR rejection before emission, invalid provenance and MIR refusal.
The real ASDL/Aurocks and fallback scanners agree. Parser compatibility fixes
preserve `split!(...)` macros and variable `close` in `if close { ... }`, while
admitting `match <-rx with`. Additive PPX-v1 source constructors were synchronized
with the standalone schema; they convey no semantic authority.

From the repository root, exact focused validation commands:

```sh
cmake -S on1x -B /tmp/opencode/on1x-csp-check -DBUILD_TESTING=ON \
  -DON1X_BUILD_NATIVE_PPX=OFF -DON1X_USE_FALLBACK_SCANNER=ON
cmake --build /tmp/opencode/on1x-csp-check --parallel 4
ctest --test-dir /tmp/opencode/on1x-csp-check --output-on-failure --parallel 4 \
  --output-junit /tmp/opencode/on1x-csp-check/results-csp-final.xml
cmake -S on1x -B /tmp/opencode/on1x-csp-generated -DBUILD_TESTING=ON \
  -DON1X_BUILD_NATIVE_PPX=OFF -DON1X_REQUIRE_LIBSTONE=ON
cmake --build /tmp/opencode/on1x-csp-generated --parallel 4
ctest --test-dir /tmp/opencode/on1x-csp-generated --output-on-failure \
  -R '^on1x.(concurrency|concurrency_hir|scanner_parity|ppx|macros)$'
cmake -S on1x -B /tmp/opencode/on1x-csp-sanitize -DBUILD_TESTING=ON \
  -DON1X_BUILD_NATIVE_PPX=OFF -DON1X_USE_FALLBACK_SCANNER=ON \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined' \
  '-DON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer'
cmake --build /tmp/opencode/on1x-csp-sanitize --parallel 4 \
  --target on1xc on1x_concurrency_hir_tests
ctest --test-dir /tmp/opencode/on1x-csp-sanitize --output-on-failure \
  -R '^on1x.concurrency(_hir)?$'
```

Results: generated/GCC focused **5/5**; fallback/GCC complete **50 passed,
1 failed, 1 skipped** (52 registered); fallback/Clang ASan+UBSan focused
**2/2**, including generated applications and MuLIB kernels. ASan prints its
known `makecontext/swapcontext` support warning; no sanitizer findings in
the exercised normal paths. Terminal deadlock and budget tests disable leak
reporting because `exit(73)` does not unwind. The pre-change shared-tree
baseline was **41 passed, 8 failed, 1 skipped** of 50; fixing the two
contextual parser regressions and standalone PPX schema resolves seven of
the eight baseline failures. `on1x.mulib` still fails with a *pre-existing*
stale `tools/embed_units.py` fixture fingerprint; it was not regenerated or
called a green suite. The `on1x.category` test skips in the native-PPX-off
configuration. The full generated-scanner suite after the final fixes was
not rerun; its focused parity/PPX/macros/concurrency checks passed.

Unsupported at the time of the original baseline below: `poll`, cancellation/deadlines, `xfiber`/shell command grammar,
duplex/MPMC and arbitrary payloads, native workers/actors, stackless async,
effectful ownership MIR, other platforms and cross-target ABI. TSAN is not
evidence for this one-native-thread profile. Fatal runtime errors do not
unwind; this is EXECUTABLE, not VERIFIED, full general-purpose concurrency.

## M2 continuation — finish function-scoped defer and typed Result propagation

Scope: complete the committed WIP contract in [defer-errors.md](docs/defer-errors.md),
not general exceptions, foreign owners or concurrency. Re-inspected clean MuLIB
`604db67c3160e7958345a3c1fefe757e5f499b95` and actual configured LibStone
`55755485acaded7c4ed804c0946bc8c7363f6763` at `on1x/third_party/libstone`.
LibStone remains unchanged/read-only. Linux `6.18.52-1-lts` x86_64, glibc 2.44,
GCC/g++ 16.2.1 (`20260810`), Clang/clang++ 22.1.8, CMake 4.4.3, Python 3.14.7.
No commit, push, SDK/submodule update or unrelated MuLIB kernel change.

### Behavior and implementation boundary

- `defer call;` snapshots the callable and arguments once, left-to-right, at
  registration. Reached registrations belong to the function/lambda invocation,
  survive block/break/continue exits, accumulate in loops and execute LIFO on
  explicit/implicit and typed-error returns. Return operands are saved first;
  defers run before remaining temporary/local cleanup. Managed call results are
  discarded/released, and deferred `Err` results do not mask the pending outcome.
- Postfix `?` admits only a nominal two-parameter enum with exactly Ok(T)/Err(E).
  Error returns use the same declaration and exact error type, allowing a different
  success type. Existing canonical regions/tag tests/projections/initialization/
  returns preserve early-operand cleanup and skip later operands. Explicit `match`
  supplies recovery. Generic, imported and equivalently defined alternate-name
  protocols work; module identities and error types are not coerced.
- The WIP omitted defer/propagation discovery in `needs_values`. It is completed,
  with checked-source lowering guards and independent thunk/return/variant checks.
  Private registration cannot enter through PPX source wire. Existing canonical
  source grammar, inference, nominal protocols, hygienic transforms and source
  PPX constructors are reused rather than replaced.
- C11 emits typed invocation/destruction helpers and a stack only in functions
  that actually register defers. Runtime records and immediate capture environments
  share the existing environment allocator; both count (4,096) and byte (8 MiB)
  budgets are checked before allocation, including size overflow. Record/capture
  OOM has a located exit-72 diagnostic; a failed capture allocation frees its
  pending record. No erased function-pointer ABI, longjmp or runtime compiler.
- Source changes: `src/canonical/{values,verify_values}.cpp`,
  `src/{parser,codegen,type_safety}.cpp`, `runtime/on1x_runtime.{c,h}` and CMake.
  Tests: `tests/test_defer_errors.{cpp,py}`, `tests/test_defer_runtime.c`,
  `tests/fixtures/defer_{foreign,json_allocator}.c`, allocator live-count test hook,
  scanner parity and the formerly unsupported-defer control negative. Example:
  `examples/defer_errors.on1x`. Linked manuals/matrix/roadmap are updated.

### Baseline and regression evidence

Before implementation, rebuilding the existing generated/GCC and fallback/Clang
trees and running the complete suites gave **47/49** and **47/48**. These are
recorded in `results-defer-baseline.xml` / `ctest-defer-baseline.log` in each tree.
Failures were the old `cleanup_unsupported` test's parser-error expectation and
the generated build's lexical-invalid `?` fixture. A standalone
`fn main(){defer println(42);}` reproduced `ON1X-VALUES-VERIFY: unlowered defer`.
Tests now validate the implemented syntax and retain negative resource captures;
no test is disabled to obtain green results.

The new suites cover actual C11 execution, seeded independent outcome/LIFO
oracles at `-O0/-O2`, saved return values, per-call/recursive stacks, loop exits,
partial registration, short-circuiting, nested control regions and exact aggregate
error types. They exercise ordinary functions/closures/intrinsics and exact raw
flattened C calls, generic specialization, hygiene/macros, imports, staging and
both native/Python identity PPX. RC and canonical JSON counters check normal and
propagated-error cleanup, including abandoned operands, deferred reference
lifetime, shared-buffer reference semantics and discarded managed results.
Registration OOM is injected at every record/capture allocation in a two-action
fixture. Count/byte exhaustion and body/return/deferred traps are separate tests.
The fatal-path allocator hook also checks the exact surviving allocation count:
capture OOM must free the pending record while earlier committed registrations
retain their documented terminal-failure lifetime.

Independent C runtime callers test stack isolation/reuse, nested invocation,
destruction and malformed allocation sizes. Separate-TU C/C++ callers share the
actual generated interface and execute success/error/defer paths: this does not
establish separate On1x compilation or a foreign aggregate ABI. HIR/wire mutations
reject missing captures, invalid thunks, surviving source operations, wrong tags,
payloads, initializations, returns and cross-function outcomes. Existing output
files survive source/unsupported-profile errors.

The first full post-change runs passed all behavioral/legacy tests but failed
the new HIR suite's located-error assertion: its hand-built malformed wire lacked
a wire filename. The fixture now round-trips through the real wire reader with
`bad-wire` provenance before testing rejection, without relaxing the assertion.
The earlier results remain `results-defer.xml`. A stale Release makefile initially
did not know the new target; explicit CMake configuration fixed that build setup.

### Exact configurations and commands

The five existing trees retain the exact configurations from the earlier
value-loop/nominal sections below. GCC Debug uses the generated scanner; Clang
Debug uses fallback. The SDK tree uses generated scanning plus real
ExoML/Exotract/Chisel/Nerotypical/Novostone and the separately prepared Satie SDK.
The sanitizer tree has the same SDKs and instruments compiler/runtime/processors
and generated C/C++/foreign fixtures with `-fsanitize=address,undefined
-fno-omit-frame-pointer`, including explicit
`ON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer`.
Only intentional fatal non-unwinding subprocesses disable leak reporting;
ASan/UBSan remain active and normal/error-return leak checks remain enabled.

For each `<tree>` in `/tmp/opencode/on1x-values-{gcc,clang,sdk,sanitize}`:

```sh
cmake --build <tree> --parallel 2
ctest --test-dir <tree> --output-on-failure --parallel 2 \
  --output-junit <tree>/results-defer-final.xml
```

Completed builds never overlap a relink or another CTest run in that same tree.
| Final profile | Result |
|---|---|
| GCC/generated Debug, native processors | 51/51 |
| Clang/fallback Debug, native processors | 50/50 |
| Clang/generated Debug + ExoML/Exotract/Chisel/Nerotypical/Novostone | 60/60 |
| Same SDK + compiler/runtime/generated-program ASan/UBSan | 60/60, no sanitizer findings |
| GCC Release/fallback/native-off, focused control/matching/value/nominal/defer suites | 10/10 |

JUnit artifacts are `results-defer-final.xml` in each tree.

After the final exact-live-allocation assertion was added to the fault fixture,
the affected defer/closure/tuple/staging/value-loop suites were rerun in all five
trees above: **5/5 each**, including ASan/UBSan, with no findings:

```sh
ctest --test-dir <tree> --output-on-failure --parallel 2 \
  -R '^on1x.(defer_errors|closures|tuples|staging|value_loops)$' \
  --output-junit <tree>/results-defer-allocation-final.xml
```

Release/fallback/native-off focused profile:

```sh
cmake -S on1x -B /tmp/opencode/on1x-values-release
cmake --build /tmp/opencode/on1x-values-release --parallel 2 --target \
  on1xc on1x_defer_error_tests on1x_control_tests on1x_pattern_tests \
  on1x_value_loop_tests on1x_nominal_tests
ctest --test-dir /tmp/opencode/on1x-values-release --output-on-failure \
  --parallel 2 -R '^on1x.(control|pattern|patterns|value_loop|value_loops|nominal|nominal_adts|defer_error|defer_errors)' \
  --output-junit /tmp/opencode/on1x-values-release/results-defer-final.xml
```

Standalone acceptance and prior hello regression:

```sh
python3 on1x/tools/on1x-build.py on1x/examples/defer_errors.on1x \
  --on1xc /tmp/opencode/on1x-values-gcc/on1xc --cc gcc \
  -o /tmp/opencode/on1x-defer-errors-example
/tmp/opencode/on1x-defer-errors-example
readelf -d /tmp/opencode/on1x-defer-errors-example
python3 on1x/tools/on1x-build.py on1x/examples/hello.on1x \
  --on1xc /tmp/opencode/on1x-values-clang/on1xc --cc clang \
  -o /tmp/opencode/on1x-defer-baseline-hello
/tmp/opencode/on1x-defer-baseline-hello
```

Defer example output: `21`, `99`, `42`, `-1`, `99`, `7`, `2`, `1`, `0` on separate
lines; only `libc.so.6` is dynamically required. Hello prints `21` as before.
The same emitted program also passed strict GCC static-archive and shared-runtime
links, using artifacts under `/tmp/opencode/on1x-defer-link`:

```sh
/tmp/opencode/on1x-values-gcc/on1xc --emit-c on1x/examples/defer_errors.on1x \
  -o /tmp/opencode/on1x-defer-link/program.c
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -I on1x/runtime -fPIC -shared \
  on1x/runtime/on1x_runtime.c -o /tmp/opencode/on1x-defer-link/libon1x-defer.so
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -I on1x/runtime \
  /tmp/opencode/on1x-defer-link/program.c -L /tmp/opencode/on1x-defer-link \
  -lon1x-defer -Wl,-rpath,/tmp/opencode/on1x-defer-link \
  -o /tmp/opencode/on1x-defer-link/shared
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -I on1x/runtime \
  /tmp/opencode/on1x-defer-link/program.c \
  /tmp/opencode/on1x-values-gcc/libon1x_runtime.a \
  -o /tmp/opencode/on1x-defer-link/static
/tmp/opencode/on1x-defer-link/shared
/tmp/opencode/on1x-defer-link/static
```

Changing the copied header's `ON1X_DEFER_ABI_VERSION` to 99 and compiling with
`gcc -std=c11 -I /tmp/opencode/on1x-defer-link/bad-abi -fsyntax-only
/tmp/opencode/on1x-defer-link/program.c` rejects with the explicit incompatible
defer runtime ABI diagnostic. Python syntax, canonical/native PPX schema equality
and `git diff --check` also pass.

Two fresh emissions of the same example agree byte-for-byte in both scanner
builds (including equal SHA-256
`65524b324e57c67bc6c89ab2d0ad323e0a77eee2c8b9c596322a71ce307b8c23`
for invocation from this repository root). A separate seeded truncation/token
mutation check (seed `2026111704`) of that example tested 183 inputs with each compiler, without
crashes or unlocated source failures; an empty unit retains its existing
`<unit>:1:1: missing main function` diagnostic. This is bounded robustness
evidence, not a general fuzzing-conformance claim.

Unsupported: using, block-defer shorthand, exceptions/handlers/finally,
panic/recover, trap/cancellation unwind, JSON owner/view capture, owning/managed
Result fields, Option propagation, implicit error conversion, general foreign
error/ownership admission and dynamic defer/aggregate-result MIR/SDK models.
Raw foreign pointer/text lifetimes remain unsafe caller obligations. Legacy
payload-lambda match arms retain their old function boundary; use `match ... with`
to propagate from an arm into its surrounding function. Only Linux x86_64 was
executed; no 32-bit/non-Linux/native-machine-backend or full-language claim.
TSAN is not applicable evidence for this single-threaded increment. Optional
Exotract Lua remains disabled; the real native SDK integrations are exercised.

## M1 continuation — nonrecursive nominal records and parameterized enums

Scoped dependency slice of the request for owning lists, defer/typed errors,
concurrency and nominal ADTs. **This increment implements nonrecursive nominal
data only.** Named `Result<T,E>` values can be matched, but there is no implicit
error propagation, unwinding, owning list, boxed recursion or task runtime.
See [the exact source/ownership/ABI contract](docs/nominal-adts.md).

Re-inspected MuLIB `7391d9e9d58e987ea234c64dbfd352c31f220537`; configured
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763` remains clean/read-only at
`on1x/third_party/libstone`. Linux `6.18.52-1-lts` x86_64, glibc 2.44,
GCC/g++ 16.2.1, Clang/clang++ 22.1.8, CMake 4.4.3 and Python 3.14.7.
The existing uncommitted value-loop and immutable C-string FFI work is preserved.
No commit, push, submodule update or new SDK/runtime dependency.

### Implemented behavior and review boundaries

- Intrinsic `record Name<T>{...}` / `enum Name<T,E>{...}`, source named-type
  references and explicit typed record/variant constructors. Existing structural
  syntax is unchanged. Module type namespaces, aliases/re-exports and canonical
  identity preserve distinct declarations even with equal representations.
- Argument sharing, invariant phantom parameters, generic specialization and
  explicit forall proofs extend the existing inference graph. The checked type
  retains its existing Record/Sum representation plus nominal identity/arguments;
  C11 emits distinct internal structs without a runtime ABI change.
- `src/nominal.cpp` validates all declaration catalogs (including unused ones),
  rejects cycles/resources/unknown types, checks bounded layout width/height and
  reconstructs checked layouts independently. Backend entry rejects tampered
  representations, missing catalogs and null HIR children. Cached layout height
  prevents shallow-cache reuse from bypassing depth exhaustion.
- New source PPX constructors `NamedType`/`NominalDeclaration` retain source
  declarations, not layout certificates. Both schemas are semantically equal;
  native processors were rebuilt. Python/native derive preserve data declarations
  and render named signatures; cfg/FFI processors do not misread them as functions.
  Category remains its existing pinned native scalar profile.
- Source files: `include/on1x/{ast,type,clone,nominal}.hpp`,
  `src/{parser,type,nominal,infer,sema,loader,ppx_ast,cli,codegen,extensions}.cpp`,
  `src/backend/lower.cpp`, grammar/schema inputs and `CMakeLists.txt`.
  Tests: `tests/test_nominal_adts.{cpp,py}`, `examples/nominal_adts.on1x`.
  PPX changes: `stdlib/ppx/signature.on1x`, `stdppx/{native/derive.on1x,
  _support/passes.py,schemas/PPX.schema.json}`. `FILE_HASHES.json` refreshes the
  changed signature helper and three pre-existing stale PPX source entries;
  all 563 file hashes now match. Their source contents were not overwritten.
- Executable evidence covers exact type isolation/phantoms, constructor order,
  nested data/arrays/slices, generic/forall functions, source match/value loops,
  macro hygiene/staging, imports/unimported-type rejection, source-wire/layout
  mutations, six seeded Python result oracles, strict C11/O2, and actual aggregate
  calls across C/C++ translation units sharing one generated interface. That
  boundary test does **not** establish separate On1x compilation or aggregate FFI.
- Closure capture tests count live environments and inject allocation failure;
  no nominal value itself allocates or has a destructor. Existing terminal
  failures still do not unwind. Negative cases reject recursive/managed/resource
  fields, wrong constructors/arguments/payloads, layout budgets and foreign ABI.

### Exact validation commands

The four Debug build trees reuse the **exact configuration commands in the
value-loop section below**, including generated versus fallback scanner flags,
strict warnings, the real optional SDKs and the separately prepared Satie SDK.
Both compiler/runtime and generated-program tests are instrumented in the
sanitizer tree (`ON1X_TEST_C_FLAGS` includes ASan/UBSan).

For each `<tree>` in `/tmp/opencode/on1x-values-{gcc,clang,sdk,sanitize}`:

```sh
cmake --build <tree> --parallel 2
ctest --test-dir <tree> --output-on-failure --parallel 2 \
  --output-junit <tree>/results-nominal-adts-final.xml
```

Completed-build tests never overlap a relink or another CTest run in that tree.
Baseline reruns before nominal edits were GCC/generated 47/47 and
Clang/fallback 46/46 (`results-adts-baseline.xml`). Before rebuilding the old
Release compiler, `on1x-values-release/on1xc --check
on1x/examples/nominal_adts.on1x` reproduced `1:1: expected 'fn', found 'record'`.

Release/fallback/native-off configuration:

```sh
cmake -S on1x -B /tmp/opencode/on1x-values-release -DBUILD_TESTING=ON \
  -DON1X_USE_FALLBACK_SCANNER=ON -DON1X_BUILD_NATIVE_PPX=OFF \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  '-DCMAKE_C_FLAGS=-Wall -Wextra -Wpedantic -Werror' \
  '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic -Werror'
cmake --build /tmp/opencode/on1x-values-release --parallel 2 --target \
  on1xc on1x_nominal_tests on1x_control_tests on1x_pattern_tests on1x_value_loop_tests
ctest --test-dir /tmp/opencode/on1x-values-release --output-on-failure \
  --parallel 2 -R 'on1x.(nominal|control|pattern|value_loop)' \
  --output-junit /tmp/opencode/on1x-values-release/results-nominal-adts-final.xml
```

| Final profile | Result |
|---|---|
| GCC/generated Debug, native processors | 49/49 |
| Clang/fallback Debug, native processors | 48/48 |
| Clang/generated Debug + ExoML/Exotract/Chisel/Nerotypical/Novostone | 58/58 |
| Same SDK + ASan/UBSan, including generated C/C++ programs | 58/58, no sanitizer findings |
| GCC Release/fallback/native-off, focused control/matching/value/nominal suites | 8/8 |

JUnit artifacts are `results-nominal-adts-final.xml` in those five trees.

Standalone acceptance:

```sh
python3 on1x/tools/on1x-build.py on1x/examples/nominal_adts.on1x \
  --on1xc /tmp/opencode/on1x-values-gcc/on1xc --cc gcc \
  -o /tmp/opencode/on1x-nominal-adts-example
/tmp/opencode/on1x-nominal-adts-example
readelf -d /tmp/opencode/on1x-nominal-adts-example
```

Output: `42`, `true`, `-7`. The only dynamic dependency is `libc.so.6`.
Strict GCC C11 static-archive and shared-runtime links of that generated program
also execute with the same output:

```sh
/tmp/opencode/on1x-values-gcc/on1xc --emit-c on1x/examples/nominal_adts.on1x \
  -o /tmp/opencode/on1x-nominal-link.c
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -fPIC -shared -I on1x/runtime \
  on1x/runtime/on1x_runtime.c -o /tmp/opencode/libon1x-nominal-runtime.so
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -I on1x/runtime \
  /tmp/opencode/on1x-nominal-link.c -L /tmp/opencode -lon1x-nominal-runtime \
  -Wl,-rpath,/tmp/opencode -o /tmp/opencode/on1x-nominal-shared
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -I on1x/runtime \
  /tmp/opencode/on1x-nominal-link.c /tmp/opencode/on1x-values-gcc/libon1x_runtime.a \
  -o /tmp/opencode/on1x-nominal-static
/tmp/opencode/on1x-nominal-shared
/tmp/opencode/on1x-nominal-static
```

Direct SDK boundary checks with `on1x-values-sdk/on1xc --check-memory` and
`--check-types` on that source reject with `ON1X-MEMORY: parameter memory model
unavailable` and `ON1X-TYPE-SAFETY: unresolved or quantified type is outside the
structural adapter`. MIR emission/evaluation of aggregates remains unsupported.

Historical attempts: strict compiler warnings caught two indentation issues and
a parser-local name collision; new fixture setup initially used an omitted
lambda return annotation and an invalid MIR CLI spelling. The first complete
Debug runs then caught a changed legacy syntax diagnostic, null-child traversal
ahead of canonical validation, and Python named-signature rendering. Those were
fixed in the implementation, not by disabling/relabeling tests. The cross-TU
fixture was also corrected to keep generated private adapters out of its shared
interface. An added deep-layout regression exposed a shallow-cache depth bypass;
cached height is now checked. Earlier failed logs remain `results-nominal-adts.xml`.
Final compatibility review also added ordinary `record`/`variant` comparison
bindings: complete-prefix lookahead avoids greedily reinterpreting their `<`
operators as constructor syntax. All five final profiles were rebuilt/rerun
after that parser regression fix (`build-nominal-compat.log` in each tree).

Unsupported: recursive/owning nominal layouts, managed fields, field mutation,
constructor-function sugar, general aggregate MIR/SDK models, foreign aggregate
ABI, module interfaces/separate compilation, general resource transfer/errors,
owning lists, defer/using/try/propagation and checked concurrency. No 32-bit,
non-Linux or alternative-native-backend certification. TSAN is not evidence for
this single-threaded by-value increment; Exotract's optional Lua layer remains
unavailable, while its real native CLI is exercised in SDK suites.

## G2 continuation — value-producing loops

Scoped task: the next executable dependency slice of the request for value loops,
owning lists, defer/typed errors, concurrency and nominal ADTs. **Only value loops
are implemented by this slice.** The other capabilities remain unsupported; no
syntax-only runtime, competing IR or guessed foreign ABI was added.

Re-inspected MuLIB `7391d9e9d58e987ea234c64dbfd352c31f220537` and the configured,
unchanged/read-only LibStone `55755485acaded7c4ed804c0946bc8c7363f6763` at
`on1x/third_party/libstone`. Linux `6.18.52-1-lts` x86_64, glibc 2.44,
GCC/g++ 16.2.1, Clang/clang++ 22.1.8, CMake 4.4.3, Python 3.14.7.
The shared tree also contains concurrent C-string FFI work, which was preserved;
these combined-tree suites include that work where its SDK is enabled. Its
separate isolated evidence remains below. No commit, push or submodule update.

### Behavior, regressions and changed areas

- `loop` expressions and labeled `break with` infer a common exact result type;
  a bare break yields void. Only expression-loop targets admit payloads. Existing
  statement-loop/while/for behavior and ordinary keyword-spelled bindings remain
  compatible. Parenthesize a loop expression to discard its result.
- Operands execute once before lexical cleanup. Scalar/tuple/closure and bounded
  by-value C11 results reuse existing representation and RC. JSON owners/views
  cannot be transferred/discarded as value exits. Fatal failures do not unwind.
- Control normalization resolves function-local expression targets, and the
  independent verifier checks the nearest matching label/syntax context rather
  than accepting any active target ID; value lowering
  erases loops/break operands into existing while/region/initialize/leave nodes.
  Independent possible/definite-initialization checks reject missing/repeated
  writes and writes on repeatable backedges, including condition expressions.
  A fatal impossible-normal-exit edge prevents uninitialized result reads.
- Additive v1 PPX nodes `LoopValue`/`ValueBreak` carry source names/contexts,
  never canonical targets or result/ownership proofs. Both JSON schemas are
  semantically synchronized (their historical formatting/order differs).
- Files: `include/on1x/{ast,clone}.hpp`, `src/{parser,infer,sema,loader,macros,
  macro_rules,ppx_ast,staging}.cpp`, `src/canonical/{control,values,verify_values}.cpp`,
  source-node rejection in `src/{codegen,safety,type_safety}.cpp`, grammar/schema
  inputs, `CMakeLists.txt`, `tests/test_value_loops.{cpp,py}`, updated statement
  control negatives, `examples/value_loops.on1x` and linked manuals/matrix.
  Existing MIR lowering/runtime require no new opcode, format or ABI.
- The example initially failed on the labeled expression (`expected ';', found
  ':'`). It now compiles/links/runs and prints `49`, `42`, `8` through C11 and
  scalar/numeric MIR. The standalone C11 executable needs only `libc.so.6`
  (`readelf -d /tmp/opencode/on1x-value-loops-example`).
- New tests cover plain/optimized MIR C11 and evaluator, deterministic MIR
  round-trip, strict C11/O2, ten seeded search oracles, nested/outer exits and
  for-latch behavior, exact numeric joins, bounded aggregate/closure results,
  source return from payloads, macro hygiene, native/Python identity PPX, staging,
  generics/imports, separate C translation-unit calls, JSON cleanup, closure
  counting and allocation failure at three points, terminal division failure,
  malformed HIR/wire, initialization mutations and output preservation.

### Exact commands and profiles

From the MuLIB root (SDK preparation copies the actual pinned headers/allocator
to a separate source SDK and applies the existing explicit CMake-only patch):

```sh
cmake -S on1x -B /tmp/opencode/on1x-values-gcc -DBUILD_TESTING=ON \
  -DON1X_REQUIRE_LIBSTONE=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  '-DCMAKE_C_FLAGS=-Wall -Wextra -Wpedantic -Werror' \
  '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic -Werror'
cmake -S on1x -B /tmp/opencode/on1x-values-clang -DBUILD_TESTING=ON \
  -DON1X_USE_FALLBACK_SCANNER=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  '-DCMAKE_C_FLAGS=-Wall -Wextra -Wpedantic -Werror' \
  '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic -Werror'
python3 on1x/tools/prepare-type-sdk.py --output /tmp/opencode/on1x-values-type-sdk
cmake -S on1x -B /tmp/opencode/on1x-values-sdk -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DON1X_REQUIRE_LIBSTONE=ON -DON1X_WITH_EXOML=ON -DON1X_WITH_EXOML_EXTRACT=ON \
  -DON1X_WITH_EXOTRACT=ON -DON1X_WITH_CHISEL=ON -DON1X_WITH_NEROTYPICAL=ON \
  -DON1X_WITH_NOVOSTONE=ON -DON1X_SATIE_ROOT=/tmp/opencode/on1x-values-type-sdk \
  '-DCMAKE_C_FLAGS=-Wall -Wextra -Wpedantic -Werror' \
  '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic -Werror'
cmake -S on1x -B /tmp/opencode/on1x-values-sanitize -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DON1X_REQUIRE_LIBSTONE=ON -DON1X_WITH_EXOML=ON -DON1X_WITH_EXOML_EXTRACT=ON \
  -DON1X_WITH_EXOTRACT=ON -DON1X_WITH_CHISEL=ON -DON1X_WITH_NEROTYPICAL=ON \
  -DON1X_WITH_NOVOSTONE=ON -DON1X_SATIE_ROOT=/tmp/opencode/on1x-values-type-sdk \
  '-DCMAKE_C_FLAGS=-Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined' \
  '-DON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer'
```

For each of those four trees, build first, then run tests without overlapping a
relink or another CTest invocation in that same tree:

```sh
cmake --build <tree> --parallel 2
ctest --test-dir <tree> --output-on-failure --parallel 2 \
  --output-junit <tree>/results-value-loops.xml
```

| Profile | Result |
|---|---|
| Clang/fallback pre-change baseline | 44/44 |
| GCC/generated Debug, native processors | 47/47 |
| Clang/fallback Debug, native processors | 46/46, including clean final rerun |
| Clang/generated Debug + ExoML/Exotract/Chisel/Nerotypical/Novostone | 56/56 |
| Same SDK + ASan/UBSan, including generated-program test flags | 56/56, no sanitizer findings |
| Focused control/matching/value-loop suites, GCC Debug and SDK Debug | 6/6 each, including final wire/header/backedge mutations |
| GCC Release/fallback, native processors disabled | Focused 6/6 |

Final complete-build reruns in all four main trees used the same command with
`--output-junit <tree>/results-value-loops-final.xml` (after the final nearest-target
verification regression). An additional clean Clang XML is
`results-value-loops-clean.xml`; focused results are
`results-value-loops-focused.xml` in GCC/SDK trees. GCC Release used the same
strict flags with `-DCMAKE_BUILD_TYPE=Release -DON1X_USE_FALLBACK_SCANNER=ON
-DON1X_BUILD_NATIVE_PPX=OFF`, built `on1xc on1x_control_tests on1x_pattern_tests
on1x_value_loop_tests`, then `ctest -R 'on1x.(control|pattern|value_loop)'` with
the same output/parallel/JUnit options (final Release XML:
`results-value-loops-final.xml`).

Final acceptance commands also ran the build helper twice using
`/tmp/opencode/on1x-values-sdk/on1xc`: default HIR→C11 to
`/tmp/opencode/on1x-value-loops-final` and `--via-mir --optimize-ir` to
`/tmp/opencode/on1x-value-loops-mir-final`, both from
`on1x/examples/value_loops.on1x`. Python syntax checks, semantic JSON-schema
equality and `git diff --check` passed. LibStone stayed clean.

Initial historical build directories were absent. An attempted reused GCC tree
had missing artifacts and existing FFI-tool indentation warnings; it was not a
green baseline. An intermediate Clang run overlapped a relink and reported four
`PermissionError` process-launch failures; it was superseded by completed-build
reruns. No tests were disabled or their assertions relaxed to obtain green runs.

Limits: existing 2 MiB source/parser limits and bounded canonical traversal/
flow-state budgets remain. No general bottom type, loop-exhaustion `else`, unique
resource transfer, owning list, nominal ADT, defer, recoverable-error unwind or
concurrency is admitted. Aggregates/managed results still reject scalar MIR and
unsupported SDK models. No new ABI or native backend; no other-platform/32-bit,
TSAN or repository-wide MuLIB conformance claim. TSAN is not relevant evidence
for this single-threaded control increment. Exotract's optional Lua/Kaguya layer
is unavailable; its actual native CLI/typed discovery was exercised.

## M2 continuation — immutable C-string admission

Baseline MuLIB `7391d9e9d58e987ea234c64dbfd352c31f220537`, actual configured
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763` (unchanged). Platform for
this run: Linux `6.18.52-1-lts` x86_64, glibc 2.44; GCC/g++ 16.2.1,
Clang/clang++ 22.1.8, CMake 4.4.3, Python 3.14.7.

Scope: extend existing reviewed scalar FFI with exact immutable `const char *`
call borrows and immutable process-lifetime results; directional v4 evidence,
ABI assertions, null/byte-limit checks, unchanged review/Chisel/target gates.
No production contract review status is changed. General bridge safety remains
**PARTIAL**, not VERIFIED; see [the profile contract](docs/ffi-strings.md).

### Reproduction and isolation

The strict original-tree baseline first failed on three existing misleading
indentation warnings in `tools/on1x-bind.cpp`; these are fixed. After the fixes,
the existing generated/GCC suite passed **49/49**, excluding the new test. The
new `on1x.contract_strings` test failed before implementation because string
parameters/static borrowed results were outside the scalar-only profile.

Concurrent value-loop edits then appeared in the shared source tree and
temporarily introduced a duplicate `Stmt::control_target` field. Those changes
were not overwritten. Final FFI-slice validation uses a detached HEAD worktree
at `/tmp/opencode/on1x-cstring-source`, with only this slice's compiler/build/test
changes copied in. The baseline canonical/control/parser files remain unchanged
in that validation snapshot. This evidence does **not** certify the concurrent
value-loop changes or the combined shared tree.

Exact configuration/build commands (from the repository root):

```sh
git worktree add --detach /tmp/opencode/on1x-cstring-source HEAD
# Apply this slice's changes to the snapshot, excluding concurrent value-loop work.
cmake -S /tmp/opencode/on1x-cstring-source/on1x \
  -B /tmp/opencode/on1x-cstring-isolated-gcc \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DON1X_LIBSTONE_ROOT=/mnt/nvme/mulib/on1x/third_party/libstone \
  -DON1X_WITH_CHISEL=ON -DON1X_WITH_EXOML=ON -DON1X_WITH_EXOML_EXTRACT=ON
cmake --build /tmp/opencode/on1x-cstring-isolated-gcc --parallel 4
ctest --test-dir /tmp/opencode/on1x-cstring-isolated-gcc \
  --output-on-failure --parallel 4 --output-junit results-cstrings.xml

cmake -S /tmp/opencode/on1x-cstring-source/on1x \
  -B /tmp/opencode/on1x-cstring-isolated-clang \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DON1X_LIBSTONE_ROOT=/mnt/nvme/mulib/on1x/third_party/libstone \
  -DON1X_USE_FALLBACK_SCANNER=ON \
  -DON1X_WITH_CHISEL=ON -DON1X_WITH_EXOML=ON -DON1X_WITH_EXOML_EXTRACT=ON
cmake --build /tmp/opencode/on1x-cstring-isolated-clang --parallel 4
ctest --test-dir /tmp/opencode/on1x-cstring-isolated-clang \
  --output-on-failure --parallel 4 --output-junit results-cstrings.xml

cmake -S /tmp/opencode/on1x-cstring-source/on1x \
  -B /tmp/opencode/on1x-cstring-isolated-sanitize \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DON1X_LIBSTONE_ROOT=/mnt/nvme/mulib/on1x/third_party/libstone \
  -DON1X_WITH_CHISEL=ON -DON1X_WITH_EXOML=ON -DON1X_WITH_EXOML_EXTRACT=ON
cmake --build /tmp/opencode/on1x-cstring-isolated-sanitize --parallel 4
ctest --test-dir /tmp/opencode/on1x-cstring-isolated-sanitize \
  --output-on-failure --parallel 4 --output-junit results-cstrings.xml
```

### Evidence and limits

- GCC/generated: **50/50 passed**; standalone `contract_strings` rerun also
  passed after moving the source program into `tests/fixtures/contract_strings.on1x`.
- Clang/fallback: **49/49 passed**. Clang/generated ASan/UBSan: **50/50 passed**.
  All three configurations also passed the final extended string regression
  independently (pointer-layout drift, multiline gate observations, checked build
  helper and deterministic MIR rejection included). Full-suite JUnit evidence is
  `results-cstrings.xml` in each isolated build directory.
  After the last diagnostic wording change and test additions, the focused
  `contracts_sdk`, `contract_strings`, `safety_hir` suites passed **3/3** in
  each configuration, recorded in `results-cstrings-final.xml`:

  ```sh
  ctest --test-dir /tmp/opencode/on1x-cstring-isolated-gcc -R '^on1x.(contract_strings|contracts_sdk|safety_hir)$' --output-on-failure --parallel 2 --output-junit results-cstrings-final.xml
  ctest --test-dir /tmp/opencode/on1x-cstring-isolated-clang -R '^on1x.(contract_strings|contracts_sdk|safety_hir)$' --output-on-failure --parallel 2 --output-junit results-cstrings-final.xml
  ctest --test-dir /tmp/opencode/on1x-cstring-isolated-sanitize -R '^on1x.(contract_strings|contracts_sdk|safety_hir)$' --output-on-failure --parallel 2 --output-junit results-cstrings-final.xml
  ```
- The string test exercises real libclang extraction, generated On1x facade,
  Chisel use-site checks, strict C11 compilation, C/C++ separate-TU execution at
  `-O0`/`-O2`, seeded Python byte-length comparisons, stable old results and nested
  calls. Negative tests cover unreviewed metadata, borrowed/owned/nullability
  errors, retained lifetimes, unknown/escape/allocation/invalidation effects,
  source type errors, pointer signature drift, legacy/directional evidence,
  null/oversized results, null inputs, exact/over-limit inputs, and nonpublication
  after gate failure. Synthetic review flags are test inputs, not certification.
- ASan/UBSan configuration instruments compiler, public SDK code built here,
  runtime, native processors, adapters and generated C/C++ consumers. The system
  libclang library is not instrumented. No new runtime allocations are introduced
  by this profile. TSAN is not applicable to this single-threaded slice.
- Other optional integrations, 32-bit execution, other targets/platforms, owned
  buffers/resources, recoverable foreign errors, callbacks, async suspension,
  MIR foreign calls, general effect proofs and independent standard-bridge review
  are **not exercised or admitted** by this slice. Existing scalar v1/v2/v3
  regressions remain green; no direct machine-code backend is claimed.

## Intrinsic G2 continuation — guarded structural matching and typed decisions

Scoped task: implement generalized matching over the **existing bounded value
model**, not all of `ON1X_CORE_SYNTAX_ADDITIONS.md`. Re-inspected MuLIB
`9090e643e11051e000b3bcc68ebc777dfe07776a` and configured read-only LibStone
`55755485acaded7c4ed804c0946bc8c7363f6763` at `on1x/third_party/libstone`.
The submodule is clean. Previous uncommitted Category/loop work and the user's
untracked specification remain intact; no commit, push or unrelated kernel work.

Platform remains Linux `7.2.6-arch2-1` x86_64, glibc 2.44; GCC/g++ 16.2.1,
Clang/clang++ 22.1.8, CMake 4.4.3 and Python 3.14.7. C11 is still the reference
AOT backend, not direct machine-code generation.

### Behavior and exact implementation boundary

- `match expr with { ... }` statements/expressions and restricted `match let`
  discriminants; guards, nested tuples/closed sums, exact arrays/snapshot slices,
  record fields/rest, scalar/string literals, wildcard/final catch-all, or/as
  patterns and arm blocks with final unterminated expression results.
- Shared patterns in `include/on1x/ast.hpp`, canonical parser/inference/sema,
  finite-constructor coverage/redundancy in `src/patterns.cpp`. Match names and
  arms retain loader, hygienic macro, PPX and staging scope/identity behavior.
  Existing name/tuple let/parameter patterns and legacy payload-lambda matches
  remain compatible; old `_` bindings are not silently changed to wildcards.
- `src/canonical/values.cpp` transactionally replaces both match spellings with
  typed HIR decision regions, branches/projections, single-initialization storage
  and lexical leave edges. Guards/bodies are shared across or-alternatives, not
  duplicated effects. `verify_values.cpp` checks initialized reads/joins, repeat
  initialization, tag domination, projection/result types, completion, source
  points and private node shapes. No second parser, runtime interpreter or MIR.
- Direct C11 cleanup handles managed results, arm-local owners/loans and outer
  return/break/continue, including abandoned earlier operands. Scalar/numeric
  tuple cases use the existing MIR slots/extracts/branches/jumps, reference
  evaluator, serialization and transactional passes without a format change.
- Additive source wire nodes are `SourceMatch`, `MatchArm`, `StructuralPattern`,
  `UnitValue`. PPX cannot inject canonical regions, storage facts, payload proofs
  or resolved targets. Canonical/native JSON schemas are equal. Generated scanner
  files remain build artifacts; Aurocks/fallback recognize `|` and `::` identically.
- Actual Chisel CFG/Nerotypical constraints support the admitted scalar/tuple
  decision profile and tested arm-local JSON cleanup. Fatal decision fallbacks
  have non-returning CFG sinks, not fictitious normal drops. Sum/record/array/
  slice and managed/foreign profiles still reject unsupported SDK/MIR requests.

Budgets: 256 arms/width, 4,096 expanded alternatives, 262,144 pattern-analysis and
canonical traversal/copy work units, 8 MiB copied text per analysis/planning
phase, 256 syntax-pattern levels, 520 decision/traversal levels and 4,194,304
flow-state work units. Exhaustion rejects without output/coverage approximation.

### Reproductions and tests

`tests/test_patterns.py` compiles actual sources with strict
`-std=c11 -Wall -Wextra -Wpedantic -Werror`, separately compiling the real runtime
and allocator/foreign/JSON fixtures. It tests ordered/guarded matching, scrutinee
once, guard once for overlapping alternatives, block scopes/results, or-binder
equivalence, nested sums/empty payloads, record omission, exact array/slice lengths,
numeric/string patterns and signed-zero redundancy. Scalar/tuple fixtures compare
HIR→C11, MIR reference execution, unoptimized/optimized MIR→C11 and serialized
round-trips against six seeded independent Python oracles.

Other cases cover source imports, global specialization, macro/staging hygiene,
Python/native identity PPX, original keyword-spelled values, closure allocation
counts and allocation-failure injection, managed results and JSON return/loop
cleanup. Guard division failure preserves the preceding trace and status 70.
Negative programs cover non-exhaustive/redundant arms, guard/result types, rest
placement, nonlinear and incompatible or-binders, arities/extents/fields/tags,
scope escape, unsupported owning-list/unique-resource profiles, depth/expansion
budgets and preservation of existing output on failure.

`tests/test_patterns.cpp` checks source-wire round-trips, canonical erasure and
idempotence, transactional rejection of unchecked source, missing/repeated
initialization, projection corruption, missing tag domination, completion/target
metadata, null children/values and private/refutable wire-node rejection.

The new nested-match fixture initially reproduced a MIR infinite loop: continue
discovery skipped expression-region bodies, so for-C continued at its header
without executing the latch. MIR and Chisel now collect target exits through
expression bodies, and the regression executes with its expected `2`/`4` trace.
The first SDK suite also exposed a diagnostic regression: the new value verifier
intercepted ordinary post-check type corruptions before Nerotypical. Its extra
gate now runs after the real solver and before successful admission, preserving
the unchanged independent-solver mutation/budget/oracle tests. No test was
disabled or relaxed to bypass either regression. The scanner invalid-byte corpus
was updated because `|` is now deliberately valid, and expanded with valid `|`/`::`
and matching syntax rather than dropping parity checks.

### Commands and results

The four existing configured trees retain the documented scanner/SDK settings.
The sanitizer tree instruments C/C++ with
`-fsanitize=address,undefined -fno-omit-frame-pointer` **and** has
`ON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer`; generated
applications, runtime and extra fixture TUs are instrumented, not only on1xc.

```sh
cmake --build /tmp/opencode/on1x-loader-gcc --parallel 2
ctest --test-dir /tmp/opencode/on1x-loader-gcc --output-on-failure --parallel 2 \
  --output-junit /tmp/opencode/on1x-loader-gcc/results-core-match.xml
cmake --build /tmp/opencode/on1x-loader-clang --parallel 2
ctest --test-dir /tmp/opencode/on1x-loader-clang --output-on-failure --parallel 2 \
  --output-junit /tmp/opencode/on1x-loader-clang/results-core-match.xml
cmake --build /tmp/opencode/on1x-data-sdk --parallel 2
ctest --test-dir /tmp/opencode/on1x-data-sdk --output-on-failure --parallel 2 \
  --output-junit /tmp/opencode/on1x-data-sdk/results-core-match.xml
cmake --build /tmp/opencode/on1x-safe-ffi-sanitize --parallel 2
ctest --test-dir /tmp/opencode/on1x-safe-ffi-sanitize --output-on-failure --parallel 2 \
  --output-junit /tmp/opencode/on1x-safe-ffi-sanitize/results-core-match.xml
```

Final runs: GCC/generated **45/45**, Clang/fallback **44/44**, optional SDK
**53/53**, SDK ASan/UBSan **53/53**, with no sanitizer findings. These are full
reruns after the solver-diagnostic fix; the earlier failed SDK runs are not
counted as passes.

Focused native-off Debug/Release configurations were refreshed without changing
their cached compiler/build settings (Clang Debug and GCC Release, both fallback):

```sh
cmake -S on1x -B /tmp/opencode/on1x-core-debug
cmake --build /tmp/opencode/on1x-core-debug --target on1xc on1x_control_tests on1x_pattern_tests --parallel 2
ctest --test-dir /tmp/opencode/on1x-core-debug -R 'on1x.(control|pattern)' --output-on-failure \
  --output-junit /tmp/opencode/on1x-core-debug/results-core-match.xml
cmake -S on1x -B /tmp/opencode/on1x-core-release
cmake --build /tmp/opencode/on1x-core-release --target on1xc on1x_control_tests on1x_pattern_tests --parallel 2
ctest --test-dir /tmp/opencode/on1x-core-release -R 'on1x.(control|pattern)' --output-on-failure \
  --output-junit /tmp/opencode/on1x-core-release/results-core-match.xml
python3 on1x/tools/on1x-build.py on1x/examples/structural_match.on1x \
  --on1xc /tmp/opencode/on1x-core-release/on1xc --cc gcc \
  -o /tmp/opencode/on1x-core-match-acceptance
/tmp/opencode/on1x-core-match-acceptance
readelf -d /tmp/opencode/on1x-core-match-acceptance
```

Both focused suites passed **4/4**. Native identity execution is intentionally
absent from these native-off profiles, not counted as native PPX evidence.
The acceptance executable prints `42`, `8`, `4` and needs only `libc.so.6`.
Python syntax parsing, canonical/native schema equality and `git diff --check`
pass. The unrelated repository-wide `tools/check.py` failures recorded below
were not rerun or claimed green.

### Not implemented / not certified

This advances the G2 **bounded structural-match** capability to EXECUTABLE, not
full generalized data/concurrency conformance. Owning lists/list-cons, refutable
let/parameter bindings, loop values, general iterator/drop protocols, defer/using,
recoverable errors, concurrency/processes, async/generators, synchronization,
nominal/parametric/recursive ADTs and contracts still need vertical implementations.
Aggregate/managed/foreign/concurrency MIR and broader independent SDK models
remain closed. No general bottom type or cleanup on terminal traps is promised.
No new thread facility was added; TSAN and non-Linux/cross-target execution were
not run for this slice. General safe FFI and complete Category remain incomplete.

Files changed by this matching continuation (prior Category/loop edits elsewhere
are preserved, not attributed to this slice):

- `CMakeLists.txt`, `grammar/{On1x.g,PPX.schema.json}`,
  `stdppx/schemas/PPX.schema.json`.
- `include/on1x/{ast,clone,patterns}.hpp`, `include/on1x/canonical/values.hpp`.
- `src/{parser,lexer_fallback,loader,infer,sema,macros,macro_rules,ppx_ast,staging,codegen,safety,type_safety,patterns}.cpp`,
  `src/canonical/{control,values,verify_values}.cpp`, `src/backend/lower.cpp`.
- `tests/{test_patterns.cpp,test_patterns.py,scanner_parity.py}`,
  `examples/structural_match.on1x`.
- `README.md`, `BACKEND.md`, `COMPLETION.md`, `VERIFICATION.md`,
  `docs/{architecture,backend-mir,capability-matrix,core-syntax,data-model,language,memory-safety,metaprogramming,roadmap,type-safety}.md`.

## Intrinsic core-syntax G1/G2 — statement loops, ranges and lexical exits

This implements a runnable **statement-control slice**, not all of
`ON1X_CORE_SYNTAX_ADDITIONS.md`. Re-inspected MuLIB
`9090e643e11051e000b3bcc68ebc777dfe07776a`, configured read-only LibStone
`55755485acaded7c4ed804c0946bc8c7363f6763` at `on1x/third_party/libstone`.
Existing uncommitted Category changes and the user's specification were preserved;
no commit/push or unrelated kernel/inventory changes were made.

Platform: Linux `7.2.6-arch2-1` x86_64, glibc 2.44. GCC/g++ 16.2.1
(`20260810`), Clang/clang++ 22.1.8, CMake 4.4.3, Python 3.14.7.

### Implemented boundary

- Canonical parser and both scanners now admit statement `loop`, for-C, ascending
  i64 `..`/`..=` ranges, loop labels and break/continue. Existing `...`, ordinary
  keyword-spelled identifiers, macro hygiene, source locations and lexical scopes
  are retained. Source grammar is summarized in `grammar/On1x.g`; the existing
  token-arena ASDL/C++ source-tree bridge is extended rather than replaced.
- `src/canonical/control.cpp` lowers source ranges into checked i64/bool
  preheaders, conditions, bodies and latches; evaluates bounds once left-to-right;
  avoids inclusive MAX+1; and recomputes lexical identities after specialization.
  The independent verifier rejects absent/duplicate/nonlexical/cross-function
  targets, malformed exits, invalid source points and surviving surface ranges.
  Target IDs cannot be supplied by PPX. Normalization is bounded/transactional.
- The additive PPX-v1 `ControlWhile`, `RangeLoop`, `LoopExit` constructors carry
  source syntax only. Both schemas, reconstruction, macro substitution, loader
  scopes and staging were updated. Ordinary post-expansion type/ownership checks
  remain required. The native Category processor remains a separate feature.
- C11 emits explicit jumps and existing lexical ownership cleanup for precisely
  the exited scopes. Scalar MIR uses existing verified Jump/Branch CFG edges;
  its serialization versions, evaluator and transactional optimizer are unchanged.
  Optional Nerotypical validates generated HIR; Chisel models real cleanup/jump
  edges, using the pinned SDK's actual `c::Id`/Function/block API. No new runtime
  library, scheduler, safe-FFI permission or native machine-code backend is claimed.

### Reproductions and executable coverage

`tests/test_control.py` compiles actual On1x programs to strict ISO C11 with
`-Wall -Wextra -Wpedantic -Werror`, separately compiling the existing runtime and
applicable allocator/kernel fixtures. The same scalar programs execute through
MIR reference evaluation, deterministic serialization round-trips and optimized
MIR→C11. The example additionally runs with C `-O2`. Tests include:

- Empty/reversed/inclusive/extreme ranges; left-to-right bound effects; snapshot
  bounds despite later mutation; immutable iteration bindings; shadowing and scope.
- For-C initializer/condition/step behavior, empty clauses, nested labeled exits,
  exactly-once continue step and abandoned-step suppression; checked step overflow
  agrees with MIR failure output/status. Eight deterministic seeded numeric oracles.
- Existing identifiers/calls/discard statements, including an imported module;
  macro-introduced label hygiene, captured exits and identity PPX (Python/native);
  staging and distinct i64/bool generic specializations. Contextual labels named
  `with` work for both exits; `break with value` still rejects. No Python transform
  implements loop semantics.
- Closure/tuple reference accounting over 100 iterations and nested return/break/
  continue, using the existing allocation-count fixture. Real MuLIB JSON owner/loan
  exits compile/run; SDK builds admit these same edges through Chisel.
- Negative types, immutable assignment, loop/binder/initializer escape, duplicate
  labels, missing/cross-lambda targets, implicit template capture, unreachable
  statements, malformed headers/depth, unsupported value loops/general iteration
  and output preservation after errors.

`tests/test_control.cpp` mutates actual checked HIR: missing/duplicate/wrong-world
targets, an exit into its own latch, invalid provenance, null bounds and surviving
source ranges fail closed. It checks emitter/MIR rejection, source-wire round-trip,
invalid wire operations and no partial publication after normalization failure.
Scanner tests assert exact longest-match tokens; parity includes all examples,
new lexical edge cases and the existing malformed/randomized corpus.

The first regression run exposed three integration mistakes: a too-small combined
traversal depth rejected a previously valid 255-term lambda tree; unconditional
cloning invalidated checked-HIR references on recheck; and lowering staging before
deciding whether to infer changed an established run-type diagnostic. They were
corrected without changing the baseline tests or disabling checks. The existing
limits, closure-HIR idempotence and staging tests reproduce/protect these cases.

### Exact commands and results

The existing four configured trees retain generated/fallback scanner and optional
SDK coverage. The sanitizer tree sets both C/C++ flags to
`-fsanitize=address,undefined -fno-omit-frame-pointer`, **and** explicitly sets
`ON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer` so generated
applications, runtime and extra fixtures are instrumented too.

```sh
cmake --build /tmp/opencode/on1x-loader-gcc --parallel 2
ctest --test-dir /tmp/opencode/on1x-loader-gcc --output-on-failure --parallel 2 \
  --output-junit /tmp/opencode/on1x-loader-gcc/results-core-loops.xml
cmake --build /tmp/opencode/on1x-loader-clang --parallel 2
ctest --test-dir /tmp/opencode/on1x-loader-clang --output-on-failure --parallel 2 \
  --output-junit /tmp/opencode/on1x-loader-clang/results-core-loops.xml
cmake --build /tmp/opencode/on1x-data-sdk --parallel 2
ctest --test-dir /tmp/opencode/on1x-data-sdk --output-on-failure --parallel 2 \
  --output-junit /tmp/opencode/on1x-data-sdk/results-core-loops.xml
cmake --build /tmp/opencode/on1x-safe-ffi-sanitize --parallel 2
ctest --test-dir /tmp/opencode/on1x-safe-ffi-sanitize --output-on-failure --parallel 2 \
  --output-junit /tmp/opencode/on1x-safe-ffi-sanitize/results-core-loops.xml
```

Final runs passed GCC/generated **43/43**, Clang/fallback **42/42**, SDK **51/51**,
and SDK ASan/UBSan **51/51**, with no sanitizer findings. These include the final
provenance/null-bound/import regressions, contextual `with` labels and distinct
generic specializations. Changed Python syntax/JSON schema checks and
`git diff --check` also pass.

Independent named build profiles (focused suite only, no native PPX execution):

```sh
cmake -S on1x -B /tmp/opencode/on1x-core-release -DBUILD_TESTING=ON \
  -DON1X_BUILD_NATIVE_PPX=OFF -DON1X_USE_FALLBACK_SCANNER=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build /tmp/opencode/on1x-core-release --target on1xc on1x_control_tests --parallel 2
ctest --test-dir /tmp/opencode/on1x-core-release -R 'on1x.control' --output-on-failure \
  --output-junit /tmp/opencode/on1x-core-release/results-core-loops.xml
cmake -S on1x -B /tmp/opencode/on1x-core-debug -DBUILD_TESTING=ON \
  -DON1X_BUILD_NATIVE_PPX=OFF -DON1X_USE_FALLBACK_SCANNER=ON \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build /tmp/opencode/on1x-core-debug --target on1xc on1x_control_tests --parallel 2
ctest --test-dir /tmp/opencode/on1x-core-debug -R 'on1x.control' --output-on-failure \
  --output-junit /tmp/opencode/on1x-core-debug/results-core-loops.xml
```

Both passed **2/2**; the native-identity subcase is unsupported/not run in these
explicitly native-off configurations, not counted as native PPX evidence.

```sh
python3 on1x/tools/on1x-build.py on1x/examples/statement_loops.on1x \
  --on1xc /tmp/opencode/on1x-core-release/on1xc --cc gcc --via-mir --optimize-ir \
  -o /tmp/opencode/on1x-core-loops-acceptance
/tmp/opencode/on1x-core-loops-acceptance
readelf -d /tmp/opencode/on1x-core-loops-acceptance
```

Output: `8`, `3`, `9223372036854775807`, each on its own line. The sole NEEDED
library is `libc.so.6`. No runtime compiler/LibStone/Python dependency is added.

### Unsupported / not promoted

The complete specification is **not implemented**. General iterator/drop
protocols, unsigned/float/custom-step ranges, loop values, generalized pattern
matching, owning lists/comprehensions, defer/using, typed errors, fibers/channels/
poll/taskgroups, process literals, async/generators, synchronization, nominal
recursive ADTs and native contracts remain closed. Conservative return analysis
requires a returning tail in nonvoid functions even after an endless loop.
The canonical bridge is not full cleanup/effect/suspension Core. Resource-bearing
loops still reject scalar MIR. No threads were added; TSAN is not applicable.
Only Linux x86_64 execution was measured. See `docs/core-syntax.md` for the exact
per-feature status/migration limits. Historical repository-wide checker failures
recorded below remain outside this task; that suite was not rerun or claimed green.

## Category M0/M1 — bundled native activation and explicit local scalar protocols

Implemented a real first slice of `CATEGORY.md`, **not** the complete Category
system. Baseline re-inspected at MuLIB
`9090e643e11051e000b3bcc68ebc777dfe07776a`, with LibStone unchanged at
`55755485acaded7c4ed804c0946bc8c7363f6763`. Linux `7.2.6-arch2-1` x86_64,
glibc 2.44, GCC/g++ 16.2.1, Clang/clang++ 22.1.8, CMake 4.4.3, Python 3.14.7.
The working tree was clean before this task; no commit/push was made.

### Implementation / trust boundary

- `stdppx/native/protocols.on1x` is bootstrapped to a native C11 executable, using
  `stdlib/ppx/category/{syntax,witness}.on1x` and shared native token, diagnostic,
  source/fresh-name, schema, S-expression and visitor libraries. Protocol parsing,
  scalar `Self` substitution, local canonical-head checks, named alternatives,
  reporting identities and selector generation are On1x code, not Python/C++.
- Generic compiler changes are in `src/{extensions,parser,loader,cli,ppx}.cpp`
  and AST/clone/loader headers. They implement `@enable`, `--enable/--disable`,
  registry introspection, bounded balanced carriers, module provenance, pinned
  native execution, ordinary-function reconstruction and immutable generated
  interfaces across later ordinary PPXs. No protocol/member names are special
  typechecker intrinsics. Normal post-expansion typing/ownership/checking remains
  authoritative; reporting facts are not proof tokens.
- Registry `stdppx/category/features.sexpr` is checked in/embedded. Native/schema
  SHA256 pins are built by CMake, installed under `libexec/on1x` and verified
  before/after invocation. Linux discovery uses the actual `/proc/self/exe`, not
  argv[0], CWD, PATH or schema/stdlib environment overrides. Existing MuLIB SHA256
  is compiled in one compiler-only TU; applications and PPXs use the existing
  On1x runtime without LibStone/compiler/Python runtime dependencies.
- The explicit initial phase split expands declaration spans before export
  binding; ordinary macro/PPX/staging/inference behavior is unchanged. Body macros
  and generic/associated annotations reject rather than being dropped. Details,
  grammar subset, bounds and stage statuses: `docs/category.md`.

### Executable coverage

`tests/test_category.py` compiles the actual Eq/ordering programs under
`stdppx/examples/category/`, then compiles/runs strict C11. It covers source/CLI
activation parity, repeated/disabled/unknown/late/nested directives, multiple
named witnesses, extreme signed ordering, numeric/bool targets, MIR C11,
ordinary PPX/macro coexistence, an independently compiled authoring-parser API,
malformed native packets, token/source/coordinate binding, deterministic facts,
declaration/token budgets and output preservation on failure.

Negative cases include wrong member parameter/result/arity/body/call types,
duplicate member/parameter/protocol/impl, orphan/foreign protocol, unsupported
generic/inheritance/associated syntax, generated-name collisions, source-PPX
authorization, conflicting ordinary PPX labels and attempted generated-interface
erasure by an explicitly authorized test mutator. Optional SDK runs additionally
check the generated HIR through real Nerotypical and Chisel.
The C++ meta API regression also checks exact activation byte ranges, independent
host rejection of inactive carrier owners, preservation after failure and
non-executing inspection. Its missing rejection was reproduced before the host
preflight guard was added.

Import tests cover aliases, module-local activation, disabled imported features,
unknown directives in unused imports and legacy quoted flat imports. The latter
reproduced a real initial helper-name collision; origin-domain-separated fresh
names fixed it without changing public selectors. Security tests cover spoofed
argv[0], hostile PATH/schema/protocol variables, relocated installation without
Python in PATH, binary/schema drift and missing native artifacts. No test was
disabled to obtain passing results.

### Commands / results

Same configured generated/fallback and SDK trees as the preceding numeric
verification. Sanitizer configuration retains explicit generated-program flags
`ON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer`, in addition
to compiler/runtime/native-PPX CMake instrumentation.

```sh
cmake --build /tmp/opencode/on1x-loader-gcc --parallel 2
ctest --test-dir /tmp/opencode/on1x-loader-gcc --output-on-failure --parallel 2 \
  --output-junit /tmp/opencode/on1x-loader-gcc/results-category.xml
cmake --build /tmp/opencode/on1x-loader-clang --parallel 2
ctest --test-dir /tmp/opencode/on1x-loader-clang --output-on-failure --parallel 2 \
  --output-junit /tmp/opencode/on1x-loader-clang/results-category.xml
cmake --build /tmp/opencode/on1x-data-sdk --parallel 2
ctest --test-dir /tmp/opencode/on1x-data-sdk --output-on-failure --parallel 2 \
  --output-junit /tmp/opencode/on1x-data-sdk/results-category.xml
cmake --build /tmp/opencode/on1x-safe-ffi-sanitize --parallel 2
ctest --test-dir /tmp/opencode/on1x-safe-ffi-sanitize --output-on-failure --parallel 2 \
  --output-junit /tmp/opencode/on1x-safe-ffi-sanitize/results-category.xml
```

Final GCC/generated **41/41**, Clang/fallback **40/40**, optional GCC SDK **49/49**,
and optional Clang SDK ASan/UBSan **49/49**, with no sanitizer findings. These
final runs include the independent inactive-carrier host regression and ordinary
PPX interface-erasure rejection.

`python3 -m unittest discover -s on1x/stdppx/tests -v`: **30/30**. Changed Python
AST parsing, JSON schema decoding and `git diff --check` pass. Repository-wide
`TMPDIR=/tmp/opencode python3 tools/check.py --jobs 2` is an additional independent
check and **failed (exit 1)** outside the changed On1x sources. Its results were:

- Module examples **999/999**, compositions **188/189**, focused tests
  **609/612**, combined builds **10/10**, isolated headers **3495/3495**.
- MuLL registration is missing `mulib-elvi-scrape.h`; dependency strata are stale.
- `examples/elvi-scrape.c` has unresolved load/url/parse symbols;
  `tests/test-elvi-scrape.c` fails its load assertion.
- Inventory/manual checks report an elvi-scrape dependency mismatch, missing
  `examples/modules/elvi-scrape.c`, missing declaration/implementation header
  registration and missing `MULIB_IMPL_ELVI_SCRAPE` registration.
- Compilation of `tests/test_hardening.c` and `tests/test_hostile_inputs.c`
  exceeded the checker's 90-second timeout. These timing failures were not
  classified as semantic regressions.

HEAD remained `9090e643e11051e000b3bcc68ebc777dfe07776a`; this task changed none of
those root headers, inventory generators, examples or tests. No unrelated
regeneration or kernel fixes were attempted. The repository-wide suite is **not
green**, and its passing checks are not Category completion evidence. The On1x
suite results above remain separate.

Build-helper/optimized-MIR acceptance was also executed:

```sh
python3 on1x/tools/on1x-build.py \
  /tmp/opencode/on1x-loader-gcc/test-artifacts/category/activation.on1x \
  --on1xc /tmp/opencode/on1x-loader-gcc/on1xc --cc gcc --enable Protocols \
  --via-mir --optimize-ir -o /tmp/opencode/on1x-category-acceptance
/tmp/opencode/on1x-category-acceptance
readelf -d /tmp/opencode/on1x-category-acceptance
```

The fixture is the Eq example without its source directive. Output is `true`,
then `false`; the only `NEEDED` library is `libc.so.6`.

Native-off profile:

```sh
cmake -S on1x -B /tmp/opencode/on1x-category-disabled \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DON1X_USE_FALLBACK_SCANNER=ON -DON1X_BUILD_NATIVE_PPX=OFF -DBUILD_TESTING=OFF
cmake --build /tmp/opencode/on1x-category-disabled --target on1xc --parallel 2
```

After copying an existing native binary, valid pin and schema into that build's
`stdppx/` directory, `on1xc --check .../category/eq.on1x` still rejects with
`ON1X_BUILD_NATIVE_PPX=OFF`; ordinary `--emit-c .../examples/hello.on1x` succeeds.
This is an actual negative profile, not a green test pretending a PPX executed.

### Unsupported / not promoted

Only the generic gateway and explicit **module-local concrete scalar** witness
selectors are executable. Bare direct syntax, first-class nominal witness types,
qualified schemes, implicit/generic resolution, cross-module evidence/interfaces,
package orphan authority, associated types/constants, signatures/functors,
abstract sealing, derive/laws, auto ownership/concurrency protocols, higher kinds
and dynamic dispatch remain unimplemented. Other-host native discovery and
build-host/target cross compilation are closed. No thread runtime was added;
TSAN is not applicable. This is neither full Category completion nor a self-hosted
compiler. Exact phase split and limits remain documented and fail closed.

## Numeric-limit continuation — literals, scalar MIR v3 and independent typing

Implemented the numeric syntax, MIR and independent-checker gaps from the
preceding slice. This does **not** claim every listed limit is removed:
floating execution remains guarded to the tested Linux x86_64 profile; other
targets need actual toolchains, execution and ABI/environment evidence.

Re-inspected base: MuLIB `cbdb2e290a41e39a8e6447f39426b1407c3e73b0`, configured
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763` at
`on1x/third_party/libstone`, unchanged. Platform/toolchains remain Linux
`7.2.6-arch2-1` x86_64, GCC/g++ 16.2.1, Clang/clang++ 22.1.8, CMake 4.4.3,
Python 3.14.7. All changes remain uncommitted.

### Executable changes and compatibility

- Both canonical generated grammar and fallback scanner recognize
  `u8/u16/u32/u64` integer suffixes (decimal or hex) and decimal `f32/f64`
  suffixes. Width overflow/invalid suffixes reject before publication. Unsuffixed
  i64/f64 semantics and hex digits ending in f32/f64 are preserved. f32 decimal
  literals round directly via `from_chars<float>`; the halfway-plus-epsilon
  regression distinguishes this from double rounding. Typed literals are bounded
  to 1,024 bytes including the suffix.
- PPX adds a distinct `UnsignedLiteral` under `NumericLiteral`, not the legacy
  signed-i64 `IntegerLiteral` category. C++, canonical/standalone Python and native
  On1x validators accept the same bounded spellings. Native/Python identity and
  fold processors preserve typed literals and locations. Scanner parity includes
  valid/malformed suffix boundaries; old schema-pinned processors must reject
  new constructors rather than treating a full-range unsigned value as i64.
  Typed macros use the same categories, and unsigned literal matching compares
  both value and suffix type (decimal/hex equivalents match, mixed types do not).
- Existing MIR gains version 3, not a replacement IR. New typed scalar operations
  use bounded fixed-allowlist descriptors with exact signatures and conservative
  effect tokens. Constants/arguments validate unsigned widths and canonical IEEE
  payloads. Independent C++ evaluation, C11 emission, serialization and the
  transactional pass pipeline execute unsigned/f32 arithmetic, wrapping/bit/
  shift operations, all conversions, calls, slots, branches and numeric tuples.
  Legacy scalar v1 and f64/product v2 keep their old formats and lowering version.
  v3 is explicitly outside the existing Novostone relational mapping v1.
- Nerotypical constructs independent numeric/literal/operator/call type
  obligations, including concrete f64/f32/unsigned constructors. Post-sema
  corruption and 196 ground/structural oracle cases reject invalid type claims.
  This is typing evidence, not proof of IEEE arithmetic or foreign implementation
  semantics. Chisel additionally admits recursively trivial numeric/bool tuples
  and shared destructuring patterns, while rejecting other tuple field kinds.
- Positive/negative tests exercise malformed MIR descriptors, signature/effect
  drift, oversized descriptors, out-of-width constants/API arguments,
  noncanonical f32 NaNs, range errors and output preservation. Existing Python
  integer/binary32 oracles now execute HIR C11, plain/optimized MIR C11 and plain/
  optimized reference evaluation. Failure cases compare exit class, output and
  diagnostics between the C runtime and MIR execution.
- Initial full runs caught a changed x87 unsupported diagnostic and missing
  Chisel tuple lowering. The legacy diagnostic was restored, and real trivial
  tuple checking (with negative field tests) was added; no test was disabled.
- Review regressions reproduced legacy macro misclassification of unsigned
  literals and omitted v3 descriptors in optimizer preservation records. Both
  were fixed and covered by executable macro/mutated-MIR rejection tests.
  Malformed unary HIR with a missing numeric operand now diagnoses rather than
  dereferencing a null child. Literal-budget tests cover the exact 1,024-byte
  acceptance/rejection boundary.

### Commands and final results

Build configurations are the preceding generated/fallback and optional SDK
profiles. The sanitizer tree retains explicit
`ON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer` so Python-
built applications and positive admission boundaries are instrumented too.

```sh
cmake --build /tmp/opencode/on1x-loader-gcc --parallel 2
ctest --test-dir /tmp/opencode/on1x-loader-gcc --output-on-failure --parallel 2 \
  --output-junit /tmp/opencode/on1x-loader-gcc/results-numeric-limits.xml
cmake --build /tmp/opencode/on1x-loader-clang --parallel 2
ctest --test-dir /tmp/opencode/on1x-loader-clang --output-on-failure --parallel 2 \
  --output-junit /tmp/opencode/on1x-loader-clang/results-numeric-limits.xml
cmake --build /tmp/opencode/on1x-data-sdk --parallel 2
ctest --test-dir /tmp/opencode/on1x-data-sdk --output-on-failure --parallel 2 \
  --output-junit /tmp/opencode/on1x-data-sdk/results-numeric-limits.xml
cmake --build /tmp/opencode/on1x-safe-ffi-sanitize --parallel 2
ctest --test-dir /tmp/opencode/on1x-safe-ffi-sanitize --output-on-failure --parallel 2 \
  --output-junit /tmp/opencode/on1x-safe-ffi-sanitize/results-numeric-limits.xml
```

Final: GCC/generated **40/40**, Clang/fallback **39/39**, optional GCC SDK
**48/48**, optional Clang SDK ASan/UBSan **48/48**, no sanitizer findings.

Integrated acceptance:
`python3 on1x/tools/on1x-build.py on1x/examples/numeric_literals.on1x --on1xc
/tmp/opencode/on1x-data-sdk/on1xc --cc gcc --via-mir --optimize-ir --verify-types
--memory-check -o /tmp/opencode/on1x-numeric-limits-example`, then execute it.
Output: `18446744073709551615`, `255`, `0x1.800000p+1`, `0`, `1`, `2`,
`1065353217` on separate lines. This uses the real MIR/C11 backend and both
independent checkers, without compiler/LibStone dependencies in the application.
`readelf -d /tmp/opencode/on1x-numeric-limits-example` reports only `libc.so.6`
as a `NEEDED` library. `git diff --check`, changed Python AST parsing and changed
JSON schema decoding also pass.

### Remaining limits / unavailable certification

`command -v qemu-aarch64 aarch64-linux-gnu-gcc qemu-x86_64
x86_64-w64-mingw32-gcc` found no tools. No ARM/Windows/macOS target execution,
alternative floating ABI, FP exception-mask restoration or new native backend
was verified; platform/environment guards remain intact. General aggregate/
closure/resource/foreign-call MIR, quantified/data-model independent checking,
narrow signed/pointer-sized types, hex float syntax, ownership/concurrency and
general safe FFI are still separate work. Finite safety budgets are intentional,
not limitations to remove. Status remains **EXECUTABLE** for the admitted numeric
subset, not full-language or cross-platform **VERIFIED**.

## M1/M2 numeric continuation — unsigned widths and binary32

Implemented distinct `u8/u16/u32/u64/f32` values in the canonical parser, types,
inference, semantic checker, HIR-to-C11 emitter, runtime and PPX type transport.
Actual source: `examples/unsigned_f32.on1x`; semantics and unsupported cases:
`docs/numerics.md`. These are executable C11 value/scalar-FFI slices, not a
completed numeric MIR, signed-width family, Basis conformance or general FFI.

Base remains MuLIB `cbdb2e290a41e39a8e6447f39426b1407c3e73b0`, LibStone
`55755485acaded7c4ed804c0946bc8c7363f6763` at `on1x/third_party/libstone`,
unchanged. Linux `7.2.6-arch2-1` x86_64, glibc 2.44; GCC/g++ 16.2.1
(20260810), Clang/clang++ 22.1.8, CMake 4.4.3, Python 3.14.7.

### Implementation evidence

- Unsigned arithmetic checks overflow/underflow/divisors before C operations.
  Widened operands avoid signed C integer-promotion overflow for u8/u16;
  explicit wrapping arithmetic is modulo 2^N. Bitwise/checked shift intrinsics,
  every unsigned-width conversion, checked signed/float conversion, exact
  bit construction/transport and full-range decimal printing execute normally.
- Binary32 uses actual C float, not widened double storage or signed handles.
  Volatile single-precision operations round once, preserve IEEE special values
  and canonicalize NaNs. Conversion checks handle double-to-float overflow
  *before* any out-of-range C cast, including the FLT_MAX/Inf rounding midpoint.
  Direct u64-to-f32 rounding avoids the tested double-rounding cases. The
  floating environment guard and relaxed-mode restrictions are retained.
- Runtime ABI additions are version 1 (`ON1X_UNSIGNED_ABI_VERSION`,
  `ON1X_F32_ABI_VERSION`); old headers fail generated-code guards. Extern scalar
  parameters/results and explicit flattening use exact C types. Raw and reviewed
  generated adapters support unsigned/f32 with strict prototype, scalar layout,
  signedness/range and float format checks. A regression exposed Clang's unused
  helper error for bool-/unsigned-only bundles; narrowing support is now emitted
  only when required, and dedicated bundles test the fix.
- Safe profile/evidence v3 adds unsigned/f32 without widening v1/v2.
  Chisel models numeric primitives as scalar operations and checks exact typed
  evidence. MIR/Nerotypical fail closed on the new types. Unsuffixed literals
  remain i64/f64; explicit constructors avoid changing existing source meaning.
  PPX v1 type constructors are additive; canonical and standalone schemas and
  native/Python signature writers agree. Scanner rules are unchanged, and both
  scanners scan the new acceptance example.
- `test_unsigned_f32.py` executes independent seeded Python integer and binary32
  bit oracles, exact integer-to-f32 rounding, conversions, special-value printing,
  O0/O2 agreement, aggregate/closure/staging composition and PPX identity
  round-trips. Negatives cover implicit mixing, unsigned negation, overflow,
  underflow, zero divisors, negative/oversized/lossy shifts, narrowing, NaN/Inf
  and signed/unsigned conversion boundaries, invalid bit imports, environment
  changes, old runtime headers and unsupported MIR/checkers. Separate strict
  C11/C++20 implementations execute full-range unsigned and f32 raw ABI calls.
- `test_contracts_sdk.py` adds real extracted unsigned/f32 functions, full-range
  C/C++/On1x consumers, a binary32 FFI bit oracle, type/layout/signedness drift,
  typed-evidence drift, and compatibility of v1/v2 bundles. No production
  contract review is claimed; fixture review flags remain synthetic gate tests.

### Reproducible validation

The optional SDK configuration and pinned Satie checkout remain as recorded
below. Ordinary profiles exercise generated GCC and fallback Clang scanning.
All generated C is compiled with `-std=c11 -Wall -Wextra -Wpedantic -Werror`.

**Sanitizer scope correction:** inspection found that the earlier
`on1x-safe-ffi-sanitize` tree instrumented CMake-built compiler/runtime/PPX
targets, but its empty `ON1X_TEST_C_FLAGS` did not instrument most executables
compiled by Python tests. Earlier 46/46 runs remain historical results, not
proof of sanitizer execution for those generated programs. This continuation
explicitly configures the missing flags; the full 47/47 run now instruments
compiler tools, generated applications and positive FFI admission boundaries.

```sh
cmake --build /tmp/opencode/on1x-loader-gcc --parallel 4
ctest --test-dir /tmp/opencode/on1x-loader-gcc --output-on-failure --parallel 3 \
  --output-junit /tmp/opencode/on1x-loader-gcc/results-unsigned-f32.xml
cmake --build /tmp/opencode/on1x-loader-clang --parallel 4
ctest --test-dir /tmp/opencode/on1x-loader-clang --output-on-failure --parallel 3 \
  --output-junit /tmp/opencode/on1x-loader-clang/results-unsigned-f32.xml
cmake --build /tmp/opencode/on1x-data-sdk --parallel 4
ctest --test-dir /tmp/opencode/on1x-data-sdk --output-on-failure --parallel 3 \
  --output-junit /tmp/opencode/on1x-data-sdk/results-unsigned-f32.xml
cmake -S on1x -B /tmp/opencode/on1x-safe-ffi-sanitize \
  '-DON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer'
cmake --build /tmp/opencode/on1x-safe-ffi-sanitize --parallel 4
ctest --test-dir /tmp/opencode/on1x-safe-ffi-sanitize --output-on-failure --parallel 3 \
  --output-junit /tmp/opencode/on1x-safe-ffi-sanitize/results-unsigned-f32.xml
```

Results: GCC/generated **39/39**, Clang/fallback **38/38**, optional GCC SDK
**47/47**, Clang SDK ASan/UBSan **47/47**. No sanitizer findings. Tests initially
caught diagnostic regressions for old invalid operator expressions; historical
rejection diagnostics were restored rather than disabling tests.

Build-helper acceptance also ran:
`python3 on1x/tools/on1x-build.py on1x/examples/unsigned_f32.on1x --on1xc
/tmp/opencode/on1x-loader-gcc/on1xc --cc gcc -o
/tmp/opencode/on1x-unsigned-f32-example` followed by the executable. Output:
`18446744073709551615`, `0`, `42`, `0x1.800000p+1`, `2`,
`0x1.000000p+64` on separate lines. `git diff --check` and Python syntax
compilation of the changed test/PPX drivers pass.

Not certified: other targets, TSAN, direct f32/unsigned literal suffixes,
pointer-sized/narrow signed integers, numeric MIR/Nerotypical, compiler float
folding, borrowed buffers/foreign aggregates/resources/callbacks, full foreign
effects or standard-library numeric API migration. Existing Basis word/real
facades retain their compatibility encodings. No commits or pushes were made.

## M2 continuation — exact boolean/binary64 foreign scalars

Scoped task: extend the reviewed scalar-only boundary, not general safe FFI.
`tools/on1x-bind.cpp` now emits exact C `_Bool`/On1x `bool` and binary64 C
`double`/On1x `f64` parameters/results, including typedefs and mixed signatures.
This closes the generated float-scalar adapter gap; buffers/resources/callbacks
and arbitrary C implementation verification remain unsupported.

Re-inspected repository `cbdb2e290a41e39a8e6447f39426b1407c3e73b0`; configured
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763` at
`on1x/third_party/libstone`, unchanged. Linux `7.2.6-arch2-1` x86_64,
glibc 2.44; GCC/g++ 16.2.1 (20260810), Clang/clang++ 22.1.8, CMake 4.4.3,
Python 3.14.7. All changes in this continuation are uncommitted.

### Implementation and compatibility

- Scalar descriptors preserve the exact extracted C spelling, width and u64
  alignment. Generated `_Generic` checks validate the actual function pointer
  and `_Bool`/`double` identity, not just a same-sized substitute. Static
  assertions check width/alignment and binary64 format. Double adapters match
  the runtime's Linux x86_64 GCC/Clang/SSE2 profile and reject exposed relaxed
  IEEE modes. Integer adapters retain checked narrowing and existing raw names.
- Integer-only bundles retain `reviewed-scalar/v1` and
  `on1x.admitted-extern/v1`; bundles using bool/f64 select profile/evidence v2.
  v1 parsing still rejects bool/f64. The JSON manifest remains
  `on1x.safe-ffi/v1` because its structure is unchanged. See `docs/contracts.md`
  for the additive migration contract; old compilers reject v2 evidence.
- `src/safety.cpp` models f64 literals, parameters and explicit numeric/bit/
  classification intrinsics as trivially copied scalar values. Evidence checks
  match every parameter and result at both declarations and calls. The C++ API
  rejects resource/non-scalar entries even if supplied without the wire parser.
  Trailing commas and unsupported scalar types fail closed.
- `tests/fixtures/contract_scalars.on1x` is an actual compiled consumer of the
  generated facade; it checks both booleans, mixed signatures, evaluation order,
  negative zero, subnormals, infinities and NaNs. The SDK test compares a seeded
  44-value bit corpus against an independent Python binary64 oracle through
  separate C and C++20 `extern "C"` implementation objects, at C `-O0`/`-O2`.
- Negative cases cover declaration/callsite parameter/result drift, malformed
  and legacy evidence, wrong source argument types, bool-as-byte and
  double-as-integer typedef substitutions, width/alignment drift (including
  alignment greater than `UINT_MAX`), unsupported floating formats, relaxed
  math modes, and scalar MIR's explicit foreign-call rejection. Existing
  review/effect/compile/runtime/failure-publication tests are retained.
- The interrupted integration initially failed on unsupported f64 HIR and a
  missing boolean harness declaration; the new tests reproduce these paths and
  now pass. No failure was disabled. Gate observations remain representative
  boundary execution, not proofs of foreign arithmetic or semantic claims.

### Executed commands and results

The SDK configuration enables ExoML extraction, Exotract, Chisel, Nerotypical
and Novostone, using `ON1X_SATIE_ROOT=/tmp/opencode/on1x-final-type-sdk`.
The ASan/UBSan configuration is the one recorded in the earlier scalar-only
slice below. Its generated-program instrumentation claim is superseded by the
scope correction in the unsigned/binary32 continuation above.

```sh
cmake --build /tmp/opencode/on1x-data-sdk --parallel 4
ctest --test-dir /tmp/opencode/on1x-data-sdk --output-on-failure --parallel 3 \
  --output-junit /tmp/opencode/on1x-data-sdk/results-scalar-v2.xml
cmake --build /tmp/opencode/on1x-safe-ffi-sanitize --parallel 4
ctest --test-dir /tmp/opencode/on1x-safe-ffi-sanitize --output-on-failure --parallel 3 \
  --output-junit /tmp/opencode/on1x-safe-ffi-sanitize/results-scalar-v2.xml
cmake --build /tmp/opencode/on1x-loader-gcc --parallel 4
ctest --test-dir /tmp/opencode/on1x-loader-gcc --output-on-failure --parallel 3 \
  --output-junit /tmp/opencode/on1x-loader-gcc/results-scalar-v2.xml
cmake --build /tmp/opencode/on1x-loader-clang --parallel 4
ctest --test-dir /tmp/opencode/on1x-loader-clang --output-on-failure --parallel 3 \
  --output-junit /tmp/opencode/on1x-loader-clang/results-scalar-v2.xml
```

Results: optional GCC/generated SDK **46/46**; Clang/generated SDK ASan/UBSan
**46/46**; ordinary GCC/generated **38/38**; ordinary Clang/fallback **37/37**.
No sanitizer findings. SDK tests exercise real libclang extraction; the ordinary
profiles do not claim to exercise unavailable SDK integrations.

### Still unsupported

Unsigned/f32/enums and pointer-sized numeric language types, foreign aggregate
ABI, nullable pointers/buffers, owned/borrowed foreign resources, recoverable
foreign errors, callbacks, thread affinity and full foreign effects. No
production contract is independently reviewed by this change. No 32-bit,
non-Linux or non-x86_64 execution, TSAN/concurrency, foreign floating-environment
restoration, or native/VM backend certification is claimed. This is an
**EXECUTABLE** subset, not a completed M2 or general safe FFI.

## M2 prerequisite — reviewed scalar-only safe FFI admission

Scoped implementation task: open `emit-safe` only for a complete reviewed
signed-integer/void bundle with no pointer, aggregate, ownership, lifetime,
error, callback, destructor, invalidation or concurrency-lowering obligation.
This is `reviewed-scalar/v1`, not general safe FFI and not proof of arbitrary C
implementation behavior.

Re-inspected base: MuLIB `cbdb2e290a41e39a8e6447f39426b1407c3e73b0`,
configured LibStone `55755485acaded7c4ed804c0946bc8c7363f6763` at
`on1x/third_party/libstone`, unchanged. Linux `7.2.6-arch2-1` x86_64 GNU/Linux;
GCC/g++ 16.2.1
(20260810), Clang/clang++ 22.1.8, CMake 4.4.3, Python 3.14.7.

### Implementation

- `tools/on1x-bind.cpp`: `emit-safe DECL CONTRACT PREFIX --on1xc PATH --cc
  PATH --workdir DIR [--cflag ARG] [--impl FILE]` now runs four mandatory
  gates. Review metadata must cover the full bundle; profile eligibility is
  all-or-nothing; generated use is checked by the real Chisel-enabled compiler;
  and a strict C11 adapter/harness is compiled and run with bounded outputs and
  timeouts. Any missing tool input, unreviewed/unknown-effect function,
  resource/error/callback obligation, compile failure, timeout or nonzero
  boundary exit rejects without publishing partial outputs.
- Successful emission exclusively creates `PREFIX.on1x`, `PREFIX.c`,
  `PREFIX.admitted-extern.txt` and `PREFIX.manifest.json`. The manifest records
  source/header hashes, target/compiler identity, contract provenance and
  effects, flags/implementation TUs, Chisel report and one observed
  representative boundary result per function. Observations are evidence that
  the target call boundary executed, not semantic expected-value proofs.
- Generated adapters retain the raw emitter's `_Generic` function-pointer
  check and checked narrowing, and now assert extracted `_Alignof` as well as
  size, signedness and range. Safe modules expose ordinary `on1x_safe_*`
  wrappers while keeping their raw declarations explicit and full-fingerprint-scoped
  so stale admission companions cannot authorize a regenerated module identity.
- `src/{cli,safety}.cpp` and `include/on1x/safety.hpp`: Chisel memory modes accept
  bounded `--admitted-extern FILE` companions. The parser rejects unknown
  schema, malformed identity/signatures, NUL/oversize input, duplicate names,
  unsupported types and limits; checked HIR rejects absent declarations and
  arity/result drift. Raw externs without exact evidence remain rejected.
- Safe eligibility allows declared `read`, `write`, `io` and `foreign_call`
  effects because no resource crosses this scalar boundary. It rejects
  `allocate`, `free`, `escape`, `thread` and `suspend`; the emitted evidence
  does not call this profile pure.
- Test contracts deliberately install synthetic review flags only to exercise
  the machinery. No shipped JSON/Vec/Parsec/Mune contract has received an
  independent production review through this change.

### Verification evidence

- Optional SDK build (ExoML extraction, Chisel, Nerotypical and Novostone):
  `cmake --build /tmp/opencode/on1x-data-sdk --parallel 4` followed by
  `ctest --test-dir /tmp/opencode/on1x-data-sdk --output-on-failure --parallel
  3 --output-junit /tmp/opencode/on1x-data-sdk/results-safe-ffi.xml` →
  **46/46**.
- Ordinary regression profiles after the CLI gained optional admission input:
  `cmake --build /tmp/opencode/on1x-loader-gcc --parallel 4 && ctest
  --test-dir /tmp/opencode/on1x-loader-gcc --output-on-failure --parallel 3
  --output-junit /tmp/opencode/on1x-loader-gcc/results-safe-ffi.xml` → **38/38**
  (GCC/generated scanner); equivalent `/tmp/opencode/on1x-loader-clang`
  command → **37/37** (Clang/fallback scanner). The no-Chisel builds compile
  the CLI and reject memory-checking options rather than referencing unavailable
  admission code.
- Fresh optional-SDK ASan/UBSan configuration:
  `cmake -S on1x -B /tmp/opencode/on1x-safe-ffi-sanitize
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined' -DBUILD_TESTING=ON
  -DON1X_WITH_EXOML=ON -DON1X_WITH_EXOML_EXTRACT=ON
  -DON1X_WITH_EXOTRACT=ON -DON1X_WITH_CHISEL=ON
  -DON1X_WITH_NEROTYPICAL=ON -DON1X_WITH_NOVOSTONE=ON
  -DON1X_SATIE_ROOT=/tmp/opencode/on1x-final-type-sdk`, build, then `ctest
  --test-dir /tmp/opencode/on1x-safe-ffi-sanitize --output-on-failure
  --parallel 3 --output-junit
  /tmp/opencode/on1x-safe-ffi-sanitize/results-safe-ffi.xml` → **46/46**.
- `on1x.contracts_sdk` executes real libclang extraction and covers successful
  four-gate emission, manifest/evidence contents, strict native boundary calls,
  a separately compiled On1x consumer, explicit Chisel admission, omission and
  arity drift, unreviewed/full mixed bundles, disallowed effects, absent gate
  tools, forced Chisel failure, target compiler and target runtime failures with
  no partial publication, existing-output/workdir protection, integer range
  failure, signature/size/alignment/signedness drift, and a separately compiled
  C++20 `extern "C"` implementation object consumed by the strict C11 adapter.
- `on1x.safety` independently covers admitted evidence parsing, use without
  memory checking, schema/result/absent-declaration failures and preservation of
  the default raw-extern rejection.

### Remaining blockers

Accountable reviews of production contracts; unsigned/f32/f64/enums and
pointer-sized ABI; nullable pointers, buffers/slices and aggregate layout;
owned/borrowed resources and cleanup; recoverable C errors; callback trampolines;
thread affinity and Wetstone effect checking; cross-target and sanitizer
certification. This slice does not complete M2.

## Language continuation — bounded value data model (records, sums, arrays, slices)

Scoped implementation task: executable structural records, closed tagged sums
with exhaustive `match`, fixed arrays and **owning snapshot slices** in the
canonical frontend and by-value C11 backend. This is an M1 slice; MIR lowering,
mutable/borrowed arrays, nominal/parametric ADTs, derives, ownership and safe
FFI remain open. The value-data contract is documented in
`docs/data-model.md`; syntax/semantics in `docs/language.md`.

Re-inspected base: MuLIB `eeb6de585303e1ea0dfd8bf2260328289b04587c`, configured
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763` at
`on1x/third_party/libstone`, unchanged. Linux `7.2.6-arch2-1` x86_64 GNU/Linux;
GCC/g++ 16.2.1 (20260810), Clang/clang++ 22.1.8, CMake 4.4.3, Python 3.14.7.

### Pre-change baselines (recorded before the slice)

- `TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-gcc
  --output-on-failure --output-junit baseline-data.xml` → 37/37
  (GCC/generated scanner).
- `TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-clang
  --output-on-failure --output-junit baseline-data.xml` → 36/36
  (Clang/fallback scanner; no scanner-parity test in the fallback tree).
- `git diff --check` clean; LibStone checkout unchanged.

### Implementation

- New syntax, both scanners: `record{label:type,...}` (labels canonicalized
  lexicographically for type identity; constructor operands stay in source
  order), `sum{Tag:type,...}`, `variant<sum{...}>(Tag, payload)`,
  `match(expr){Tag(pattern)=>arm,...}` (exhaustive, one arm per constructor),
  `array<T,N>`/`slice<T,N>` (extent ≤ 256), `array[...]`, checked `a[i]`,
  length `a[]` and bounds-checked `a[lo:hi]` slice construction. No lexical
  changes; the fallback scanner accepts identical text.
- New structural types `Record`/`Sum`/`Array`/`Slice` in `include/on1x/type.hpp`
  with label/extent shape data, canonical `Type::data` construction and
  `type_name` printing; inference (`src/infer.cpp`) builds canonical data
  terms, unifies labels/extent, infers array element types/extents, checks
  variant payloads and exhaustive duplicate-free match arms against known sum
  shapes.
- Frontend checks (`src/sema.cpp`): value-data positions require trivially
  copied types (i64/f64/bool/tuples/value-data; functions/strings/JSON/sexpr/
  buffer rejected), bounded expansion (256 fields/extent per level, 32 nesting
  levels, 4096-unit width budget), data-valued extern ABI rejected, record
  field/label arity, unknown labels/constructors, duplicate arms/fields, arm
  payload and result agreement, match-result width. The tuple-annotation
  `record{a:i64}` ordering is shape-canonical.
- C11 codegen (`src/codegen.cpp`): private compiler-owned structs
  (`on1x_data_type_N`), sums as `int64_t tag` + payload union, arrays as fixed
  element storage, slices as backing copy + `start`/`length`; field/index/
  slice/length/match/variant lowering with source-located bounds checks
  (`on1x_bounds_index`, `on1x_bounds_slice`) and discriminant validation
  (`on1x_invalid_tag`); `ON1X_VALUE_DATA_ABI_VERSION` guard emitted when used;
  slices avoid intermediate aggregate temps so generated code compiles with
  `-Wall -Wextra -Wpedantic -Werror` for both host compilers.
- Runtime (`runtime/on1x_runtime.[ch]`): located value errors, exit 70
  (bounds) — same terminal-failure policy as checked arithmetic; no new
  allocation, refcounting or destructor paths.
- Fail-closed adapters: `--emit-ir` rejects value data with
  `ON1X-MIR-UNSUPPORTED ... scalar MIR cannot lower record{...}`;
  `src/type_safety.cpp`, `src/safety.cpp`, `src/backend/lower.cpp` and
  `src/staging.cpp` reject the new kinds rather than approximating them.
  PPX round-trip support added instead: `ValueDataType`/`ValueData` nodes in
  `PPX.schema.json` (canonical, standalone and stdlib copies), `src/ppx_ast.cpp`
  read/emit, `DataExpression` macro category, signature writers
  (`stdlib/ppx/signature.on1x`, `stdppx/{passes.py,_support/passes.py}`), and
  `--types` listing.
- Not implemented: MIR for value data, data equality/ordering/printing, field
  or element mutation, zero-copy borrows, dynamic arrays, guards or refutable
  patterns, recursive/nominal/parametric data types, derives, foreign
  aggregate ABI.

### Verification evidence

- `TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-gcc
  --output-on-failure --parallel 3 --output-junit results-data.xml` →
  **38/38**, including the new `on1x.data` suite.
- `TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-clang
  --output-on-failure --parallel 3 --output-junit results-data.xml` →
  **37/37** after reconfiguration (fallback-scanner profile).
- `on1x.data` covers: record projection/nesting/canonical labels, sums with
  record/tuple/record payloads, arrays/slices of records, slice-of-slice
  composition, evaluation order, value passthrough; runtime bounds
  rejections (index high/negative, slice end/reversed/negative → exit 70,
  located message); static rejections (non-exhaustive, duplicate/unknown
  arms, arm arity/payload/result mismatch, unknown field, field on sum,
  duplicate fields, managed fields/results, data extern ABI, wrong variant
  payload/unknown tag, index on record, mixed array elements); MIR and
  structural-adapter fail-closed paths; deterministic `--emit-c`; and an
  end-to-end `tools/on1x-build.py` run.
- `examples/data_model.on1x` executes via the build helper with expected
  output (`0x1.9000000000000p+4`, `0x1.9000000000000p+4`,
  `0x0.0000000000000p+0`, `50`, `4`, `40`) on both compiler configurations,
  and identically under GCC `-O3` and Clang `-O2`.
- Sanitizers: fresh ASan/UBSan tree (`cmake -S on1x -B
  /tmp/opencode/on1x-data-sanitize -DCMAKE_C_COMPILER=clang
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_FLAGS="-fsanitize=address,undefined
  -fno-omit-frame-pointer" -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined
  -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
  -DBUILD_TESTING=ON`); full suite `TMPDIR=/tmp/opencode ctest --test-dir
  /tmp/opencode/on1x-data-sanitize --output-on-failure --parallel 3
  --output-junit results-data.xml` → **38/38**, including `on1x.data` (the
  compiler and generated code run under ASan/UBSan; no leak/UB findings).
- Optional-SDK profile (ExoML/Exotract/Chisel/Nerotypical/Novostone enabled,
  `ON1X_SATIE_ROOT=/tmp/opencode/on1x-final-type-sdk`, configuration mirroring
  the historical SDK tree): independent checkers fail closed on data programs
  with located diagnostics — `--check-memory` →
  `ON1X-MEMORY: expression memory model unavailable`, `--check-types` →
  `ON1X-TYPE-SAFETY: ... outside the structural adapter`, `--emit-ir` and
  `--analyze-ir` → `ON1X-MIR-UNSUPPORTED: scalar MIR cannot lower sum{...}`;
  full SDK suite `TMPDIR=/tmp/opencode ctest --test-dir
  /tmp/opencode/on1x-data-sdk --output-on-failure --parallel 3
  --output-junit results-data.xml` → **46/46**, including `on1x.data` and the
  SDK-specific suites (`type_safety`, `safety`, `external_memory_model`,
  `relational_hir`, `exotract`, `contracts_sdk`).
- PPX/AST wire compatibility: `--emit-ast` serializes value data as
  `ValueData`/`ValueDataType`; an external Python identity PPX round-trips
  `data_smoke.on1x` through `--ppx` and the rebuilt program reproduces the
  direct-compile output byte for byte. `--types` prints canonical data shapes
  (e.g. `take : fn(record{a:i64, b:i64}) -> i64`).
- Staged code over value data works: `quote { record{x:1} }` yields
  `code<record{x:i64}>` whose `run` returns the record; projections after
  `run` behave like ordinary values.

### Remaining blockers

MIR v2 for value data, mutation/borrowing, nominal/parametric ADTs, derives,
ownership resources, recoverable errors, safe FFI, concurrency.

## Language continuation — decimal literals and numeric/product MIR v2

Scoped implementation task: extend the existing frontend and MIR with decimal
f64 literals, dedicated floating operations and **trivially copied** nested
tuples. This is an M1/M3 prerequisite slice, not full aggregates, ownership,
safe FFI or concurrency. The earlier value-only results below are historical;
their float-literal/MIR exclusions are superseded only by this scoped profile.

Re-inspected base: MuLIB `eeb6de585303e1ea0dfd8bf2260328289b04587c`, configured
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763` at
`on1x/third_party/libstone`, unchanged. Linux `7.2.6-arch2-1` x86_64 GNU/Linux;
GCC/g++ 16.2.1 (20260810), Clang/clang++ 22.1.8, CMake 4.4.3, Python 3.14.7.

### Compatibility and implementation

- Decimal fraction/exponent tokens work in both Aurocks and fallback scanners.
  The bounded source decoder rejects overflow/underflow-to-zero and emits exact
  binary64 bits. Existing i64 literals, including hexadecimal spellings with
  `e` digits, are unchanged. Float signs remain unary operators.
- The v1 PPX schema gains `FloatLiteral` under `NumericLiteral`, not
  `IntegerLiteral`; all three validator implementations and the standalone
  schema copy are synchronized. Native/Python identity, deriving, folding and
  malformed-spelling protocol tests exercise the new node. Old pinned PPXs may
  reject the additive constructor and must be rebuilt deliberately.
- Existing scalar source still produces MIR v1; saved v1 modules continue to
  round-trip. Numeric/product source selects v2, with explicit float opcodes,
  canonical IEEE constant bits and nested structural tuple type descriptors.
  Bad versions, types, tuple arities/indices, NaN constants, effects and entry
  signatures reject rather than produce C or overwrite existing output.
- Nested tuples admit only i64/bool/f64 leaves in MIR. Calls, returns, slots,
  recursive functions, shared source destructuring patterns and simultaneous
  phi transfers use value copies. The typed evaluator API exposes exact float
  bits and ordered tuple elements; it validates incoming shapes and NaNs.
  Frame budgets count nested fields and copies consume work. Type admission and
  memoized structural equality share the verifier budget.
- Floating arithmetic/conversions/comparisons retain environment/range-check
  effects. No float arithmetic folding is added. Pass descriptors are version 2;
  reports identify `portable-numeric-product-v2` while preserving the v1 report
  envelope and the legacy scalar target name. Novostone mapping remains v1-only
  and explicitly rejects v2; optional type/ownership adapters still reject f64.
- A final review caught an evaluator guard that originally checked SSE2 presence
  but not `FLT_EVAL_METHOD`. The GCC regression now recompiles the **actual**
  evaluator translation unit with `-mfpmath=387`, links it with the canonical
  frontend and requires a located unsupported error. This closes silent x87
  double-rounding, without substituting a test evaluator or changing scalar code.

### Validation commands

Final full-suite results after the x87 guard regression: **155/155 CTests**,
with no test exclusions, disabled tests or skips:

| Build directory under `/tmp/opencode/` | Profile | Result |
|---|---|---|
| `on1x-loader-gcc` | Strict GCC Debug / generated scanner | 37/37 |
| `on1x-loader-clang` | Strict Clang Debug / fallback scanner | 36/36 |
| `on1x-loader-sanitize` | Clang / generated scanner / ASan+UBSan | 37/37 |
| `on1x-final-gcc` | GCC / generated scanner / optional SDKs ON | 45/45 |

The JUnit files are `results-v2-final.xml` in those four directories. Earlier
`results-v2-first.xml` runs also passed before the additional boundary tests.

The four existing strict Debug profiles are rebuilt and tested without excluding
tests. The sanitizer cache instruments compiler, runtime and emitted C with
ASan/UBSan and nonrecovering checks. Native PPXs are ON in all four. The optional
SDK configuration uses ExoML extraction, Exotract, Chisel, Nerotypical and
Novostone SOURCE with the existing prepared Satie SDK; Syren is OFF. SDK passing
tests are regression/admission-rejection evidence, not numeric safety proofs.

```sh
cmake --build /tmp/opencode/on1x-loader-gcc --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-gcc \
  --output-on-failure --parallel 3 --output-junit results-v2-final.xml
cmake --build /tmp/opencode/on1x-loader-clang --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-clang \
  --output-on-failure --parallel 3 --output-junit results-v2-final.xml
cmake --build /tmp/opencode/on1x-loader-sanitize --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-sanitize \
  --output-on-failure --parallel 3 --output-junit results-v2-final.xml
cmake --build /tmp/opencode/on1x-final-gcc --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-final-gcc \
  --output-on-failure --parallel 3 --output-junit results-v2-final.xml
```

`test_mir.py` compares HIR C11, plain/optimized MIR C11 and plain/optimized
reference evaluation. Fixtures include nested mutable tuple snapshots,
destructured parameters, recursion, generic imported numeric tuples, output
order/short-circuiting, NaNs/infinities, signed zero, subnormals, maximum finite
values, conversion traps, saved-wire round-trips and independently authored
aggregate phi permutations. `test_f64.py` now executes its independent Python
bit/arithmetic oracle in **all five** paths, and adds decimal rounding cases
including halfway ties. MIR tests also reject invalid floating environments and
exhaust aggregate frame storage under sanitizers. Scanner parity includes
complete/malformed exponent and dotted/ellipsis spellings plus seeded byte fuzz.

Additional complete f64/MIR suite runs pass with emitted-code GCC LTO,
Clang's individual precision-relaxation flags (the primitives restore precise
semantics), and explicit float-cast-overflow sanitization:

```sh
python3 on1x/tests/test_f64.py /tmp/opencode/on1x-loader-gcc/on1xc \
  "$PWD/on1x" /tmp/opencode/on1x-v2-lto-float gcc --cflag=-O3 --cflag=-flto
python3 on1x/tests/test_mir.py /tmp/opencode/on1x-loader-gcc/on1xc \
  "$PWD/on1x" /tmp/opencode/on1x-v2-lto-mir gcc --cflag=-O3 --cflag=-flto
python3 on1x/tests/test_f64.py /tmp/opencode/on1x-loader-clang/on1xc \
  "$PWD/on1x" /tmp/opencode/on1x-v2-precise clang \
  --cflag=-fno-honor-nans --cflag=-fno-signed-zeros
python3 on1x/tests/test_mir.py /tmp/opencode/on1x-loader-clang/on1xc \
  "$PWD/on1x" /tmp/opencode/on1x-v2-precise-mir clang \
  --cflag=-fno-honor-nans --cflag=-fno-signed-zeros
python3 on1x/tests/test_f64.py /tmp/opencode/on1x-loader-sanitize/on1xc \
  "$PWD/on1x" /tmp/opencode/on1x-v2-cast-sanitize clang \
  --cflag=-fsanitize=address,undefined,float-cast-overflow \
  --cflag=-fno-omit-frame-pointer --cflag=-fno-sanitize-recover=all
```

The real build-helper program also runs with both C compilers:

```sh
python3 on1x/tools/on1x-build.py on1x/examples/float_tuples.on1x \
  --on1xc /tmp/opencode/on1x-loader-gcc/on1xc --cc gcc \
  --via-mir --optimize-ir -o /tmp/opencode/on1x-float-tuples-gcc
/tmp/opencode/on1x-float-tuples-gcc
ldd /tmp/opencode/on1x-float-tuples-gcc
python3 on1x/tools/on1x-build.py on1x/examples/float_tuples.on1x \
  --on1xc /tmp/opencode/on1x-loader-gcc/on1xc --cc clang \
  --via-mir --optimize-ir -o /tmp/opencode/on1x-float-tuples-clang
/tmp/opencode/on1x-float-tuples-clang
ldd /tmp/opencode/on1x-float-tuples-clang
```

Both executables print `0x1.0000000000000p-1`, `0x1.0000000000000p-3`, `4`
on separate lines and depend only on libc/system loader, not LibStone, C++, Lua
or the On1x compiler. This is C11 AOT, not direct native code generation.

### Remaining unsupported profiles

No records/ADTs/exhaustive matching, arrays/slices, f32/other integer widths,
resource-bearing MIR aggregates, general moves/borrows, recoverable errors,
safe-import admission, callbacks, async/await, tasks/channels/select/actors or
native workers are added. Foreign calls remain unsupported in MIR. No new
32-bit/Windows/macOS/other-architecture, shared-runtime ABI, TSAN or self-hosting
claim is made. Follow `docs/capability-matrix.md`, `docs/numerics.md` and
`docs/backend-mir.md` for exact accepted boundaries and next blockers.

## Earlier M1 continuation — binary64 values (not complete M1)

Inspection/patch base: MuLIB `eeb6de585303e1ea0dfd8bf2260328289b04587c`.
The configured read-only LibStone is
`on1x/third_party/libstone` at `55755485acaded7c4ed804c0946bc8c7363f6763`,
with a clean working tree. Platform: x86_64 Linux `7.2.6-arch2-1`;
GCC/g++ 16.2.1 (20260810), Clang/clang++ 22.1.8, both targeting
`x86_64-pc-linux-gnu`; CMake 4.4.3, Python 3.14.7.

Before implementation, rebuilding and running the complete existing suites gave
**36/36 GCC/generated** and **35/35 Clang/fallback**. The old fallback compiler
rejected `examples/f64.on1x:2:16` with `unknown type: f64`. New tests now execute
that actual On1x program through the normal C11 backend and build helper.

### Final results

All four complete profiles pass; **155/155 CTests across configurations**, no
excluded, disabled or skipped tests:

| Build directory under `/tmp/opencode/` | Configuration | Result |
|---|---|---|
| `on1x-loader-gcc` | GCC Debug, generated scanner, optional analyzers OFF | 37/37 |
| `on1x-loader-clang` | Clang Debug, fallback scanner, optional analyzers OFF | 36/36 |
| `on1x-loader-sanitize` | Clang Debug, generated scanner, compiler/runtime/emitted C ASan/UBSan | 37/37 |
| `on1x-final-gcc` | GCC Debug, generated scanner, ExoML extraction/Exotract/Chisel/Nerotypical/Novostone ON | 45/45 |

These reused the documented strict `-Wall -Wextra -Wpedantic -Werror` C11/C++20
builds. All native PPXs remain ON. Syren is OFF in these profiles. The SDK build
retains the existing prepared Satie source SDK at
`/tmp/opencode/on1x-final-type-sdk`, with Novostone's SOURCE provider. No LibStone
or prepared-SDK source was modified. Optional checkers still **reject f64**;
their green regression tests are not a float ownership/type proof. The SDK
profile also runs actual extraction/archive regressions, not a mock SDK.

Exact final build/test commands (from the MuLIB root):

```sh
cmake --build /tmp/opencode/on1x-loader-gcc --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-gcc \
  --output-on-failure --parallel 3 --output-junit results-numeric-final.xml
cmake --build /tmp/opencode/on1x-loader-clang --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-clang \
  --output-on-failure --parallel 3 --output-junit results-numeric-final.xml
cmake --build /tmp/opencode/on1x-loader-sanitize --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-sanitize \
  --output-on-failure --parallel 3 --output-junit results-numeric-final.xml
cmake --build /tmp/opencode/on1x-final-gcc --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-final-gcc \
  --output-on-failure --parallel 3 --output-junit results-numeric-final.xml
```

Baseline JUnit files are `baseline-numeric.xml` in the two loader build
directories. Final JUnit files are `results-numeric-final.xml` in all four
directories. The first post-patch runs exposed a stale standalone PPX schema
copy, failing identity transport and the schema-equality test. The schema was
synchronized with the additive `F64Type` constructor; **all four complete suites
were rerun**, rather than suppressing those failures.

For a fresh default GCC configuration equivalent to the loader profile:

```sh
cmake -S on1x -B /tmp/opencode/on1x-numeric-new -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DON1X_LIBSTONE_ROOT="$PWD/on1x/third_party/libstone" \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'
cmake --build /tmp/opencode/on1x-numeric-new --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-numeric-new --output-on-failure
```

For fallback select Clang/clang++ and `-DON1X_USE_FALLBACK_SCANNER=ON` in
a separate directory. The measured sanitizer cache uses
`-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all`
in both C/C++ flags, plus the semicolon-separated equivalent in
`ON1X_TEST_C_FLAGS`. SDK setup/options remain documented in
`docs/{contracts,memory-safety,type-safety,backend-analysis}.md`.

### New executed coverage and extra runs

`tests/test_f64.py` compares 480 arithmetic results, 720 comparisons, 114 bit
imports/exports (including signaling/negative NaN canonicalization), 228
classification/sign checks, 72 integer-to-float conversions and seven valid
float-to-integer boundary cases with an independent seeded Python oracle.
It reruns the oracle under C `-O2` and `-O3`. Other tests cover:

- Signed zero, gradual underflow, maximum finite values, overflow, infinities,
  unordered NaNs and exact locale-independent hex output.
- Imports, inference, monomorphic numeric constraints, forall instantiation,
  tuple copies/destructuring, escaping closures, staging, counted lexical
  cleanup and environment-allocation failure.
- Real separately compiled C and C++ double definitions, flattened mixed
  parameter patterns, `_Generic`/C++ prototype checks, deliberate incompatible
  prototypes, observable operand order and short-circuit behavior.
- Five rejected numeric casts, four rejected MXCSR modes, twelve located source
  rejections with output preservation, four MIR-mode refusals, float ABI/version
  guards, missing-SSE2 refusal, and fast/finite-math compile rejection.
- Native/Python signature-PPX execution, source-AST round-trip, deterministic C,
  build-helper execution, and no float guard added to ordinary i64 output.

Additional complete numeric-suite commands also passed:

```sh
python3 on1x/tests/test_f64.py /tmp/opencode/on1x-loader-gcc/on1xc \
  "$PWD/on1x" /tmp/opencode/on1x-f64-lto gcc --cflag=-O3 --cflag=-flto
python3 on1x/tests/test_f64.py /tmp/opencode/on1x-loader-clang/on1xc \
  "$PWD/on1x" /tmp/opencode/on1x-f64-precise clang \
  --cflag=-fno-honor-nans --cflag=-fno-signed-zeros
python3 on1x/tests/test_f64.py /tmp/opencode/on1x-loader-sanitize/on1xc \
  "$PWD/on1x" /tmp/opencode/on1x-f64-cast-sanitize clang \
  --cflag=-fsanitize=address,undefined,float-cast-overflow \
  --cflag=-fno-omit-frame-pointer --cflag=-fno-sanitize-recover=all
python3 on1x/tools/on1x-build.py on1x/examples/f64.on1x \
  --on1xc /tmp/opencode/on1x-loader-gcc/on1xc --cc gcc \
  -o /tmp/opencode/on1x-f64-example
/tmp/opencode/on1x-f64-example
ldd /tmp/opencode/on1x-f64-example
```

The example prints `0x1.5400000000000p+5` (42.5). Its dynamic dependencies are
libc and the system loader only: no compiler, LibStone, Lua, libm or C++ runtime.
The normal `examples/hello.on1x` build/run also passes (prints `21`).

### Limits and next blockers

This is the **executable f64 value profile**, not a complete numeric language
or general FFI. See `docs/numerics.md` and `docs/capability-matrix.md`. Float
literals, f32, other integer widths, float MIR/folding, arrays/slices, foreign
records, generated float adapters, safe imports and recoverable conversion
errors remain unsupported. Safe-export gates stay closed. Other architectures,
32-bit targets, Windows/macOS, shared runtime ABI stability and TSAN are not
verified by this slice. No new threading facility is added; TSAN is not a float
value/ownership proof. The finite oracle corpus is not universal equivalence.

## ElviScrape hardening and fixture integration

- ElviScrape retains its six-function C11 single-header API and offline `.elvs`
  providers. Extraction now uses bounded iterative scanner work and staged
  results, ignores inert/raw-text markup, and terminates stale heading context.
  Ambiguous Google targets, malformed URL references and request fragments
  reject without loosening HTTPS/private-network policy.
- Independent HTTPS fixtures cover negotiated certificate/hostname/protocol/ALPN
  checks before HTTP writes, partial I/O, fixed/chunked/authenticated-EOF framing,
  malformed/duplicate metadata, unsupported content encoding, cumulative body
  limits, cleanup and unchanged outputs on failure. Per-allocation injection
  exercises compile/load/URL/parse/search/HTTPS paths with no outstanding tracked
  allocations after each attempt.
- **8/8 selected CTests pass with Clang ASan/UBSan**: three ElviScrape tests and
  all five other shared-webscan consumers. The final sanitizer-backed fuzz
  campaign executes **118,189 inputs** across DSL, HTML, all anchor modes and
  URL/redirect invariants, without a sanitizer/assertion failure. This is bounded
  testing, not a proof of complete HTML5 behavior or transport security.
- Both trusted provider files are registered in `std/mulib/catalog.json`.
  Explicit fixture selection now preserves source-pinned recipes, required
  exports and evidence inputs. The complete archive's `shylock-core` C math
  dependency is explicit, including GCC consumers without sanitizer-provided
  link libraries. The optional owned parser remains off.
- The complete `test_mulib.py` suite passes with GCC and Clang ASan/UBSan;
  **the previously recorded ElviScrape fixture-inventory failure is resolved**.
  Both embedded providers compile and drive offline extraction through a test-only,
  ABI-exact scalar adapter in generated On1x clients. The complete
  `test_exotract.py` suite also passes with GCC and Clang ASan/UBSan, including
  separate-TU C and On1x consumers of the one-TU archive.
- Header/manual and 16 scanner/provider/fixture/test evidence inputs are pinned.
  Source semantics/drift tests and real ExoML SDK extraction tests pass.
  `/tmp/opencode/elvi-solid-contracts-final` passes independent pin/check/recheck:
  `gate_b=true`, unchanged inputs, **six actual exports**, and
  `safe_export=false`. Annotations remain unreviewed; no safe FFI, independent
  certificate/DNS validation or Chisel/Wetstone usage proof is implied.

Artifacts: `/tmp/opencode/elvi-solid-build` (sanitizer CTests/fuzzer),
`/tmp/opencode/elvi-solid-{on1x-mulib-final,exotract-final}` (Clang sanitizer
consumers), `/tmp/opencode/elvi-solid-{on1x-mulib-gcc,exotract-gcc}` (GCC), and
`/tmp/opencode/elvi-solid-fuzz-final.log`.
The initial fuzzer launch hit its default RSS guard with the default sanitizer
quarantine; the completed campaigns use a 16 MiB quarantine and finish below
100 MiB reported peak RSS. The final fuzz command is:

```sh
ASAN_OPTIONS=quarantine_size_mb=16:thread_local_quarantine_size_kb=64 \
  /tmp/opencode/elvi-solid-build/mulib_fuzz_elvi_scrape \
  /tmp/opencode/elvi-solid-corpus -max_total_time=60 -max_len=65536 \
  -timeout=5 -rss_limit_mb=8192 -print_final_stats=1 -verbosity=0
```

## Mune contract rebinding

The Lua standard-library continuation is rebound in both contract catalogs:

- `contracts-exoml/mune.exocontract.json` covers all **52** compiler-discovered
  public Mune declarations, with exact anonymous parameter names preserved.
  Gate B passes; review metadata and `safe_export` remain **false**. This is an
  AI-authored source audit, not independent review or generalized safe FFI.
- The source pin covers `include/mulib-mune.h` and `manual/mulib-mune.md`; the
  evidence pins cover `tests/test_mune_runtime.c`,
  `tests/test_mune_runtime_alloc.c` and `tools/check_mune_runtime.py`. All five
  files enter extraction provenance and archive locks, with negative drift tests.
- The full recipe explicitly opts into `MULIB_IMPL_PARSER`, deduplicated with
  other selected profiles, without widening standalone core Parsec. Real
  Exotract discovery/build reports **52 archive exports, zero declaration-only
  Mune APIs, and one `mulib.o` implementation member**.
- `tests/test_contracts.py` passes with GCC and with Clang ASan/UBSan;
  `tests/test_contracts_sdk.py --extract` passes real libclang discovery,
  pin/check/recheck, deterministic snapshots, denied unsafe promotion and drift
  tests for all five profiles.
- The complete `tests/test_exotract.py` suite passes with GCC and Clang ASan/UBSan,
  including Mune's separate-TU C and generated On1x consumers, existing
  core/MicroJS/ElviScrape/YANTLR/Shiraz/stdlib-bridge consumers and rollback tests.
  Mune acceptance exercises tree/VM/JIT library execution, rooted binary strings,
  native userdata/context ownership through coroutine callbacks, retained chunks,
  bundle validation and unchanged outputs on errors.

Artifacts are under `/tmp/opencode/on1x-mune-exotract-tests/mune` and the
corresponding `mune-app` executable. The source/SDK test directories are
`/tmp/opencode/on1x-mune-contract-tests{,-sanitize}` and
`/tmp/opencode/on1x-mune-sdk-tests`; the independently regenerated/rechecked
Mune contract snapshot is `/tmp/opencode/on1x-mune-rebound-contracts`. The GCC
archive matrix is `/tmp/opencode/on1x-mune-exotract-tests-gcc`.
Reproduce the archive suite with:

```sh
python3 on1x/tests/test_exotract.py \
  /tmp/opencode/on1x-final-gcc/libstone-moonstone/exotract/exotract \
  /tmp/opencode/on1x-loader-gcc/on1xc "$PWD/on1x" \
  /tmp/opencode/on1x-mune-exotract-tests-new clang \
  --cflag=-g --cflag=-fsanitize=address,undefined \
  --cflag=-fno-omit-frame-pointer --cflag=-fno-sanitize-recover=all
```

The broader `tests/test_mulib.py` remains blocked at its existing line-30
inventory assertion: `library/elvi-scrape` is absent from the fixture registry.
This rebinding does not repair or suppress that unrelated failure (also recorded
in the Syren continuation below).

## Syren diagnostics and repair continuation

The optional `ON1X_WITH_SYREN=ON` compiler adapter is implemented and tested;
see [diagnostics.md](docs/diagnostics.md). No grammar or runtime changes, SDK
patches, or new stdlib modules are part of this continuation. The actual supplied
LibStone revision is `55755485acaded7c4ed804c0946bc8c7363f6763`, with a clean SDK
working tree.

- `/tmp/opencode/on1x-syren`: GCC 16.2.1 Debug, generated scanner,
  `ON1X_BUILD_NATIVE_PPX=OFF`; **37/38 CTests pass**. The sole failure is
  `on1x.mulib`, `tests/test_mulib.py:30`, the existing fixture-inventory assertion.
  It also fails at the identical assertion in `/tmp/opencode/on1x-no-syren`
  (Syren OFF, fallback scanner); it is not repaired by this compiler-tooling task.
- `/tmp/opencode/on1x-syren-asan`: GCC Debug, fallback scanner, ASan/UBSan,
  non-PIE compiler and generated C11 executables; **35/35 selected CTests pass**
  with `ctest -j 8 -E '^on1x.(mulib|install)$' --output-on-failure`.
  Inventory and installation were excluded from this sanitizer run.
- Both profiles pass `on1x.diagnostics_api` and `on1x.diagnostics`, including
  1,000 deterministic malformed inputs, Unicode/invalid UTF-8 reporting,
  source edits/statuses, whole-import-graph checking, PPX refusal and
  overwrite/symlink protection. A repaired proposal is C11-compiled and executed
  (exit value 42), with sanitizer flags in the sanitizer profile.
- All **36** `examples/**/*.on1x` inputs have identical exit codes and emitted C
  with the generated-scanner Syren build and fallback-scanner non-Syren build.
- `src/diagnostics.cpp` passes an independent
  `-std=c++20 -Wall -Wextra -Wpedantic -Werror -fsyntax-only` check.

These are compiler-valid **proposals**, not proofs of intended behavior, safe FFI
admission, runtime error recovery or automatic adoption of an edited program.
Normal compilation still rejects invalid input; repair writes only a new file.

## Standard-library and native PPX extension integration

**PASS: 107/107 final CTests, with no failures, disabled or skipped tests.**

| Build under `/tmp/opencode/` | Configuration | Final suite |
|---|---|---|
| `on1x-loader-gcc` | GCC 16.2.1, generated scanner | 36/36 |
| `on1x-loader-clang` | Clang 22.1.8, fallback scanner | 35/35 |
| `on1x-loader-sanitize` | Clang 22.1.8, generated scanner, ASan/UBSan | 36/36 |

All three are Debug builds using `-Wall -Wextra -Wpedantic -Werror`. Final JUnit
evidence is `results-extensions-final.xml` in each directory. Initial
`results-extensions.xml` exposed a Clang-only unused helper in the status-only
bridge configuration. Conditional compilation was fixed without suppressing
warnings, then **all three complete suites were rerun**.

### Executed coverage

- Compiler admission of **118 library modules**, six On1x library programs,
  existing native Basis regressions and the new pure/Parsec examples. The Basis
  catalog remains 186 entries / 60 partial bindings; status was not promoted.
- Checked S-expression Option/Result tags, callback short-circuiting, map
  lookup/removal, graph shape rejection and deduplicated cyclic BFS with bounded
  edge/membership work; signed scanning including `INT64_MIN`, rollback on
  overflow, bounds and snapshot-free repeated byte searches.
- The scalar bridge calls **canonical MuLIB** JSON/YAML/TOML/INI, BLAKE2s/BLAKE3
  and POSIX process helpers, with automatic adapter/kernel linkage and one
  kernel implementation TU. Status tuples distinguish invalid data, missing and
  empty INI values, and whole-document rejection after a matching key.
- Counted adapter allocation failures, temporary-copy cleanup, pool exhaustion
  and process-exit cleanup. Each of seven bridge groups and the status-only
  configuration independently compiles under strict flags.
- A local Python TCP server and On1x numeric-IPv4 client exercise offset sends,
  binary hex reception with NUL/0xff, text-NUL rejection, EOF, invalid offsets,
  double close and stale handles after slot reuse. No external network service.
- All **15 native PPXs** build/install and execute compiler-generated C11.
  Newly supplied assertions/cond/format/logging/state_machine/tap/trace have
  Python/native behavior comparisons, operand-order and compatibility-form
  tests, malformed carriers, duplicate transitions and no-output protocol errors.
  A six-processor example runs through both transports.
- All **24 Python PPXs**, **28 Python protocol tests**, strict raw-FFI manifest
  rejection and existing type/resource/staging/macro/MIR/scanner regressions.
- Installation/prefix relocation executes the installed INI consumer against an
  external MuLIB root, in addition to Basis, PPX and `std.mulib` clients.
- Real Exotract/ExoML archive tests pass with GCC, Clang and ASan/UBSan:
  JSON/YAML/TOML/INI/cryptohash/socket discovery/build and an On1x JSON/INI/hash
  bridge consumer without manual adapter selection. Core/MicroJS/YANTLR/Shiraz,
  artifact drift, ABI mismatch and rollback regressions remain enabled.

### Identity and artifacts

MuLIB HEAD: `e67f842f9211f6bbabf35cea07e6c55d66045e52` plus current uncommitted
changes. The supplied read-only LibStone is clean at
`55755485acaded7c4ed804c0946bc8c7363f6763`; it was not updated or patched.
Optional SDK integrations are OFF in the full loader matrix and tested separately
using existing real Exotract executables from
`on1x-final-{gcc,clang-fallback,sanitize}` and the rebuilt loader-matrix compilers.
Archive logs and artifacts are `/tmp/opencode/on1x-extensions-exotract-{gcc,clang,sanitize}.log`
and matching directories. System libclang itself is not instrumented.

### Reproduction

From the MuLIB root:

```sh
cmake -S on1x -B /tmp/opencode/on1x-extensions-new -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'
cmake --build /tmp/opencode/on1x-extensions-new --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-extensions-new \
  --output-on-failure --parallel 3 --output-junit /tmp/opencode/on1x-extensions-new/results.xml
TMPDIR=/tmp/opencode python3 on1x/tests/test_exotract.py \
  /tmp/opencode/on1x-final-gcc/libstone-moonstone/exotract/exotract \
  /tmp/opencode/on1x-loader-gcc/on1xc "$PWD/on1x" \
  /tmp/opencode/on1x-extensions-exotract-gcc gcc
```

For Clang fallback use `clang`/`clang++` and `-DON1X_USE_FALLBACK_SCANNER=ON` in
a separate directory. For sanitizer coverage, use Clang generated scanning and
append `-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all`
to both C/C++ flag strings and set
`-DON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer;-fno-sanitize-recover=all`
as one quoted CMake argument. Sanitized archive tests additionally receive
`--cflag=-g --cflag=-fsanitize=address,undefined --cflag=-fno-omit-frame-pointer
--cflag=-fno-sanitize-recover=all`. For a new real extraction SDK build, follow
[`contracts-exotract/README.md`](contracts-exotract/README.md).

These remain trusted raw scalar adapters and bounded syntactic transforms,
**not** independent ExoML review or generalized safe FFI. No f64/record/slice ABI,
foreign-owner lowering, native ADTs, full SML/Base/Batteries conformance, TLS
socket API or completed Shylock backend is claimed. Operation contracts and
examples: [`stdlib/README.md`](stdlib/README.md),
[`stdlib/_bridge/README.md`](stdlib/_bridge/README.md),
[`stdppx/README.md`](stdppx/README.md).

## Earlier artifact-only verification (historical)

The original extension artifact reported the following before local compiler
integration. These statements retain their historical scope, not current status:

- GitHub source/API read of current `Chubek/mulib` plus `janestreet/base/src` and `ocaml-batteries-team/batteries-included/src` directories.
- C bridge tested with GCC and Clang using `-std=c11 -Wall -Wextra -Werror -pedantic -fsyntax-only` **against signature-matching local mock headers**. This proves only C syntax and basic type compatibility of the bridge under these mocks.
- The On1x compiler was not executable locally; no native On1x unit tests or PPX binaries were compiled/run.
- Actual MuLIB archive generation, linkage and ABI/contract validation are pending.
- No claimed complete Base/Batteries parity, no crypto/security certification, and no generalized safe FFI.
