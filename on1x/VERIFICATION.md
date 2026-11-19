# On1x verification — through 2026-11-19

## Restricted multistage lexical quotation regions — 2026-11-19

This increment is **EXECUTABLE**, not a claim that arbitrary runtime code
generation or the whole compiler is verified. Starting from MuLIB
`6a145cbf09efcac76f387f6b664fc2257bac93aa`, the canonical parser now accepts
typed lexical regions such as
`quote { let sampled = lift(input); sampled + 2 }`. They desugar to an
immediately invoked result-inferred lambda inside the existing `Quote` node,
then use the existing checked AOT factory/thunk and strict C11 path. There is no
new evaluator, JIT, compiler subprocess, runtime ABI, or Unit-v1 PPX
constructor.

The measured host was Linux x86-64 with GCC/G++ 16.2.1, Clang/Clang++ 22.1.8,
Python 3.14.7, CMake 4.4.3, and read-only LibStone
`3399047f6c6328b8f07788795c610699e66c060a`. Builds remained outside the
worktree.

| Check | Result |
|---|---|
| GCC Debug, LibStone generated scanner, staging/cross-feature selection including scanner parity | **12/12 passed** |
| GCC Debug, bootstrap fallback scanner, corresponding selection | **10/10 passed** |
| Clang Debug, fallback scanner, compiler and emitted C/runtime under ASan+UBSan | **10/10 passed**, no sanitizer findings |
| Generated-scanner complete inventory | **105/108 passed** initially; timing-sensitive `on1x.concurrency` passed alone, leaving two unrelated baseline inventory failures |
| Fallback-scanner complete inventory | **104/107 passed** initially; timing-sensitive `on1x.concurrency` passed alone, leaving the same two unrelated baseline inventory failures |
| Embedded-unit consistency and diff whitespace | **PASS** |

The two persistent full-inventory failures are outside this change.
`on1x.contracts` sees 262 auxiliary MuLIB headers where its stale assertion
expects exactly three, and `on1x.mulib` does not list the baseline
`library/muw3lib` directory as a fixture. No contract, library inventory, or
concurrency source was changed here. The failed parallel process-stream case
(`captured_stdin_epipe`) passed in an immediate isolated rerun under each GCC
scanner profile.

The focused suites execute real generated C11 and cover statement/tail and
empty/void regions, explicit staged returns, mutable locals, loops and matches,
defer/resource cleanup, nested stages, lift/splice timing, quotation-world
extrusion, exact diagnostic provenance, macro hygiene, imports, PPX identity
round trips, deterministic emission, scanner parity, depth limits, and
transactional HIR failure.

Reproduction:

```sh
worktree=/mnt/nvme/opencode-worktrees/ses_e0d13d7a9ffe4DEJxtuNoZl9ei/multistage
build=/mnt/nvme/opencode-builds/ses_e0d13d7a9ffe4DEJxtuNoZl9ei/multistage

cmake --build "$build/gcc-generated" --parallel 4
ctest --test-dir "$build/gcc-generated" --output-on-failure --parallel 4
ctest --test-dir "$build/gcc-generated" --output-on-failure --parallel 3 \
  -R '^on1x\.(scanner|scanner_parity|meta_hir|value_loop_hir|defer_error_hir|macros|ppx|staging|local_syntax|modules|value_loops|defer_errors)$'
ctest --test-dir "$build/gcc-generated" --output-on-failure \
  -R '^on1x\.concurrency$'

cmake --build "$build/gcc-fallback" --parallel 4
ctest --test-dir "$build/gcc-fallback" --output-on-failure --parallel 4
ctest --test-dir "$build/gcc-fallback" --output-on-failure --parallel 3 \
  -R '^on1x\.(meta_hir|value_loop_hir|defer_error_hir|macros|ppx|staging|local_syntax|modules|value_loops|defer_errors)$'
ctest --test-dir "$build/gcc-fallback" --output-on-failure \
  -R '^on1x\.concurrency$'

cmake -S "$worktree/on1x" -B "$build/clang-fallback-san" \
  -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DON1X_USE_FALLBACK_SCANNER=ON \
  -DON1X_LIBSTONE_ROOT=/mnt/nvme/mulib/on1x/third_party/libstone \
  -DON1X_BUILD_DYNAMIC_SDK_PLUGINS=OFF -DON1X_USE_YELLOWSTONE_LINK=OFF \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all' \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all' \
  '-DON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer;-fno-sanitize-recover=all'
cmake --build "$build/clang-fallback-san" --parallel 3 \
  --target on1xc on1x_meta_tests on1x_value_loop_tests on1x_defer_error_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir "$build/clang-fallback-san" --output-on-failure --parallel 2 \
  -R '^on1x\.(meta_hir|value_loop_hir|defer_error_hir|macros|ppx|staging|local_syntax|modules|value_loops|defer_errors)$'

cd "$worktree"
python3 tools/embed_units.py --check
git diff --check
```

## Move-only Result ownership lifecycle — 2026-11-19

Validated detached MuLIB baseline
`6a145cbf09efcac76f387f6b664fc2257bac93aa` plus this continuation.
The generated-scanner build used the read-only LibStone checkout
`3399047f6c6328b8f07788795c610699e66c060a` at
`/mnt/nvme/mulib/on1x/third_party/libstone`. Host: Linux
`6.18.52-1-lts` x86-64, GCC/G++ 16.2.1 (`20260810`),
Clang/Clang++ 22.1.8, CMake 4.4.3 and Python 3.14.7. All build and
test artifacts were kept under
`/mnt/nvme/opencode-builds/ses_e0d13d7a9ffe4DEJxtuNoZl9ei/resource-ownership`;
the LibStone checkout remained clean.

This is a bounded extension of the existing finite `own<Named>` profile, not a
general resource or foreign-handle model. Canonical postfix `?` now consumes a
direct local move-only `Result`: the complete input, selected `Ok`/`Err`
projection, and region result carry independently checked transfer metadata.
The C11 backend therefore clears each transferred source and retains exactly
one active owner across either continuation or early return. Missing any of
those three transfer classes is rejected by `canonical::verify_values`.

The lifecycle suite covers:

- the pervasive nominal `Result` with owning success and error payloads, plus a
  distinct local Result declaration in checked-HIR transfer tests;
- successful transfer/take/drop, one- and two-owner partial acquisition,
  propagated `Err`, defer-before-lexical-cleanup order, and zero live owners
  after each path;
- implicit copy, second propagation/use-after-move, generic and shipped
  pervasive-helper copies, raw foreign owner-bearing parameter/result ABIs,
  owner-bearing defer capture, staging, concurrency, MIR and PPX rejection;
- unique allocation failure at every constructor allocation point. Exit 72 is
  retained as terminal OOM, and the allocator's exit verifier requires all
  previously acquired owners to have been drained;
- strict `-std=c11 -Wall -Wextra -Wpedantic -Werror` generated C plus strict
  C11 and C++17 separate translation-unit callers of the same generated
  internal interface; and
- malformed-HIR transfer-marker mutations, a second take through a cleared
  internal owner slot, ABI-version mismatch, and existing 50,001/20,000-node
  iterative destruction stress.

The boundary callers deliberately rebuild generated C and the runtime together.
They do **not** establish a stable aggregate foreign ABI or promote arbitrary
pointers/scalar handles to a checked owner.

### Exact results

| Profile | Result |
|---|---|
| GCC 16.2.1 Release, LibStone generated scanner, final ownership/defer selection | **4/4 passed** |
| GCC 16.2.1 Debug, fallback scanner, final ownership/defer selection | **4/4 passed** |
| Clang 22.1.8 Debug, fallback scanner, compiler/runtime/generated C with ASan+UBSan, final cross-feature selection | **12/12 passed**, no sanitizer findings |
| Complete generated-scanner On1x inventory | **95 passed, 2 failed, 1 skipped of 98**; both failures are stale baseline inventories, not ownership failures |
| Complete fallback-scanner On1x inventory | **90 passed, 3 failed, 1 skipped of 94**; the ownership tests and concurrency passed |
| Repository-wide GCC Debug integration inventory | **707/709 passed**; two pre-existing generated inventories are stale |
| `git diff --check` and embedded-unit consistency | **passed** in the final tree |

The complete generated run's stable failures were:

- `on1x.contracts`: the baseline assertion still permits only three auxiliary
  MuLIB headers while the baseline tree has 259 additional unregistered
  auxiliary headers; and
- `on1x.mulib`: baseline `library/muw3lib` is not present in the embedded
  fixture catalog.

Neither failing test nor any implicated `include/`, `library/`, manifest, or
catalog path differs from baseline in this change. The root integration run
reported the same pre-existing registration drift as
`mulib_dependency_strata` and `mulib_mull_inventory`. These failures are
preserved rather than hidden, skipped, or repaired outside this task.

The fallback complete run additionally had the expected
`on1x.type_checker_hardening_hir` failure because that bootstrap configuration
has no Firestone regex SDK. An earlier parallel run's
`on1x.concurrency` captured-stdin assertion failed once, then passed both an
isolated rerun **1/1** and the refreshed complete inventory.
`on1x.category` skipped in both complete configurations because native PPX was
disabled.

Final affected and sanitizer commands:

```sh
base=/mnt/nvme/opencode-builds/ses_e0d13d7a9ffe4DEJxtuNoZl9ei/resource-ownership

cmake --build "$base/gcc-generated" --parallel 2 --target \
  on1xc on1x_recursive_owning_adts_tests on1x_defer_error_tests
ctest --test-dir "$base/gcc-generated" --output-on-failure \
  -R '^on1x\.(recursive_owning_adts(_hir)?|defer_error_hir|defer_errors)$'

cmake --build "$base/gcc" --parallel 2 --target \
  on1xc on1x_recursive_owning_adts_tests on1x_defer_error_tests
ctest --test-dir "$base/gcc" --output-on-failure \
  -R '^on1x\.(recursive_owning_adts(_hir)?|defer_error_hir|defer_errors)$'

cmake --build "$base/clang-san" --parallel 2 --target \
  on1xc on1x_recursive_owning_adts_tests on1x_defer_error_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:strict_string_checks=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir "$base/clang-san" --output-on-failure --parallel 2 \
  -R '^on1x\.(compiler|functions|patterns|value_loops|nominal_adts|owning_adt|recursive_owning_adts(_hir)?|defer_error_hir|defer_errors|prelude|modules)$'
```

The Clang tree uses
`-fsanitize=address,undefined -fno-omit-frame-pointer
-fno-sanitize-recover=all` for C/C++ compiler targets and the same flags through
`ON1X_TEST_C_FLAGS` for generated programs and their runtime.

Complete inventory commands:

```sh
ctest --test-dir "$base/gcc-generated" --output-on-failure --parallel 4 \
  --output-junit "$base/gcc-generated/results-resource-ownership.xml"
ctest --test-dir "$base/gcc" --output-on-failure --parallel 4 \
  --output-junit "$base/gcc/results-resource-ownership.xml"

cmake -S . -B "$base/repo-gcc" -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=gcc
cmake --build "$base/repo-gcc" --parallel 4
ctest --test-dir "$base/repo-gcc" --output-on-failure --parallel 4 \
  --output-junit "$base/repo-gcc/results-resource-ownership.xml"
```

Remaining closed boundaries are checked scalar/foreign owners and stale-handle
generation validation, general borrows, callbacks, user destructors,
affinity/reentrancy effects, ownership-constrained generic helpers,
owner-bearing deferred arguments, concurrency ownership and `Send`/`Sync`,
PPX/interface transport, and typed ownership/cleanup MIR. Allocation failure
remains terminal and no budget is reinterpreted as a timeout.

## Function defer and owned Result closeout — 2026-11-19

Validated MuLIB
`6a145cbf09efcac76f387f6b664fc2257bac93aa` plus this continuation, with the
read-only configured LibStone
`3399047f6c6328b8f07788795c610699e66c060a`, Linux
6.18.52-1-lts x86-64, GCC/G++ 16.2.1 (`20260810`), Clang/Clang++ 22.1.8,
CMake 4.4.3 and Python 3.14.7.

This closes the direct-C11 ownership gap in the already executable
[function-defer/typed-error profile](docs/defer-errors.md). Postfix `?` now
uses checked destructive payload projections for either active arm of a
move-only nominal `Result<T,E>`, then transfers the successful result slot.
The projection clears its source before residual sum destruction. Postfix `?`
is an explicit consuming operation for a named move-only Result; a second read
reports a located use-after-move error. Immutable shared `sexpr` payloads
retain/release through the existing generated aggregate helpers. No runtime
ABI, foreign admission, exception model or MIR representation changed.

Executable tests cover:

- shared success/error propagation, deferred aggregate captures, discarded
  managed deferred results, abandoned operands and zero final RC live nodes;
- finite `own<Named>` success and error transfer, consuming named-local use,
  zero internal/fixture live-owner counters, and abort-on-second-free fixtures;
- every owning-box allocation failure in the two-path program, with terminal
  exit 72, the original located allocation diagnostic and no sanitizer finding;
- malformed HIR with removed destructive-projection metadata, plus located
  source rejection for an implicit unique copy, JSON Result payloads, unique
  deferred arguments and unique values in cooperative concurrency;
- cooperative cancellation through nested shared-Result propagation frames:
  function defers drain exactly once in `2, 3, 1` order and the shared-node
  counter reaches zero before `cancel` returns; and
- the existing registration-time snapshot, LIFO, propagated-error, fault,
  budget, C/C++ caller, generated/fallback scanner and O0/O2 seeded oracles.

The emitted C11 for both owning arms was inspected. It contains ordinary tag
tests, payload loads, `memset` source clearing, explicit error returns and
`on1x_defer_run`; there is no `longjmp`, C++ unwinding or erased owner transfer.
A deferred call returning `Err` still cannot replace the pending outcome.
Fatal traps and cleanup/allocation failures remain terminal rather than
recoverable typed errors.

### Current measured results

| Profile | Result |
|---|---|
| GCC, LibStone-generated scanner, strict compiler/emitted-C11 warnings, focused defer/Result/cancellation/scanner selection | **4/4 passed** |
| Clang, fallback scanner, strict compiler/emitted-C11 warnings, focused defer/Result/cancellation selection | **3/3 passed** |
| GCC, generated scanner, compiler plus emitted C/runtime under ASan/UBSan, same focused selection | **4/4 passed**, no sanitizer findings |
| Clang, fallback scanner, compiler plus emitted C/runtime under ASan/UBSan, same focused selection | **3/3 passed**, no sanitizer findings |
| CDD GCC, Clang, GCC ASan/UBSan and Clang ASan/UBSan profiles | **PASS** in all four; each retained the 500 seeded + 24,576 exhaustive oracle cases and allocation-failure test |
| Broader GCC/generated inventory excluding the two known base-catalogue checks | **86 passed, 1 configured Category skip, 0 failed** (87 total) |
| Root MuLIB strict GCC build and 709-test inventory | Build **PASS**; **707/709 passed**, with two untouched stale-inventory checks failing; the exclusion rerun passed **707/707** |

Expected terminal-failure subprocesses disable leak reporting only for that
subprocess; address/undefined-behavior checks remain active. The enclosing
sanitizer CTests run with `ASAN_OPTIONS=detect_leaks=1`.

The complete generated-scanner inventory was also attempted: 85 passed, three
failed and one configured Category test skipped out of 89. The native self-host
PPX failure was a 5-second timeout under parallel load and passed immediately
when rerun serially. The remaining two failures reproduce without this patch:
base commit `6a145cb` contains hundreds of new `mulib-*.h` files not represented
by the On1x contract inventory and `library/muw3lib` is absent from the fixture
catalogue. Excluding only those two checks, a clean rerun passed all 86 executed
tests with the configured Category skip.

The broader strict Clang/fallback run, excluding the same two catalogue checks,
passed 82 tests, failed only the pre-existing hardening fixture that requires
the unavailable Firestone SDK, and had the same configured Category skip.
Every defer/error, cancellation, ownership and scanner test passed in both
broader attempts.

The repository-root GCC build completed every target. Its full 709-test
inventory passed 707 tests; only `mulib_dependency_strata` (stale generated
strata) and `mulib_mull_inventory` (the same unregistered base headers) failed.
Neither check reads a file changed by this continuation. Excluding those two
checks, all **707/707** root tests passed.

Reproduction from the repository root (build trees are external):

```sh
base=/mnt/nvme/opencode-builds/ses_e0d13d7a9ffe4DEJxtuNoZl9ei/defer-errors
focus='^on1x\.(defer_error_hir|defer_errors|cancellation_deadlines|scanner_parity)$'
ctest --test-dir "$base/gcc-generated" --output-on-failure -R "$focus"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 \
  ctest --test-dir "$base/gcc-sanitize" --output-on-failure -R "$focus"

focus='^on1x\.(defer_error_hir|defer_errors|cancellation_deadlines)$'
ctest --test-dir "$base/clang-fallback" --output-on-failure -R "$focus"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 \
  ctest --test-dir "$base/clang-sanitize" --output-on-failure -R "$focus"

ctest --test-dir "$base/gcc-generated" --output-on-failure --parallel 2 \
  -E '^on1x\.(contracts|mulib)$'
ctest --test-dir "$base/root-gcc" --output-on-failure --parallel 4
ctest --test-dir "$base/root-gcc" --output-on-failure --parallel 4 \
  -E '^mulib_(dependency_strata|mull_inventory)$'

TMPDIR=/tmp/opencode CC=gcc CXX=g++ sh tests/run-cdd.sh
TMPDIR=/tmp/opencode CC=clang CXX=clang++ sh tests/run-cdd.sh
TMPDIR=/tmp/opencode CC=gcc CXX=g++ \
  CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  CXXFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS='-fsanitize=address,undefined' sh tests/run-cdd.sh
TMPDIR=/tmp/opencode CC=clang CXX=clang++ \
  CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  CXXFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS='-fsanitize=address,undefined' sh tests/run-cdd.sh
```

The supported boundary remains deliberately finite: JSON owner/view and
concurrency-capability Result fields, unique deferred arguments/captures,
unique values crossing cooperative tasks, Option propagation, general foreign
errors/resources, typed cancellation outcomes, exceptions, trap unwinding,
aggregate ownership MIR/SDK models and other targets remain closed.
## Shared/exclusive borrow checking — 2026-11-19

Validated from detached MuLIB baseline
`6a145cbf09efcac76f387f6b664fc2257bac93aa` plus the borrow-checking worktree,
using configured external LibStone
`3399047f6c6328b8f07788795c610699e66c060a`. The host profile is Linux x86-64,
GCC/G++ 16.2.1, Clang/Clang++ 22.1.8, Python 3.14.7, and CMake 4.4.3. The
required build is Debug with the generated scanner, Chisel, and Novostone.
Nerotypical is disabled because this pinned Satie checkout does not provide the
prepared public EUF target; neither LibStone nor Satie was modified.

The implemented profile is **EXECUTABLE HIR-to-C11**, not a general pointer,
foreign-effect, concurrency, or independent Chisel/Nerotypical proof. Borrow
modes remain non-first-class metadata. Shared `borrow T`, exclusive `ref T`,
lexical `borrow [mut]`, nested reborrows, branch/loop scope ends, and direct
generic/module calls are checked against canonical move/partial-move state.
`own<Named>` lends its nominal payload through a size-checked runtime accessor;
the owner cannot move, take, or drop while loaned. Capture, callbacks, defer,
PPX/interfaces, raw FFI modes, MIR, structural adapters, and cooperative
suspension fail closed.

Focused reproduction:

```sh
cmake --build \
  /mnt/nvme/opencode-builds/ses_e0d13d7a9ffe4DEJxtuNoZl9ei/borrow-checking \
  --target on1xc on1x_core_ref_tests --parallel 4
ctest --test-dir \
  /mnt/nvme/opencode-builds/ses_e0d13d7a9ffe4DEJxtuNoZl9ei/borrow-checking \
  --output-on-failure \
  -R '^on1x\.(core_ref(_hir)?|scanner_parity)$'
```

| Check | Result |
|---|---|
| `on1x.core_ref_hir` malformed checked-HIR modes/origins/types/stores/captures and deterministic C11 | **PASS** |
| `on1x.core_ref` accepted/rejected source, joins, nested reborrows, owner move/drop invalidation, modules/generics, escapes and diagnostics | **PASS** |
| Generated owner-payload C11 under strict GCC and Clang (`-std=c11 -Wall -Wextra -Wpedantic -Werror`) | **PASS** |
| Generated C11/runtime under ASan+UBSan+LSan | **PASS**, no sanitizer findings |
| `on1x_unique_borrow` healthy/null/taken/size-mismatch runtime probes | **PASS**; malformed calls terminate with located ownership error/status 72 |
| Separate canonical loan-work exhaustion (`ON1X-BORROW-LIMIT`) | **PASS**, deterministic located rejection |
| Focused CTest selection | **3/3 passed** |
| Generated-scanner full build | **PASS**, 100% of configured targets built |
| Complete configured CTest inventory | **110/112 passed**; all borrow, ownership, scanner, safety, backend, SDK, and integration tests passed |

The Python test compiles and executes an owner-payload program with both
available production C compilers, then independently instruments nested-owner
replacement and cleanup outside the C++ compiler build. It also asserts that
failed compilation leaves an existing output untouched. The C++ test corrupts
already checked HIR before both canonical verification and C11 emission. These
checks do not convert the raw C accessor into a temporal-safety API; temporal
validity is established by the On1x loan verifier.

The two complete-inventory failures are unchanged baseline inventory assertions,
not borrow regressions. `on1x.contracts` expects exactly three auxiliary MuLIB
headers, while baseline `6a145cbf09efcac76f387f6b664fc2257bac93aa`
already tracks 259 additional unregistered headers. `on1x.mulib` likewise
rejects baseline-tracked `library/muw3lib`, which is absent from its fixture
inventory. This worktree changes none of `include/`, `library/`,
`on1x/contracts-exotract/`, or `on1x/stdlib/mulib/`.
## On1x extension SDK

SDK ABI 1 was tested on Linux x86-64 at MuLIB
`b9790059fb89ada683f05081239222d92aa93d53` plus SDK worktree changes, with
LibStone `934b6ed029bfea28bbf23b5dc77c476154b0dd0d`.

```sh
fixture="$PWD/on1x/sdk/tests/linked_extension.c"
cmake -S on1x -B build/on1x-sdk-gcc-make -DBUILD_TESTING=ON \
  -DON1X_REQUIRE_LIBSTONE=ON -DCMAKE_BUILD_TYPE=Release \
  -DON1X_USE_YELLOWSTONE_LINK=OFF \
  -DON1X_LINKED_SDK_EXTENSION="$fixture"
cmake --build build/on1x-sdk-gcc-make --parallel 4
ctest --test-dir build/on1x-sdk-gcc-make --parallel 4 --output-on-failure

cmake -S on1x -B build/on1x-sdk-clang-san -DBUILD_TESTING=ON \
  -DON1X_USE_FALLBACK_SCANNER=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DON1X_BUILD_DYNAMIC_SDK_PLUGINS=OFF -DON1X_USE_YELLOWSTONE_LINK=OFF \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DON1X_LINKED_SDK_EXTENSION="$fixture"
cmake --build build/on1x-sdk-clang-san --parallel 3 \
  --target on1xc on1x_sdk_test_registry on1x_sdk_test_analyzer \
  on1x_sdk_test_backend on1x_sdk_test_security on1x_sdk_test_c_abi
ctest --test-dir build/on1x-sdk-clang-san --output-on-failure -R '^on1x\.sdk\.'
```

The GCC 16.2.1 Release build with the generated scanner passed the full
**105/105** suite; the SDK-only rerun passed **7/7** after the short-descriptor
validation change. Clang 22.1.8 with the fallback scanner and ASan/UBSan
passed the focused SDK suite **7/7**, with no sanitizer findings.

The seven SDK tests cover registry transactions, bounded sinks and ABI
validation, C11 linkage from an installed and relocated package, and an
explicitly linked C provider consuming compiler-verified MIR. The provider
round-trips `examples/hello.on1x` and produces the same executable output as
the direct C11 path. Bad source and malformed or unverifiable MIR never reach
the provider; hook failures leave existing output unchanged.

The original SDK evidence above is for linked, trusted native providers.
External providers now run in a separate Linux x86-64 worker (see
[SDK guide](sdk/README.md)). The test `on1x.sdk.dynamic_plugins` checks
colon-separated discovery, explicit selectors, duplicate rejection, real
loading and MIR handoff, and rejection of constructor file writes, network
socket creation, process creation, and unbounded CPU use. The worker builds
with `ON1X_BUILD_DYNAMIC_SDK_PLUGINS=ON`; disable it for instrumented SDK
builds (the sanitizer runtime is not inside the worker syscall profile).
The profile has not been independently audited. There is no public checked
HIR ABI, alternate native code backend, or safe FFI admission through SDK
metadata.

### SDK backend migration and Yellowstone manifest

The `on1x.backends.c11` and `on1x.backends.mir_c11` tests exercise the
checked compiler views, compare the new SDK artifact bytes to the canonical
emitters, and check rejection and diagnostic preservation. `on1x.mir`
continues to compare direct C11, MIR C11, and reference evaluation through
the driver.

On Linux x86-64, use the configuration above with
`-DON1X_USE_YELLOWSTONE_LINK=ON` for GCC (this checkout includes the extended
LibStone manifest API); `on1x.backends.yellowstone` checks generation and
rejects invalid manifests. The generated-scanner Release build passed
**109/109** with `TMPDIR` set to a disk-backed build directory. For the Clang
sanitizer build add
`-DON1X_BUILD_DYNAMIC_SDK_PLUGINS=OFF -DON1X_USE_YELLOWSTONE_LINK=OFF`,
also build `on1x_backend_test_c11` and `on1x_backend_test_mir_c11`, and run
`ctest --test-dir build/on1x-sdk-clang-san --output-on-failure -R '^on1x\.(sdk\.|backends\.)'`.
That fallback-scanner sanitizer run passed **9/9**.

The backend manifest is platform-independent, but non-Linux target linking
and execution were not exercised. The target C++ toolchain still links the
driver; no native qobjld C++ compiler-driver link is claimed.
In the LibStone worktree, the GCC Debug `qld-*` suite passed **7/7**.
Clang ASan/UBSan/LSan passed the three focused manifest and frontend tests.
See [`ld/VALIDATION.md`](third_party/libstone/yellowstone/ld/VALIDATION.md)
for the linker commands and coverage.

## Combined core and ChatGPT-addon closeout — 2026-11-19

Validated MuLIB
`bf23bd1282d280f1935b9ca1898fcb2ab19a7ca4` plus the closeout documentation
changes, with configured LibStone
`934b6ed029bfea28bbf23b5dc77c476154b0dd0d`, Linux x86-64,
GCC/G++ 16.2.1 and Clang/Clang++ 22.1.8.

The final shared source tree was configured and rebuilt directly on the NVMe
workspace so compiler temporaries and build paths had one canonical spelling.
This closeout includes the late `on1x.addon_chatgpt` CTest registration.

| Final check | Result |
|---|---|
| GCC 16.2.1, LibStone generated scanner, complete final inventory | **98/98 passed** |
| GCC 16.2.1, bootstrap fallback scanner, complete final inventory | **97/97 passed** |
| Standalone ChatGPT addon Python development-mode suite | **27/27 passed** |
| Focused Clang 22.1.8 ASan/UBSan cross-feature profile | **15/15 passed**, no sanitizer findings |
| CDD suite, GCC and Clang, normal and ASan/UBSan profiles | **PASS** in all four profiles |
| Existing configured root integration suite | **683/683 passed**; existing binaries, no full rebuild |
| Embedded-unit consistency and final diff whitespace checks | **PASS** |

The generated inventory has one additional test because it includes scanner
parity; both inventories include the addon. The addon remains a **PARTIAL**
hosted integration: its local read-only MCP behavior, packaging and relocation
are tested, but operator TLS/auth deployment and actual ChatGPT web
interoperability are not.

Reproduction from the repository root:

```sh
TMPDIR="$PWD/build/on1x-final-tmp" \
  cmake -S on1x -B build/on1x-final-generated-nvme -DBUILD_TESTING=ON
TMPDIR="$PWD/build/on1x-final-tmp" \
  cmake --build build/on1x-final-generated-nvme --parallel 3
TMPDIR="$PWD/build/on1x-final-tmp" \
  ctest --test-dir build/on1x-final-generated-nvme \
    --output-on-failure --parallel 4

TMPDIR="$PWD/build/on1x-final-tmp-fallback" \
  cmake -S on1x -B build/on1x-final-fallback -DBUILD_TESTING=ON \
    -DON1X_USE_FALLBACK_SCANNER=ON
TMPDIR="$PWD/build/on1x-final-tmp-fallback" \
  cmake --build build/on1x-final-fallback --parallel 3
TMPDIR="$PWD/build/on1x-final-tmp-fallback" \
  ctest --test-dir build/on1x-final-fallback \
    --output-on-failure --parallel 4

TMPDIR="$PWD/build" \
  python3 -B -X dev on1x/addons/chatgpt/test_chatgpt.py
python3 tools/embed_units.py --check
git diff --check
```

Two infrastructure artifacts were isolated without changing tests or source
semantics. An initial generated run exhausted the host `/tmp` tmpfs; its four
affected tests passed after redirecting `TMPDIR` to NVMe. Moving that configured
tree and accessing it through its old `/tmp/opencode` symlink later made six
exact-path assertions compare the symlink spelling with canonical NVMe paths.
All six passed in the fresh canonical NVMe build, followed by the complete
**98/98** run above.

## Final type-checker and cross-feature hardening — 2026-11-19

Validated MuLIB `b66da8f5150adca2a3416db747d91bf81a3fe138` plus
the existing shared worktree, with configured LibStone
`934b6ed029bfea28bbf23b5dc77c476154b0dd0d`, Linux x86-64,
GCC/G++ 16.2.1 (`20260810`) and Clang/Clang++ 22.1.8.
Work continued under GPT-6.1 Sol without restarting or discarding prior edits.

The final audit reproduced three missing malformed-HIR checks before repair:
forged transfer metadata on an intrinsic callee, and synchronized renaming of
retained/generated cliche and inherent member identities. The regressions in
`test_type_checker_hardening.cpp`, `test_cliche.cpp`, and `test_core_impl.cpp`
failed against the prior implementation. Canonical verification now validates
transfer kind/type/local provenance before prototype/callee shortcuts, including
retained local generic sources. Cliche identities are reconstructed from the
same deterministic implementation ordering used during preparation. Loaded
inherent names now use flattened-unit implementation indices, permitting exact
identity checks while retaining the `@inherent_` private-name family.
No ownership syntax, operation, runtime ABI or admitted backend was added.

| Final configuration | Result |
|---|---|
| GCC/generated scanner, complete final suite | **98/98 passed** |
| GCC/fallback scanner, complete final suite | **97/97 passed** |
| Clang/generated ASan+UBSan, compiler and emitted C11/runtime, focused cross-feature selection | **15/15 passed**, no sanitizer findings |
| Combined ownership/shared ownership, cliche, inherent and hardening selection in each of the three profiles | **9/9 passed** |
| Standalone ChatGPT addon suite | **27/27 passed** |
| `python3 tools/embed_units.py --check` | **PASS**, embedded profiles current |

The focused selection retains both `on1x.cliche_hir` and
`on1x.core_impl_hir`. It checks generated signatures/binders/constraints,
unresolved executable projections, ownership transfers and fail-closed C11/MIR
admission. Additional source probes exercised branch restoration, loop-backedge
restoration, break joins, consuming record patterns, and rejection of a whole
value after a consuming match. These probes found no move-flow regression.

Reproduction (existing configured build trees):

```sh
cmake --build /mnt/nvme/on1x-hardening-generated-gcc --parallel 4
ctest --test-dir /mnt/nvme/on1x-hardening-generated-gcc --output-on-failure -j 4
cmake --build build/core-literals-fallback --parallel 4
ctest --test-dir build/core-literals-fallback --output-on-failure -j 4

cmake --build /mnt/nvme/on1x-hardening-generated-clang-asan --parallel 2 \
  --target on1xc on1x_type_checker_hardening_tests \
  on1x_recursive_owning_adts_tests on1x_cliche_tests on1x_core_impl_tests \
  on1x_nominal_tests on1x_pattern_tests on1x_value_loop_tests
ctest --test-dir /mnt/nvme/on1x-hardening-generated-clang-asan \
  --output-on-failure \
  -R '^on1x\.(owning_adt|recursive_owning_adts(_hir)?|type_checker_hardening(_hir)?|cliche(_hir)?|core_impl(_hir)?|nominal(_hir|_adts)|patterns?|pattern_hir|value_loop(s|_hir))$'
python3 tools/embed_units.py --check
```

The sanitizer tree uses strict warnings-as-errors, `-fsanitize=address,undefined`,
`-fno-omit-frame-pointer` and `-fno-sanitize-recover=all` for the compiler;
`ON1X_TEST_C_FLAGS` instruments emitted C/runtime too.
This is not a new full-profile sanitizer certification: the previously observed
64 MiB project/install executable-size limit, depth-254 `on1x.limits` sanitizer
stack overflow, and 180-second Category timeout remain known limitations and
were not repaired by this task. Unique ownership remains direct checked-HIR to
C11 only; scalar MIR still rejects it pending typed move/drop/cleanup events.
Earlier per-profile totals below are preserved as historical evidence.

## Finite-owner Result propagation and iterative consumption — 2026-11-19

Baseline MuLIB
`6a145cbf09efcac76f387f6b664fc2257bac93aa` plus this continuation;
configured LibStone
`3399047f6c6328b8f07788795c610699e66c060a`. Host: Linux
6.18.52-1-lts x86-64, GCC/G++ 16.2.1 (`20260810`), Clang/Clang++ 22.1.8,
Python 3.14.7 and CMake 4.4.3.

This continuation extends the existing direct checked-HIR-to-C11 finite-owner
profile rather than adding another ownership model. Postfix `?` consumes a
move-only nominal result, marks a named source moved, and uses checked projected
transfers for the active `Ok` or `Err` payload. The source and transferred
payload storage are cleared. Existing canonical move-state verification
therefore rejects later use, duplicate ownership and forged transfer metadata.
Exact generic and module-qualified nominal identity is unchanged.

A managed return snapshot is now in the bounded emergency cleanup registry
while reached function defers and trailing lexical drops execute. It is
disarmed immediately before the C return. The injected failure case proves that
an owner-bearing propagated `Err` is released when a non-owning defer reaches a
failing owning allocation and the runtime exits 72. Owner captures and deferred
owner arguments remain rejected.

The executable suite also performs explicit iterative On1x consumption of one
50,001-box chain and independent iterative destruction of another. The C++ ABI
test retains its separate 20,000-box destruction case.

### Measured results

| Configuration | Result |
|---|---|
| GCC, LibStone generated scanner, owner/defer focused CTest | **4/4 passed** |
| GCC, fallback scanner, owner/defer focused CTest | **4/4 passed** |
| Clang, generated scanner, compiler and emitted C/runtime under ASan+UBSan | **4/4 passed**, no sanitizer finding |
| GCC, generated scanner, all tests except two unchanged baseline inventory checks | **86 passed, 1 intentionally skipped, 0 failed** (87 total) |
| GCC, fallback scanner, complete CTest suite | **85 passed, 1 intentionally skipped, 2 baseline inventory failures** (88 total) |
| Generated/fallback scanner ownership executable and HIR tests | strict C11, generic/module, fault and deep paths **PASS** |

The intentional skip is `on1x.category` with native PPX disabled. The two
complete-suite failures are outside this change and reproduce in unchanged
baseline inventory code:

- `on1x.contracts` still expects exactly three auxiliary MuLIB headers, while
  the `6a145cb` baseline contains a larger uncatalogued header set;
- `on1x.mulib` finds baseline directory `library/muw3lib`, which is absent from
  the fixture catalog.

No file under `include/` or `library/`, and none of
`on1x/tests/test_{contracts,mulib}.py` or
`on1x/tools/on1x-{contracts,mulib}.py`, differs from `6a145cb`. A
generated-scanner `--parallel 4` complete run also observed one timing-sensitive
`captured_stdin_epipe` concurrency assertion; its immediate isolated retry
passed **1/1**, and the subsequent 87-test `--parallel 2` run passed every
non-inventory test. This is recorded as an unrelated flaky result, not silently
counted as a pass.

Coverage added by this continuation includes:

- generic and module-imported owner-bearing `Outcome<T,E>` success and error
  propagation;
- canonical `Result` success/error paths, nested `??`, active-arm destruction,
  and scalar non-owning defers;
- partial record construction before propagated failure and exact cleanup of
  already-built owner fields;
- use-after-propagation and repeated-propagation diagnostics;
- projected `Err` transfer-metadata mutation rejection in checked HIR;
- owning-allocation failure before propagation and while a propagated managed
  return is held across a defer;
- 50,001-box iterative consumption and 50,001-box iterative destruction; and
- retained MIR, PPX, raw FFI, capture/deferred-owner, staging and concurrency
  rejection boundaries.

Reproduction from the MuLIB root:

```sh
build=/mnt/nvme/opencode-builds/ses_e0d13d7a9ffe4DEJxtuNoZl9ei/recursive-adts
libstone=/mnt/nvme/mulib/on1x/third_party/libstone

cmake -S on1x -B "$build/gcc-generated" -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++ -DON1X_LIBSTONE_ROOT="$libstone" \
  -DON1X_REQUIRE_LIBSTONE=ON -DON1X_WITH_SYREN=OFF \
  -DON1X_WITH_WETSTONE=OFF -DON1X_BUILD_NATIVE_PPX=OFF \
  -DON1X_BUILD_EXTENSION_SDK=OFF
cmake --build "$build/gcc-generated" --parallel 4

cmake -S on1x -B "$build/gcc-fallback" -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++ -DON1X_LIBSTONE_ROOT="$libstone" \
  -DON1X_USE_FALLBACK_SCANNER=ON -DON1X_WITH_SYREN=OFF \
  -DON1X_WITH_WETSTONE=OFF -DON1X_BUILD_NATIVE_PPX=OFF \
  -DON1X_BUILD_EXTENSION_SDK=OFF
cmake --build "$build/gcc-fallback" --parallel 4

ctest --test-dir "$build/gcc-generated" --output-on-failure \
  -R '^on1x\.(defer_error_hir|defer_errors|recursive_owning_adts(_hir)?)$'
ctest --test-dir "$build/gcc-fallback" --output-on-failure \
  -R '^on1x\.(defer_error_hir|defer_errors|recursive_owning_adts(_hir)?)$'

cmake -S on1x -B "$build/clang-asan" -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ -DON1X_LIBSTONE_ROOT="$libstone" \
  -DON1X_REQUIRE_LIBSTONE=ON -DON1X_WITH_SYREN=OFF \
  -DON1X_WITH_WETSTONE=OFF -DON1X_BUILD_NATIVE_PPX=OFF \
  -DON1X_BUILD_EXTENSION_SDK=OFF \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all' \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all' \
  '-DON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer;-fno-sanitize-recover=all'
cmake --build "$build/clang-asan" --parallel 4 --target \
  on1xc on1x_defer_error_tests on1x_recursive_owning_adts_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:strict_string_checks=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir "$build/clang-asan" --output-on-failure \
  -R '^on1x\.(defer_error_hir|defer_errors|recursive_owning_adts(_hir)?)$'

ctest --test-dir "$build/gcc-generated" --output-on-failure --parallel 2 \
  -E '^on1x\.(contracts|mulib)$'
ctest --test-dir "$build/gcc-fallback" --output-on-failure --parallel 4
```

This does not admit general managed/resource-bearing results, move-only helper
combinators, Option propagation, owners in deferred arguments or captures,
ownership MIR/PPX/raw FFI/staging/concurrency, receiver ownership, other targets
or a separate-compilation ownership ABI. The exact bounded contract is
[`docs/recursive-owning-adts.md`](docs/recursive-owning-adts.md).

## Initial recursive nominal ADTs through finite unique ownership (historical)

Historical checkout identity: MuLIB
`b66da8f5150adca2a3416db747d91bf81a3fe138` plus the shared uncommitted
worktree, configured LibStone
`934b6ed029bfea28bbf23b5dc77c476154b0dd0d`. Host: Linux
6.18.52-1-lts x86-64, GCC/G++ 16.2.1 (`20260810`), Clang/Clang++ 22.1.8,
Python 3.14.7 and CMake 4.4.3.

This increment is **EXECUTABLE**, not a claim of general verified ownership or
resource safety. The canonical parser, inference engine, nominal-layout
checker, checked-HIR ownership verifier, direct C11 emitter and runtime admit
recursive records/enums only when each layout cycle crosses `own<Named>`.
`box`, `move`, `take` and `drop` are shadowable fallback intrinsics. Checked
types retain exact finite nominal identity; transfers clear source storage and
never copy unique values. Lexical cleanup covers parameters, temporaries,
overwrites, partial construction, branches, loops and early returns. Generated
sum destructors release only the active arm.

The C11 ABI uses opaque `on1x_unique *` under
`ON1X_UNIQUE_ABI_VERSION == 1`. Destruction uses a FIFO iterative queue rather
than recursive C calls. Terminal owning-allocation failure drains the bounded
cleanup registry and exits 72; the allocation budget is not interpreted as a
timeout. Consuming matches require exhaustive unguarded constructors and
closed irrefutable nested destructuring.

### Historical measured results

| Configuration | Result |
|---|---|
| GCC, LibStone generated scanner, complete CTest suite | **90 passed, 1 intentionally skipped, 0 failed** (91 total) |
| GCC, bootstrap fallback scanner, complete CTest suite | **89 passed, 1 intentionally skipped, 0 failed** (90 total) |
| Clang, generated scanner, real Chisel + Nerotypical focused profile | **6/6 passed** |
| GCC, fallback scanner, real Chisel + Nerotypical focused profile | **6/6 passed** |
| Direct strict-Clang ownership runtime/ABI/module/fault/stress test | **PASS** |
| Direct Clang emitted-C11 ASan/UBSan ownership test | **PASS**, no sanitizer findings |
| Root embedded-profile and Elvis integration selection | **6/6 passed** |

The sole skipped test in each complete suite is `on1x.category`, which reports
`UNSUPPORTED (ON1X_BUILD_NATIVE_PPX=OFF)` and returns the configured skip code.
No test failed or was disabled. `on1x.mulib` now passes in both suites after
regenerating `include/mulib-unit-data.inc`;
`python3 tools/embed_units.py --check` reports that the embedded profiles are
current. Older dated sections below retain
their original historical results, including then-current stale-profile
failures, and are not descriptions of the current continuation.

The generated-scanner complete suite includes scanner parity. The dedicated
ownership tests additionally cover:

- generic recursive lists, cross-module nominal identity, left-to-right
  effects, calls/returns and mutable reinitialization;
- moved-from and double-move rejection, ownership state at joins/backedges,
  consuming-match restrictions and forged checked-HIR metadata;
- exactly-once block/loop/return/parameter/temporary cleanup, partial aggregate
  construction and mixed shared-plus-unique active-arm destruction;
- strict C11, C and C++ runtime callers, runtime ABI-version mismatch and
  malformed raw ABI calls;
- injected allocation failure at three construction points, with exit 72 and
  emergency cleanup;
- a generated 50,001-box chain and an independent C++ 20,000-box chain,
  establishing iterative destruction without C-stack recursion; and
- fail-closed MIR, Unit-v1 PPX, raw FFI, closure, defer, staging, concurrency,
  array/unnamed-aggregate, Chisel and Nerotypical boundaries.

The optional SDK profiles use the real pinned Chisel/Nerotypical headers and a
separately prepared Satie EUF SDK; they reject unique ownership explicitly
rather than approximating it. Build objects and compiler temporaries were kept
on NVMe because the host `/tmp` filesystem was constrained.

Reproduction from the MuLIB root:

```sh
export TMPDIR="$PWD/build/opencode-tmp/compiler"

cmake --build build/elvis-on1x-generated --parallel 4
ctest --test-dir build/elvis-on1x-generated --output-on-failure --parallel 2
cmake --build build/elvis-on1x-fallback --parallel 4
ctest --test-dir build/elvis-on1x-fallback --output-on-failure --parallel 2

cmake -S on1x -B build/owning-sdk-generated -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ -DON1X_REQUIRE_LIBSTONE=ON \
  -DON1X_WITH_CHISEL=ON -DON1X_WITH_NEROTYPICAL=ON \
  -DON1X_SATIE_ROOT=/tmp/opencode/satie-owning-sdk \
  -DON1X_WITH_SYREN=OFF -DON1X_WITH_WETSTONE=OFF \
  -DON1X_BUILD_NATIVE_PPX=OFF
cmake --build build/owning-sdk-generated --parallel 2 --target \
  on1xc on1x_safety_tests on1x_type_safety_tests \
  on1x_recursive_owning_adts_tests
ctest --test-dir build/owning-sdk-generated --output-on-failure \
  -R '^on1x\.(safety(_hir)?|type_safety(_hir)?|recursive_owning_adts(_hir)?)$'

cmake -S on1x -B build/owning-sdk-fallback -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++ -DON1X_USE_FALLBACK_SCANNER=ON \
  -DON1X_WITH_CHISEL=ON -DON1X_WITH_NEROTYPICAL=ON \
  -DON1X_SATIE_ROOT=/tmp/opencode/satie-owning-sdk \
  -DON1X_WITH_SYREN=OFF -DON1X_WITH_WETSTONE=OFF \
  -DON1X_BUILD_NATIVE_PPX=OFF
cmake --build build/owning-sdk-fallback --parallel 2 --target \
  on1xc on1x_safety_tests on1x_type_safety_tests \
  on1x_recursive_owning_adts_tests
ctest --test-dir build/owning-sdk-fallback --output-on-failure \
  -R '^on1x\.(safety(_hir)?|type_safety(_hir)?|recursive_owning_adts(_hir)?)$'

python3 on1x/tests/test_recursive_owning_adts.py \
  build/elvis-on1x-generated/on1xc "$PWD/on1x" \
  build/recursive-owning-clang clang
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:strict_string_checks=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
python3 on1x/tests/test_recursive_owning_adts.py \
  build/elvis-on1x-generated/on1xc "$PWD/on1x" \
  build/recursive-owning-clang-sanitize clang \
  --cflag=-fsanitize=address,undefined \
  --cflag=-fno-omit-frame-pointer --cflag=-fno-sanitize-recover=all

ctest --test-dir build/cdd-root-gcc --output-on-failure \
  -R '^(mulib_(archml_profiles_generated|unit_profiles_embedded)|mulib_test-elvi-scrape(-providers|-https|-alloc)?)$'
```

Ownership remains a checked-HIR-to-C11 profile until MIR gains typed
move/drop/place/cleanup events. Unit-v1 PPX/interfaces, general borrows,
foreign ownership, captures, staging/defer, cooperative concurrency,
unsupported aggregates, recoverable OOM, user destructors, `Send`/`Sync`,
other targets and separate-compilation ownership ABI remain closed. `ref self`
and `move self` also remain rejected because receiver-place/lifetime lowering
is not present. The exact contract is
[`docs/recursive-owning-adts.md`](docs/recursive-owning-adts.md).

## Intrinsic core cliches

MuLIB `b66da8f5150adca2a3416db747d91bf81a3fe138` plus shared worktree
changes; configured LibStone
`934b6ed029bfea28bbf23b5dc77c476154b0dd0d`; Linux
`6.18.52-1-lts` x86-64; GCC 16.2.1 and Clang 22.1.8.

The canonical parser, loader, inference engine, checked HIR and direct C11
backend implement intrinsic declarations such as `cliche Show { ... }`,
generic implementations such as `impl<T> Show for Box<T> where Show<T>`, and
explicit projections such as `Show<T>::show(value)`. `Self` is implicit
cliche argument zero. Generic cliches, functions, implementations and members,
associated values, bounds and imported declarations are covered. This path is
separate from legacy Category `protocol`/`impl`, Unit-v1 PPX transport and
inherent `Type::member` lookup.

Selection is compile-time and deterministic. Duplicate exact heads,
overlapping blanket heads and orphan implementations reject; one exact match
overrides one matching blanket implementation. Recursive prerequisites must
decrease structurally, and depth and work budgets remain distinct from
semantic failure. Tests distinguish `ON1X-CLICHE-LIMIT`,
`ON1X-CLICHE-UNSATISFIED`, `ON1X-CLICHE-AMBIGUOUS` and
`ON1X-CLICHE-RECURSIVE`. Selected members lower to private ordinary functions;
associated values lower to nullary functions. Repeated emission is
byte-identical, and the generated C contains neither cliche source names nor a
runtime string lookup. Unresolved executable cliche or inherent projections
fail closed at checked-HIR, MIR and C11 admission.

Final focused commands, run with `TMPDIR="$PWD/build/opencode-tmp"` because the
host `/tmp` filesystem was full:

```sh
cmake --build build/core-enum-generated --parallel 4 --target \
  on1xc on1x_cliche_tests on1x_type_tests on1x_inference_tests \
  on1x_generated_tokens on1x_reference_tokens
ctest --test-dir build/core-enum-generated --output-on-failure --parallel 3 \
  -R '^on1x\.(cliche(_hir)?|types|inference_engine|inference|poly|poly_extended|scanner_parity)$'

cmake --build build/core-enum-fallback --parallel 4 --target \
  on1xc on1x_cliche_tests on1x_type_tests on1x_inference_tests
ctest --test-dir build/core-enum-fallback --output-on-failure --parallel 3 \
  -R '^on1x\.(cliche(_hir)?|types|inference_engine|inference|poly|poly_extended)$'

cmake --build build/cliche-clang --parallel 4 --target on1xc on1x_cliche_tests
ctest --test-dir build/cliche-clang --output-on-failure --parallel 3 \
  -R '^on1x\.(cliche(_hir)?|types|inference_engine|inference|poly|poly_extended)$'

cmake -S on1x -B build/cliche-sanitize -DBUILD_TESTING=ON \
  -DON1X_USE_FALLBACK_SCANNER=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined' \
  '-DON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer'
cmake --build build/cliche-sanitize --parallel 4 --target \
  on1xc on1x_cliche_tests on1x_type_tests on1x_inference_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:strict_string_checks=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build/cliche-sanitize --output-on-failure --parallel 2 \
  -R '^on1x\.(cliche(_hir)?|types|inference_engine|inference|poly|poly_extended)$'
```

| Configuration | Result |
|---|---|
| GCC, generated scanner, focused compiler/runtime plus scanner parity | **8/8 passed** |
| GCC, fallback scanner, focused compiler/runtime | **7/7 passed** |
| Clang, fallback scanner, focused compiler/runtime | **7/7 passed** |
| Clang ASan/UBSan with leak detection, including emitted C11 runtime | **7/7 passed**, no findings |

The runtime test compiles emitted applications as strict C11 with
`-Wall -Wextra -Wpedantic -Werror` and covers exact-over-blanket selection,
associated values, generic/member substitutions, left-to-right effects,
qualified and selective imports, deterministic emission and 13 source
rejections. HIR tests cover malformed declarations and signatures, cloning,
orphan/coherence rules, all four resolution outcomes above and fail-closed
backend boundaries.

A broader generated-scanner check,
`ctest --test-dir build/core-enum-generated --output-on-failure -R
'^on1x\.(compiler|nominal_adts)$'`, remains **0/2** because of concurrent
expectation drift outside cliche behavior. `on1x.compiler` now rejects the
fixture's redefinition of intrinsic `take` before reaching its expected
`resource returns` error. `on1x.nominal_adts` now reports
`ON1X-NOMINAL: malformed declaration type: unresolved inference hole` for
`hole_field` instead of its older expected text. Neither failure executes a
cliche declaration.

Unsupported cases remain associated types, negative implementations,
specialization beyond exact-over-blanket selection, dynamic dictionaries,
runtime reflection, separate compiled interface transport and recursive
nominal layouts. Unit-v1 PPX rejects rather than silently dropping cliches.
The scalar MIR accepts only programs that elaborate to its existing type and
operation profile.

## Intrinsic nominal implementation blocks

Baseline: MuLIB `b66da8f5150adca2a3416db747d91bf81a3fe138` plus the shared
worktree changes, configured LibStone
`934b6ed029bfea28bbf23b5dc77c476154b0dd0d`, Linux
`6.18.52-1-lts` x86-64, GCC 16.2.1 and Clang 22.1.8.

The canonical parser, loader, inference engine, checked HIR and direct C11
backend accept generic nominal blocks such as `impl<T> Box<T> { ... }`.
`value::member` supplies an exactly-once first argument; `Type::member` selects
an associated function or recomputed value. Generic targets and members,
records and enums, imports/aliases/re-exports, first-class associated
functions, hygienic macros in member bodies, and copy-safe value receivers are
executable. Selection uses canonical nominal identity and a unique
`(nominal, member)` key; no runtime registry or string dispatch is emitted.

Inherent and cliche declarations share `TypeParameter`, `ClicheConstraint`,
`Function`, and bound-index ordering, while retaining separate lookup and
coherence rules. The parser distinguishes a cliche `impl ... for ...` head from
a nominal `impl Target` head before building either carrier. A spelling that
resolves as both a cliche and nominal owner fails deterministically with
`ON1X-IMPL-AMBIGUOUS`. Positive coexistence and both loader and raw-HIR
ambiguity paths are covered.

The final combined focused commands used NVMe-backed build and temporary
directories because the host `/tmp` tmpfs was full:

```sh
work=/mnt/nvme/opencode-tmp

cmake -S on1x -B "$work/on1x-core-impl-generated" -DBUILD_TESTING=ON
TMPDIR="$work/tmp" cmake --build "$work/on1x-core-impl-generated" \
  --parallel 4 --target on1xc on1x_cliche_tests on1x_core_impl_tests
ctest --test-dir "$work/on1x-core-impl-generated" --output-on-failure \
  -R '^on1x\.(cliche(_hir)?|core_impl(_hir)?)$'

cmake -S on1x -B "$work/on1x-core-impl-fallback" -DBUILD_TESTING=ON \
  -DON1X_USE_FALLBACK_SCANNER=ON
TMPDIR="$work/tmp" cmake --build "$work/on1x-core-impl-fallback" \
  --parallel 2 --target on1xc on1x_cliche_tests on1x_core_impl_tests
ctest --test-dir "$work/on1x-core-impl-fallback" --output-on-failure \
  -R '^on1x\.(cliche(_hir)?|core_impl(_hir)?)$'

CC=clang CXX=clang++ cmake -S on1x -B "$work/on1x-core-impl-clang" \
  -DBUILD_TESTING=ON
TMPDIR="$work/tmp" cmake --build "$work/on1x-core-impl-clang" \
  --parallel 2 --target on1xc on1x_cliche_tests on1x_core_impl_tests
ctest --test-dir "$work/on1x-core-impl-clang" --output-on-failure \
  -R '^on1x\.(cliche(_hir)?|core_impl(_hir)?)$'

CC=clang CXX=clang++ cmake -S on1x \
  -B "$work/on1x-core-impl-sanitize" -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined \
  '-DON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer'
TMPDIR="$work/tmp" cmake --build "$work/on1x-core-impl-sanitize" \
  --parallel 2 --target on1xc on1x_cliche_tests on1x_core_impl_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:strict_string_checks=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir "$work/on1x-core-impl-sanitize" --output-on-failure \
  -R '^on1x\.(cliche(_hir)?|core_impl(_hir)?)$'
```

| Configuration | Result |
|---|---|
| GCC, generated scanner, combined cliche/inherent source and HIR | **4/4 passed** |
| GCC, fallback scanner, combined cliche/inherent source and HIR | **4/4 passed** |
| Clang, generated scanner, strict emitted C11 | **4/4 passed** |
| Clang ASan/UBSan, compiler plus emitted C11/runtime | **4/4 passed**, no findings |

The generated-scanner regression selection for macro/local-syntax, cliche,
nominal HIR, scalar MIR, scanner parity and both implementation suites passed
**11/11**. Formatter, inference, extended polymorphism, module and stdppx
protocol selections passed **6/6**. The broader `on1x.nominal_adts` test still
expects older diagnostic text for `hole_field`; the ordinary PPX sweep reaches
an unrelated concurrent canonical-value verifier failure in
`cancellation_deadlines.on1x`. Neither failure contains an inherent or cliche
implementation.

`ref self` and `move self`, managed/resource value receivers, explicit inherent
member type arguments, visibility modifiers, overloading/specialization,
Unit-v1 PPX and `.on1xi` transport, and aggregate MIR remain unsupported and
reject explicitly. Scalar-only elaborated members continue through the existing
MIR evaluator/optimizer/C11 paths.

## Core literal, regex, matching and ownership slices (historical baseline)

MuLIB `b66da8f5150adca2a3416db747d91bf81a3fe138` plus worktree changes,
LibStone `934b6ed029bfea28bbf23b5dc77c476154b0dd0d`, Linux x86-64,
GCC 16.2.1 and Clang 22.1.8. At this recorded pre-unique-owner baseline, these
features use the canonical compiler
and direct C11 backend, not an extension activation or PPX. Capture support
is a bounded exact subset, `ref i64` is call-scoped, and nominal ownership
is limited to immutable shared `sexpr` fields. Recursive nominal layouts
and general unique-resource fields still rejected. The current finite
unique-owner profile and results are recorded above.

```sh
for mode in generated fallback; do
  build="build/elvis-on1x-$mode"
  cmake -S on1x -B "$build"
  cmake --build "$build" --parallel 3
  ctest --test-dir "$build" --output-on-failure \
    -R '^on1x\.(core_.*|enum_connective|literals_core|owning_adt|scanner_parity|formatter|compiler|data|closures|concurrency)$'
done
compiler="$PWD/build/elvis-on1x-fallback/on1xc"
for suite in core_ref enum_connective literals_core owning_adt core_syntax_integration; do
  python3 "on1x/tests/test_$suite.py" "$compiler" "$PWD/on1x" \
    "build/core-sanitized/$suite" clang --cflag=-fsanitize=address,undefined \
    --cflag=-fno-sanitize-recover=all
done
python3 on1x/tests/test_core_regex.py "$compiler" clang \
  -fsanitize=address,undefined -fno-sanitize-recover=all
```

The tests include scanner byte/offset parity, malformed literals and Unicode
scalars, Python `re.fullmatch` differential tests on the shared regex subset,
borrow-conflict and escape rejection (including malformed checked HIR),
and RC copy/drop and allocation failure. The combined program is compiled
again after formatting and its stdout/stderr are compared. Sanitization
above applies to emitted C11 and the linked runtime.

The full runs passed 81 of 83 tests with the generated scanner and 80 of 82
with the fallback scanner (`ctest --test-dir "$build" --output-on-failure
--parallel 4` in both). `on1x.category` was skipped and `on1x.mulib` reported
the pre-existing stale OptML unit data.
The native `on1x.luaembed` regression also passed after migrating its
literal `$LuaEmbed` labels to raw strings.

## On1x editor addons

Baseline: MuLIB `b66da8f5150adca2a3416db747d91bf81a3fe138` plus worktree
changes, configured LibStone `934b6ed029bfea28bbf23b5dc77c476154b0dd0d`,
Linux x86-64, GCC 16.2.1, and Python 3.14.7. The addons change no compiler
or application runtime code.

The addon tests exercise Vim 9.2 and Neovim 0.12.5 with isolated installs,
the `bat` 0.26.1 syntax cache and file detection, and the VS Code manifest
and regexes with Node.js 26.9.0. All six addon tests passed in both scanner
configurations. `vsce` 3.9.2 also built a local VSIX.
VS Code/VSCodium and Sublime Text GUI hosts were unavailable; `bat` uses
the shared Sublime grammar but is not Sublime's scope-test runner.

The protocol tests cover actual compiler diagnostics and formatting as well
as malformed input, Unicode positions, stale snapshots, subprocess limits,
and non-execution of PPX. The headless Neovim test verifies manual LSP
startup, save diagnostics, and buffer-only formatting. The server's child
address-space limit is not compatible with shadow-memory sanitizers;
`-DON1X_TEST_ADDONS_WITH_COMPILER=OFF` keeps the protocol fixtures and
skips compiler-backed editor smoke tests in such configurations.

```sh
for mode in generated fallback; do
  build="build/elvis-on1x-$mode"
  cmake -S on1x -B "$build"
  cmake --build "$build" --parallel 4
  ctest --test-dir "$build" --output-on-failure -R '^on1x\.addon_'
  ctest --test-dir "$build" --output-on-failure --parallel 4
  python3 on1x/tools/on1x-build.py on1x/examples/hello.on1x \
    --on1xc "$build/on1xc" -o "$build/addon-hello"
  "$build/addon-hello" # 21 in both modes
done
```

On the initial checkout `on1x.contracts` and `on1x.mulib` failed on obsolete
`library/elvi-scrape` paths. The fixture cleanup removes both references,
keeps all five profiles under `library/elvis`, and updates the tests to use
the canonical `elvis` fixture group. `on1x.contracts` now passes in both
scanner modes. `on1x.mulib` builds and runs the Elvis client but later fails
on unrelated stale AMDGPU generated data (`tools/embed_units.py --check`).
The existing `on1x.category` test is skipped in these compiler configurations.
No other failures were observed in the full suite runs. All four native Elvis
tests passed after rebuilding:

```sh
cmake --build build/cdd-root-gcc --parallel 4 --target \
  test-elvi-scrape test-elvi-scrape-providers test-elvi-scrape-https test-elvi-scrape-alloc
ctest --test-dir build/cdd-root-gcc --output-on-failure -R '^mulib_test-elvi-scrape'
```

## Complete Clamp C11 binding (v2)

MuLIB `17a4fdd7ea78c0350b39a01892f47adeb7e1f6ba` plus worktree changes;
LibStone `934b6ed029bfea28bbf23b5dc77c476154b0dd0d`; Linux x86-64,
GCC 16.2.1 and Clang 22.1.8. All three functions in `mulib-clamp.h`
are callable through the checked C11 projection. The examples and a separate
C caller agree, and the real SDK test checks 2,670 C/adapter comparisons,
source evaluation order, reversed bounds in both modes, and rejection of
old reviews and incompatible declarations. Signing in this suite uses an
ephemeral test authority; no production approval ships.

```sh
sdk="$PWD/build/sandbox-kernelbind-sdk"
cmake --build "$sdk" --target on1xc on1x_generated_tokens on1x_reference_tokens --parallel 4
ctest --test-dir "$sdk" --output-on-failure \
  -R '^on1x\.(kernel_imports|kernel_review_sdk|kernel_history_sdk|cdd_discovery_sdk|scanner_parity)$'
cmake --build build/elvis-on1x-fallback --target on1xc --parallel 4
ctest --test-dir build/elvis-on1x-fallback --output-on-failure -R '^on1x\.kernel_imports$'
ASAN_OPTIONS=detect_leaks=1 python3 on1x/tests/test_kernel_review_sdk.py \
  "$sdk/libstone-moonstone/exotract/exotract" "$sdk/on1xc" "$PWD/on1x" \
  "$PWD/build/clamp-v2-review-clang-asan" clang \
  --cflag=-fsanitize=address,undefined --cflag=-fno-omit-frame-pointer \
  --cflag=-fno-sanitize-recover=all
```

Clamp remains limited to the native Linux x86-64 C11 build profile. MIR,
other targets and general C FFI were not enabled by this change.

## MuLIB contract inventory and CDD discovery

MuLIB `17a4fdd7ea78c0350b39a01892f47adeb7e1f6ba` plus worktree changes;
configured LibStone `57113dc48d815359431dce63a6b865a21dd993cd`; Linux x86-64,
GCC 16.2.1 and Clang 22.1.8. Both `on1x/tools/on1x-{contracts,exotract}.py list`
reported 1,188 MANIFEST kernels and three auxiliary headers. Seventeen kernels
have shipped semantic sidecars (five have core snapshot profiles); one
non-kernel companion sidecar is listed separately. The PrettySexpr sidecar's
header revision is stale on this checkout. Registration does not authorize safe
exports.

With the existing `build/sandbox-kernelbind-sdk` tools and a fresh work directory:

```sh
sdk="$PWD/build/sandbox-kernelbind-sdk"
work="$(mktemp -d /tmp/opencode/on1x-contract-coverage.XXXXXX)"
exo="$sdk/libstone-moonstone/exotract/exotract"
python3 on1x/tests/test_contracts.py "$sdk/on1xc" "$PWD/on1x" "$work/contracts" clang
python3 on1x/tests/test_cdd_discovery_sdk.py "$exo" "$sdk/on1xc" "$PWD/on1x" "$work/cdd-clang" clang
python3 on1x/tests/test_cdd_discovery_sdk.py "$exo" "$sdk/on1xc" "$PWD/on1x" "$work/cdd-gcc" gcc
python3 on1x/tests/test_exotract.py "$exo" "$sdk/on1xc" "$PWD/on1x" "$work/exotract" clang
python3 on1x/tests/test_kernel_sandbox_sdk.py "$exo" "$sdk/on1xc" "$PWD/on1x" "$work/sandbox" clang
```

These passed. The CDD SDK test extracts all eight headers with ExoML/libclang
and verifies that safe and unsafe builds and review-subject generation refuse
CDD. The CDD plan is metadata only; there was no CDD archive or callable ABI to
test. `test_contracts_sdk.py --extract` also passed with
`build/kernelbind-gcc/on1x-bind`. Broader `test_kernel_imports_sdk.py` and
`test_mulib.py` are blocked by the pre-existing PrettySexpr sidecar revision and
stale generated OptML unit profiles, respectively.

## Surfraw ElviScrape fixture integration

MuLIB `681dacbdd419b71ae86042d0c307180cbd5dea5e` plus worktree changes;
LibStone `57113dc48d815359431dce63a6b865a21dd993cd`; Linux x86-64
6.18.52-1-lts/glibc 2.44; GCC 16.2.1 and Clang 22.1.8.

- `ctest --test-dir build/elvis-clang-asan --output-on-failure -R mulib_test-elvi-scrape`:
  four strict C11 suites with Clang ASan/UBSan passed, covering all five profiles,
  legacy path parity, extraction, HTTPS transport, and allocation failures.
- `python3 on1x/tools/on1x-contracts.py lint` and the SDK `snapshot --library
  elvi-scrape` passed with refreshed source pins; neither admits safe imports.
  `on1x-exotract.py build elvi-scrape --fixture elvis --compiler clang` built
  the one-TU archive; its separate C11 client and generated On1x client both
  executed offline. The full `on1x/tests/test_exotract.py` suite passed with
  Clang. The same [On1x client](tests/mulib/elvi-scrape.on1x) passed with
  `on1x-build.py --mulib-fixture elvis --extra-c on1x/tests/mulib/elvi-scrape.c`
  using each scanner mode.
- Both On1x scanner selections passed the core test selection, including
  generated-vs-fallback scanner parity in the generated configuration.
  Fallback `on1x.contracts`, `on1x.install`, `on1x.stdlib_extensions` and
  `on1x.kernel_imports` passed separately. `on1x.mulib` ran its Elvi assertions
  successfully and then hit the previously documented unrelated stale
  `tools/embed_units.py` profile check. No live search provider was contacted.

## GrammarEmbed — native liblr lexer/parser emission

Added on the same `on1x/luaembed` worktree, based on MuLIB
`681dacbdd419b71ae86042d0c307180cbd5dea5e`. For this continuation the externally
maintained LibStone checkout reports
`934b6ed029bfea28bbf23b5dc77c476154b0dd0d` at
`/mnt/nvme/mulib/on1x/third_party/libstone`; this task did not modify it.
Linux x86-64, GCC 16.2.1, Clang 22.1.8. The preceding LuaEmbed measurements
retain their original LibStone revision and are not relabeled.

The native On1x processor in `stdppx/native/GrammarEmbed.on1x` uses the PPX,
schema, S-expression and visitor libraries. `stdppx/grammarembed/bridge.cpp`
uses the actual Firestone `liblr` headers for regex/Thompson construction,
determinization/minimization, EBNF lowering and LR1/LALR1/LL1 analysis. It emits
table data; the checked On1x `machinery.on1x` template supplies the emitted
engines. Applications compile as strict C11 against only `on1x_runtime.c`.
No runtime LibStone, PPX, C++ companion or Python interpreter is linked.

### Checks

The following commands reuse the generated/GCC, fallback/Clang and
fallback/Clang sanitizer configurations described below in the LuaEmbed section:

```sh
cmake --build /tmp/opencode/luaembed-gcc --parallel 4
cmake --build /tmp/opencode/luaembed-clang --parallel 4
cmake --build /tmp/opencode/luaembed-san --parallel 4
ctest --test-dir /tmp/opencode/luaembed-gcc --output-on-failure --parallel 3 \
  -R '^on1x\.(grammarembed(_bridge)?|install)$'
ctest --test-dir /tmp/opencode/luaembed-clang --output-on-failure --parallel 3 \
  -R '^on1x\.(grammarembed(_bridge)?|install)$'
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir /tmp/opencode/luaembed-san --output-on-failure --parallel 2 \
  -R '^on1x\.(grammarembed(_bridge)?|install)$'
```

`tests/test_grammarembed.py` exercises all six NFA/DFA × LR1/LALR1/LL1
combinations. For each, it compares all **781 strings** of length 0–4 over
`12+ x` against the independent C++ liblr lexer/parser in
`tests/grammarembed_reference.cpp`: **4,686 comparisons per configuration**.
It separately checks exact trees and byte spans, UTF-8 bytes, maximal munch,
literal priority, token helpers, epsilon, comments, start selection, multiple
grammars, fresh helper names, a canonical-LR1-but-not-LALR1 language, LR left
recursion, LL conflicts, malformed grammars, rejected partial output, type
checking, source/input/token/tree/work limits, native protocol framing,
name discovery and the shipped calculator example. LuaEmbed emits a new
GrammarEmbed marker in a real two-processor pipeline.

`test_grammarembed_bridge.cpp` tests the borrowed-view C ABI and empty-plan
failure behavior. `test_install.py` executes a relocated GrammarEmbed bundle
with its installed schema, independently of source checkout paths.
Generated programs use `-std=c11 -Wall -Wextra -Wpedantic -Werror`.
Both `g++` and `clang++` also pass `-std=c++20 -Wall -Wextra -Wpedantic -Werror
-fsyntax-only` on the new companion, bridge test and reference oracle, using
the configured Firestone headers as system SDK headers.

The first integration iterations exposed foreign-string indexing overhead,
the C11 minimum literal-length limit, and invalid test/example source escapes.
The PPX now snapshots foreign strings, emits large tables in bounded chunks,
and the source fixtures use supported On1x literals/buffer operations.

Final generated/GCC and fallback/Clang integration tests pass; bridge and
relocated-install tests also pass in both profiles (**3/3 each**, including
targeted integration reruns after fixture corrections). Related generated
PPX, native/Python stdppx, LuaEmbed and scanner-parity selections pass **4/4**.
Clang ASan/UBSan with leak detection passes the same GrammarEmbed integration,
bridge and relocated-install checks (**3/3**, no sanitizer findings), plus
the LuaEmbed regression (**1/1**). The final no-behavior-change extraction of
name/group limits into the catalog was rebuilt in all three profiles; bridge
checks and the emitted calculator example were rechecked.
Catalog regeneration, Python compilation and scoped diff checks pass. The
explicit no-LibStone/GrammarEmbed-disabled bootstrap configuration succeeds.

### Scope

This is an executable byte-oriented, Perl 6-inspired grammar **subset**, using
an ordinary attribute string carrier. It supports NFA/DFA lexing and
conflict-free LR1/LALR1/LL1 parsing, not Raku actions/inheritance or every liblr
algorithm. New diagnostic messages, statuses and limits are JSON-owned.
Liblr construction-size checks are post-construction; its internal table
generation is not incrementally work-bounded. The PPX deadline remains active.
The generated runtime has explicit work/token/depth/input limits; the ordinary
On1x allocator/string-pool limits retain their existing fatal-error behavior.
See the [GrammarEmbed contract](stdppx/grammarembed/README.md).

The full unrelated compiler suite was not rerun for this isolated PPX.
The earlier full LuaEmbed report's two baseline Elvi-scrape fingerprint
failures remain historical evidence, not a new GrammarEmbed full-suite result.

## LuaEmbed native PPX and embedded `llembed`

Isolated branch `on1x/luaembed`, starting MuLIB
`681dacbdd419b71ae86042d0c307180cbd5dea5e`; configured read-only LibStone
`57113dc48d815359431dce63a6b865a21dd993cd` at
`/mnt/nvme/mulib/on1x/third_party/libstone`, unchanged. Linux
`6.18.52-1-lts` x86-64, GCC 16.2.1 and Clang 22.1.8.

`stdppx/native/LuaEmbed.on1x` uses the existing native PPX/visitor/schema
libraries. `stdppx/luaembed/runtime.[ch]` is a trusted, synchronous build-host
scalar companion to canonical Mune, not safe foreign admission. The existing
MuLIB planner generates one Mune implementation TU; `ar t
/tmp/opencode/luaembed-gcc/libon1x_luaembed_kernels.a` reports only
`luaembed_impl.c.o`. `llembed.lua` and `messages.json` are embedded by a
build-only generator. The ordinary application runtime and core grammar did
not change; generated clients do not link Mune or LibStone.

The integration suite compiles/runs runtime-position stdout, exact text,
expressions, statement streams, full programs, generated-program/expr chains,
composition with a later explicitly registered native PPX, fresh names and
caller shadowing, UTF-8/escape handling, i64 minimum, discovery and the shipped
example. It verifies byte-identical repeated C output for these scalar/string
fixtures, final compiler type rejection and unchanged C destinations on failure.
Negatives cover malformed protocol/schema/S-expressions, trailing stream data,
bad Lua syntax/semantics, NUL, unsupported modules, source/work/heap/output
exhaustion, generated-name collisions, unauthorized labels, 65 evaluations and
nonterminating metaprogram chains. Native C tests separately exercise ABI
failure paths and terminal-control escaping. Linux linker allocation wrappers
sample 128 failure points across the full parse/state/library/execution lifetime.
Relocated installation executes LuaEmbed using the installed bundle/schema.

### Reproduction

From the MuLIB root:

```sh
cmake -S on1x -B /tmp/opencode/luaembed-gcc \
  -DBUILD_TESTING=ON -DON1X_BUILD_NATIVE_PPX=ON \
  -DON1X_LIBSTONE_ROOT=/mnt/nvme/mulib/on1x/third_party/libstone \
  -DON1X_REQUIRE_LIBSTONE=ON -DON1X_WITH_PROJECT_QMR=OFF \
  '-DCMAKE_C_FLAGS=-Wall -Wextra -Wpedantic -Werror' \
  '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic'
cmake --build /tmp/opencode/luaembed-gcc --parallel 4
ctest --test-dir /tmp/opencode/luaembed-gcc --output-on-failure --parallel 4
ctest --test-dir /tmp/opencode/luaembed-gcc --output-on-failure --parallel 3 \
  -R '^on1x\.(luaembed(_runtime|_allocation)?|install|scanner_parity)$'
```

The Clang configuration `/tmp/opencode/luaembed-clang` substitutes
`-DON1X_USE_FALLBACK_SCANNER=ON` for `-DON1X_REQUIRE_LIBSTONE=ON` and sets
`-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`. Its final selection is
`-R '^on1x\.(luaembed(_runtime|_allocation)?|install)$'`.

The Clang/fallback Debug sanitizer configuration `/tmp/opencode/luaembed-san`
adds `-fsanitize=address,undefined -fno-omit-frame-pointer
-fno-sanitize-recover=all` to both compiler flag sets and uses
`-DON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer;-fno-sanitize-recover=all`
to instrument generated client C11 as well:

```sh
cmake --build /tmp/opencode/luaembed-san --parallel 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir /tmp/opencode/luaembed-san --output-on-failure --parallel 3 \
  -R '^on1x\.(luaembed(_runtime|_allocation)?|ppx|stdppx|install)$'
python3 -m unittest discover -s on1x/stdppx/tests -v
python3 -B on1x/stdppx/make_catalog.py
```

### Results and limits

| Configuration | Result |
|---|---|
| Initial related GCC/generated PPX, libraries, stdppx and scanner parity | **4/4 passed** |
| Full GCC/generated run before allocation-test registration | **63 passed, 2 failed** (65) |
| Final GCC/generated LuaEmbed/runtime/allocation, install, scanner parity | **5/5 passed** |
| Clang/fallback related LuaEmbed/PPX/libraries/stdppx selection | **5/5 passed** |
| Final Clang/fallback LuaEmbed/runtime/allocation and relocated installation | **4/4 passed** |
| Clang ASan/UBSan/leak detection, kernel/bridge/processor/client/fault paths | **6/6 passed**, no findings |
| Final expanded program-chain/i64-boundary integration rerun, all three profiles | **1/1 passed each** |
| Standalone Python PPX/schema regressions | **30/30 passed** |
| Python compilation, generated catalog regeneration and `git diff --check` | **passed** |

The full-suite failures are the previously recorded `on1x.contracts` and
`on1x.mulib` Elvi-scrape manual fingerprint mismatch. Its audited fingerprint
was not refreshed. The first attempted all-`-Werror` C++ build also stopped on
two existing misleading-indentation warnings in `src/loader.cpp` (lines 81/284).
That file was left untouched; C11 kernels, bridge, generated PPX and client
code still compile with strict warnings-as-errors. Host C++ builds retain the
warnings without `-Werror`; no claim of a strict-clean whole compiler is made.

This is an **EXECUTABLE bounded Linux x86-64 profile**, not an OS sandbox,
full Lua conformance proof, automatic processor authorization, raw-Lua core
syntax, general foreign-resource admission or a cross-platform certification.
Lua object-identity formatting is not promised byte-reproducible; use stable
scalar/string constructors. NUL cannot cross the current On1x string ABI and
is rejected, not truncated. The kernel's parser/AST budgets are distinct from
its runtime heap budget. The compiler's existing subprocess deadline remains
the wall-clock limit. See the [complete contract](stdppx/luaembed/README.md).

## Project tooling — starting baseline

MuLIB `f520a7da8cd29c282f335ed3d0fdca1599c2ff30`; configured LibStone
`57113dc48d815359431dce63a6b865a21dd993cd` (left unchanged), Linux
6.18.52-1-lts x86-64/glibc 2.44, CMake 4.4.3 and GCC 16.2.1.

```sh
cmake -S on1x -B build/on1x-tooling-make -DBUILD_TESTING=ON -DON1X_REQUIRE_LIBSTONE=ON
cmake --build build/on1x-tooling-make --parallel 5
ctest --test-dir build/on1x-tooling-make --output-on-failure --parallel 5
make -f prettyprint.mk test
```

The first generated-scanner run had **57 passes and 2 failures**. Both failures
(`on1x.contracts`, `on1x.mulib`) rejected the existing stale
`manual/mulib-elvi-scrape.md` fingerprint. The PrettyPrint C11, bridge, CLI and
`.ppg` tests passed. This is baseline evidence, not verification of the new
formatter or project driver.

### Integrated tooling

The generated/GCC tooling selection passed **7/7** and covers formatting,
project builds, driver, Mune/QMR, discovery, relocated installation and
scanner parity. The fallback/Clang selection passed **6/6**, without the parity
harness. The ASan/UBSan selection passed **2/2**, exercising the C++ formatter,
the C11 PrettyPrint implementation, and both build-time helpers.
The full generated/GCC suite passed **61/63**; only the two baseline
Elvi-scrape fingerprint failures remained.

```sh
ctest --test-dir build/on1x-tooling-make \
  -R '(formatter|project_|install$|discovery|scanner_parity)' --output-on-failure
ctest --test-dir build/on1x-tooling-clang \
  -R '(formatter|project_|install$|discovery)' --output-on-failure
ctest --test-dir build/on1x-tooling-asan \
  -R '(formatter|project_helpers)' --output-on-failure
python3 on1x/stdlib/tests/check_package.py
```

Project tests run generated executables, check linked MuLIB plans and failed
generation rollback, execute Mune profiles and QMR preprocessing, verify
offline Git object pins, and exercise private network and filesystem
namespaces when available. Repeated sandboxed release builds of the same
fixture match; debug output can include generated-C temporary paths and is
not generally byte-identical. The stdlib inventory check passes, but the
whole stdlib is not a self-contained source package under the project's
restricted import rules; the ordinary module loader and CMake installation
remain supported.

### Explicit pinned Git acquisition

The loopback fixture in `tests/test_project_fetch.py` uses a temporary TLS
certificate and Git's `git-http-backend`, with no public network access.
It tests SHA-1 and SHA-256 pins, disabled redirects/auth/config/checkout and
submodule fetching, certificate verification, shallow and dumb HTTP refusal,
failure cleanup, transfer and pack limits, offline cache revalidation, and
real C11 program execution after stopping the server. Both ordinary and
bubblewrap builds used the cache offline on this host.

All **9/9** tests passed with a newly built generated-scanner GCC compiler,
the fallback Clang compiler, and the Clang ASan/UBSan compiler. The existing
project suite passed **11/11** with the generated scanner and Mune/QMR.
Host: Linux `6.18.52-1-lts` x86-64, Git `2.55.0`, Python `3.14.7`,
OpenSSL `3.6.4`; MuLIB and LibStone revisions and compiler versions are
as above. This tests the pinned subset of the stated
[Linux smart HTTPS profile](stdlib/on1xfile/README.md#locking-and-explicit-remote-git-acquisition),
not arbitrary Git server deployments or platform support.

```sh
cmake -S on1x -B build/on1x-fetch-agent -DBUILD_TESTING=ON -DON1X_REQUIRE_LIBSTONE=ON
cmake --build build/on1x-fetch-agent --parallel 4 --target on1xc on1x-mune-runner on1x-qmr-compile
python3 on1x/tests/test_project_fetch.py --on1xc build/on1x-fetch-agent/on1xc --cc gcc
python3 on1x/tests/test_project_fetch.py --on1xc build/on1x-tooling-clang/on1xc --cc clang
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  python3 on1x/tests/test_project_fetch.py --on1xc build/on1x-tooling-asan/on1xc --cc clang
python3 on1x/tests/test_on1x_project.py --on1xc build/on1x-fetch-agent/on1xc --cc gcc \
  --mune-runner build/on1x-fetch-agent/on1x-mune-runner --qmr-compiler build/on1x-fetch-agent/on1x-qmr-compile
```

### Locked Git selector continuation — 2026-11-18

Starting MuLIB checkout `681dacbdd419b71ae86042d0c307180cbd5dea5e`;
configured LibStone `57113dc48d815359431dce63a6b865a21dd993cd`;
Linux x86-64, Git 2.55.0 and Python 3.14.7. The continuation adds
`On1xfile.lock` v1, explicit commit/branch/tag selectors, annotated-tag peeling,
offline/locked/frozen modes, serialized atomic cache publication and atomic
lockfile replacement. Git subprocesses still use the existing shell-free,
HTTPS-only, object-only envelope and never initialize submodules.

The local project suite passed **14/14**. It uses a temporary local bare
repository with a moving branch and annotated tag, proves frozen builds keep
the old executable result, updates locks explicitly, and rejects unsafe refs,
malformed/duplicate lockfile data and lockfile symlinks. The smart-HTTPS suite
passed **11/11**. Its two new cases resolve remote branches/tags, build from the
cache after server shutdown, and race two fetch processes to one valid cache
generation. No public network is used.

```sh
python3 on1x/tests/test_on1x_project.py \
  --on1xc build/on1x-fetch-agent/on1xc --cc gcc \
  --mune-runner build/on1x-fetch-agent/on1x-mune-runner \
  --qmr-compiler build/on1x-fetch-agent/on1x-qmr-compile
python3 on1x/tests/test_project_fetch.py \
  --on1xc build/on1x-fetch-agent/on1xc --cc gcc
ctest --test-dir build/on1x-fetch-agent \
  -R '^on1x\.project_(build|fetch)$' --output-on-failure
cmake -S on1x -B /tmp/opencode/on1x-git-clang -DBUILD_TESTING=ON \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DON1X_USE_FALLBACK_SCANNER=ON -DON1X_WITH_SYREN=OFF -DON1X_WITH_WETSTONE=OFF
cmake --build /tmp/opencode/on1x-git-clang --parallel 4 \
  --target on1xc on1x-mune-runner on1x-qmr-compile
ctest --test-dir /tmp/opencode/on1x-git-clang \
  -R '^on1x\.project_(build|fetch)$' --output-on-failure
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  python3 on1x/tests/test_project_fetch.py \
  --on1xc build/on1x-tooling-asan/on1xc --cc clang
```

The registered project build/fetch pair passed **2/2** with generated-scanner
GCC and a fresh fallback-scanner Clang 22.1.8 build. The HTTPS suite also
passed **11/11** with the existing Clang ASan/UBSan compiler; the complete
project suite was not rerun under sanitizers. This does not establish
authenticated/SSH transport, a registry or version solver, transitive
dependency resolution, non-Linux publication, or a reproducibly pinned
compiler/host C toolchain.

### On1x source formatter and PrettyPrint verbatim documents

Linux x86-64: GCC 16.2.1 with LibStone's generated scanner, Clang
22.1.8 with the fallback scanner, and a Clang ASan/UBSan compiler all
passed `tests/test_formatter.py`. The test compiles original and
formatted On1x programs through strict C11, checks their identical output
and formatter idempotence, exercises invalid input/configuration and
unchanged destinations, compares all three named On1xfile formatter commands
to their CLI equivalents, and renders inert hostile-looking listings with
`pdflatex -no-shell-escape` and `groff -S -Tutf8` when installed. Both
scanner modes also produced the same, byte-idempotent output on 232
checked-in On1x sources at widths 80 and 28.

```sh
ctest --test-dir build/formatter-generated -R '^on1x.formatter$' --output-on-failure
ctest --test-dir build/formatter-fallback -R '^on1x.formatter$' --output-on-failure
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/on1x-tooling-asan -R '^on1x.formatter$' --output-on-failure
make -f prettyprint.mk test BUILD_DIR=build/formatter-prettydoc
make -f prettyprint.mk test-on1x BUILD_DIR=build/formatter-prettydoc \
  ON1XC=build/formatter-fallback/on1xc CC=clang
make -f prettyprint.mk test CC=clang BUILD_DIR=build/formatter-pretty-asan \
  CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all'
```

The source formatter is not a PPX executor, partial parser, or whole-project
rewriter; see its [manual](docs/formatter.md) for the preservation contract.

#### In-place CLI and repository style

Tested on Linux x86-64 using MuLIB `bf23bd1282d280f1935b9ca1898fcb2ab19a7ca4`
plus the formatter changes and LibStone `934b6ed029bfea28bbf23b5dc77c476154b0dd0d`.
`on1x.formatter` exercises both in-place spellings, conflicting arguments,
unchanged input on rejection, file permissions, a literal `-` filename, and
Allman formatting with strict C11 execution before and after rewriting. It
also checks that bounded search and group-fit can choose different layouts.

Applied `on1x/.on1xfmt` to the tracked `.on1x` corpus. All 250 rewritten files
preserve token spellings and dot adjacency and are byte-idempotent with both
scanners. The one
unmodified file, `addons/sublime/syntax_test_on1x.on1x`, deliberately contains
invalid syntax and column-sensitive editor assertions. The three inputs
outside `on1x/` were formatted using temporary copies with the same config.

Full GCC 16.2.1 suite runs initially passed 95/98 (generated) and 94/97
(fallback). The three failures caught an exact-whitespace source assertion
and the pinned SHA-256 of `tests/mulib/elvi-scrape.on1x`. After checking that
the fixture emits identical C11, its evidence hash was refreshed without
changing contract admission, and the diagnostic source check was made
whitespace-tolerant. All three affected tests and `on1x.formatter` then
passed in both builds.

```sh
export TMPDIR="$PWD/build/on1x-final-tmp"
for dir in build/on1x-final-generated-nvme build/on1x-final-fallback; do
  cmake --build "$dir" --parallel 3
  ctest --test-dir "$dir" --output-on-failure --parallel 4
  ctest --test-dir "$dir" --output-on-failure \
    -R '^on1x\.(diagnostic_catalog_source|contracts|mulib|formatter)$'
done
# Clang 22.1.8, fallback scanner, ASan/UBSan:
cmake --build build/cliche-sanitize --target on1xc --parallel 3
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/cliche-sanitize -R '^on1x\.formatter$' --output-on-failure
make -f prettyprint.mk test-on1x BUILD_DIR=build/formatter-prettydoc \
  ON1XC=build/on1x-final-generated-nvme/on1xc CC=cc
```

## Sandbox kernel discovery and archive

MuLIB HEAD `681dacbdd419b71ae86042d0c307180cbd5dea5e` plus sandbox worktree
changes; configured `ON1X_LIBSTONE_ROOT=on1x/third_party/libstone` at the
MuLIB gitlink `57113dc48d815359431dce63a6b865a21dd993cd`. Linux x86-64
6.18.52, glibc 2.44, GCC 16.2.1 (`x86_64-pc-linux-gnu`), Clang/libclang
22.1.8, Python 3.14.7. The clean SDK build reported linked ExoML,
vendored Lua/Kaguya, `/usr/lib/libclang.so`, and the generated scanner.
Commands to configure/build, run CTest, and retain the actual JSON/archive
are in [the sandbox manual](docs/kernel-sandbox.md#reproduce-the-evidence).

- `on1x.kernel_sandbox_sdk`: **passed** (GCC strict C11). Extracted eight
  complete CDecl v2 graphs and validated all 37 annotations, compared the
  inspection and archive evidence, checked one `mulib.o` and actual exports,
  and executed a separate-TU C client (default/invalid profile, grant
  intersection and output preservation, bounded local IPC with binary data).
  The test compares sampled C record sizes, alignment and member offsets
  against CDecl; it is not a complete ABI or callbacks test. Negative cases
  reject archive overwrite, macro override, stale revision, self-claimed
  review, safe/unsafe imports and review subjects without publishing output.
- The same test passed with Clang and ASan/UBSan on the archive and C client:

  ```sh
  sdk=build/sandbox-kernelbind-sdk
  python3 on1x/tests/test_kernel_sandbox_sdk.py \
    "$sdk/libstone-moonstone/exotract/exotract" "$sdk/on1xc" on1x \
    "$sdk/test-artifacts/kernel-sandbox-clang-san" clang \
    --cflag=-fsanitize=address,undefined --cflag=-fno-omit-frame-pointer \
    --cflag=-fno-sanitize-recover=all
  ```

- Final stable-tree GCC selection: **5/5 passed**.

  ```sh
  ctest --test-dir build/sandbox-kernelbind-sdk \
    -R '^on1x\.(kernel_sandbox_sdk|kernel_paths|kernel_review_sdk|kernel_history_sdk|kernel_imports)$' \
    --output-on-failure --parallel 3
  ```

  The separate broader `on1x.kernel_imports_sdk` test reached
  the existing PrettySexpr migration and failed its pre-existing header
  revision check: `include/mulib-prettysexpr.h` SHA-256 at HEAD starts
  `fa7bd9da`, its checked-in sidecar expects `371680e6`.

The verified local artifacts are `build/sandbox-kernelbind-sdk/sandbox-inspection-final.json`
and `build/sandbox-kernelbind-sdk/sandbox-archive-final/`; their recorded input
and archive artifact hashes were rechecked after publication.

Not run by these SDK tests: process launch, seccomp, Landlock, namespace
or delegated cgroup enforcement; AArch64; Chisel usage checking; an
independently issued sandbox review. No sandbox glue has been admitted to
safe On1x. The SDK/compiler themselves were not sanitizer-instrumented.

## KernelBind language surface: lexical unsafe expressions

Baseline MuLIB `2da654cae071226ad246415d2fad4ba43f6f9ec1` plus the prior
KernelBind worktree; LibStone `9c44917b6518c03ef33c544c0ef93b2d864fe4fa`
unchanged. No pricing or kernel C changes for this language slice.
`unsafe { statements; tail }` now lowers to the existing checked value-region
form; a block in statement position keeps its previous behavior. The
post-lowering verifier preserves and rechecks lexical authorization; neither
unsafe block form is accepted as a PPX wire node.

`examples/kernel/clamp.on1x` now exercises both forms. The SDK test compares its
saturating results with direct C and independently checks left-to-right effects,
early function return, an enclosing loop exit, and a closure whose body has its
own unsafe expression. Compiler negatives include out-of-scope calls, unsafe
arguments that don't authorize an outer call, permission not inherited by
lambda bodies, escaping locals, void bindings, malformed/deep syntax, and an
HIR mutation removing the checked scope. The generated-scanner focused run
passed **7/7** on Linux x86-64/GCC 16.2.1:

```sh
cmake --build build/kernelbind-gcc --target on1xc on1x_loader_tests --parallel 6
ctest --test-dir build/kernelbind-gcc -R \
  '(kernel_imports|loader_api|scanner_parity|value_loops|patterns)' \
  --output-on-failure --parallel 4
```

The fallback scanner with Clang ASan/UBSan and the actual ExoLib SDK also
passed **3/3** (`cmake --build build/kernelbind-sanitize --target on1xc
on1x_loader_tests --parallel 6`, then `ctest --test-dir
build/kernelbind-sanitize -R '(kernel_imports|loader_api)' --output-on-failure
--parallel 3`).

The generated-scanner regression selection
`-R '(kernel_review|kernel_history|defer_errors|control_hir|ppx$)'` passed
**6/6**. This checks that preserving unsafe scope markers did not change
reviewed imports or ordinary PPX/control behavior.

Reviewed and ABI-verified unsafe aliases can also coexist, including aliases
for the same clamp kernel. The SDK test verifies one implementation object,
separate reviewed/unsafe manifests, and refusal of an unsafe call outside its
lexical scope even when a reviewed alias is present. The combined generated
GCC integration run after the CSP and source migration changes passed **13/13**:

```sh
cmake --build build/kernelbind-gcc --parallel 6
ctest --test-dir build/kernelbind-gcc -R \
  '(kernel_(review|history|imports)|loader_api|scanner_parity|taskgroup_within|cancellation_deadlines|stream_poll|value_loops|on1x.patterns$|on1x.libraries$|on1x.stdppx$)' \
  --output-on-failure --parallel 4
```

## First-party source migration: control flow

MuLIB `2da654cae071226ad246415d2fad4ba43f6f9ec1` plus the shared
worktree; LibStone `9c44917b6518c03ef33c544c0ef93b2d864fe4fa`, Linux
6.18.52-1-lts x86-64, GCC 16.2.1 and Clang 22.1.8. The
[source audit](docs/source-migration.md) found no further eligible direct
kernel imports. No compiler, kernel, runtime ABI, or review policy changes
are needed for this slice.

Focused tests exercise range boundaries and partial Base64 groups, numeric
and list outcomes, literal/whitespace parsing, byte-escape alternatives and
invalid escapes, value-loop exits after name collisions, and native `unroll`
plugin parity with the Python processor.

```sh
for cc in gcc clang; do
  ctest --test-dir "build/kernelbind-$cc" --output-on-failure --parallel 6 \
    -R '^on1x\.(compiler|data|passes|mir|libraries|stdlib_extensions|control|patterns|relational|stdppx|scanner_parity)$'
done
work=$(mktemp -d /tmp/opencode/on1x-source-native.XXXXXX)
for test in libraries stdppx; do
  python3 "on1x/tests/test_$test.py" build/kernelbind-sanitize/on1xc on1x \
    "$work/$test" clang --cflag=-fsanitize=address,undefined \
    --cflag=-fno-omit-frame-pointer --cflag=-fno-sanitize-recover=all
done
```

Generated/GCC **11/11**, fallback/Clang **10/10** (no parity test in that
build), and both strict C11 Clang ASan/UBSan integration runs passed. The
optional Novostone analyzer was disabled; its disabled-path test passed,
and the `analysis` example ran through scalar MIR. Direct C11 executions of
`control`, `data_model` and `memory`, and MIR executions of `analysis`,
`float_tuples` and `control`, produced the same output as before the source
changes.

### CSP example migration

With the tested ABI v4 compiler/runtime, three examples now use `taskgroup
within 5s`. Fresh C11 executables built using `on1x/tools/on1x-build.py
--on1xc build/on1x-csp-within/on1xc --cc gcc` were compared to `HEAD`.
All exited zero with these unchanged stdout lines:

| Example | Output, one value per line |
|---|---|
| `cancellation_deadlines.on1x` | `90`, `7`, `8` |
| `cooperative_process_streams.on1x` | `79`, `110`, `49`, `120`, `0` |
| `cooperative_stream_poll.on1x` | `1`, `213`, `0` |

```sh
for build in on1x-csp-within on1x-csp-within-generated on1x-csp-within-sanitize; do
  ctest --test-dir "build/$build" --output-on-failure --parallel 4 \
    -R '^on1x\.(concurrency|concurrency_hir|stream_poll|cancellation_deadlines|taskgroup_within|scanner_parity)$'
done
```

GCC/fallback **5/5**, GCC/generated **6/6** including scanner parity, and
Clang ASan/UBSan/fallback **5/5**. The example migration leaves the plain
taskgroup demos, unsupported KernelBind consumers, and pricing unchanged.

## KernelBind Package D: bounded forecasting and price optimization

MuLIB `2da654cae071226ad246415d2fad4ba43f6f9ec1` plus the Package C/D worktree;
LibStone `9c44917b6518c03ef33c544c0ef93b2d864fe4fa` unchanged.
Linux x86-64/glibc 2.44, GCC 16.2.1 and Clang/libclang 22.1.8.
No canonical kernel or LibStone source was modified.

- `include/on1x/kernel.hpp`, `src/{loader,infer,sema,codegen,cli}.cpp` and
  `include/on1x/loader.hpp`: compiler-owned nominal interfaces, bounded
  immutable slice argument checking, checked result shaping, and explicit
  scalar/pointer pairs confined to generated C.
- `contracts-exoml/{histprice,priceopt}.exocontract.json`,
  `contracts-exotract/catalog.json` and `tools/{on1x-build,on1x-exotract}.py`:
  provenance and layout validation against canonical CDecl and ExoML,
  checked projections, native enum/record probes, signed multi-profile
  subjects, and one canonical implementation archive. The program and
  boundary probe link the very same adapter object.
- `examples/kernel/forecast.on1x`, `examples/kernel/pricing/{main,forecasts}.on1x`,
  `tests/test_kernel_history_sdk.py`, `tests/fixtures/kernel_pricing_oracle.c`,
  and `CMakeLists.txt`: executable fixtures and real SDK integration tests.

The profile uses `mulib_histprice_ema`, `mulib_histprice_holt`,
`mulib_priceopt_optimize` and `mulib_priceopt_candidates`. The forecasting
fixture covers zero alpha, unavailable variance, and early `?` failure.
The multi-module pricing fixture converts validated embedded cents, selects
between EMA and Holt using a chronological sample, checks a disjoint holdout,
and executes the tick-grid and discrete candidate APIs. The boundary probe and
the separate direct C client compare both models and both optimizers. Negative
tests cover wrong span element/type, missing review, stale single-profile
approval, record/enum drift, escape and output-initialization obligations,
invalid limits, bad numerical parameters, oversized/negative horizon, empty
and mismatched extents, and unpublished outputs on failure.

```sh
cmake --build build/kernelbind-gcc --parallel 6
ctest --test-dir build/kernelbind-gcc -R \
  '(kernel_(history|imports|review)|loader_api|scanner_parity|defer_errors|nominal_adts)' \
  --output-on-failure --parallel 4
cmake --build build/kernelbind-sanitize --parallel 6
ctest --test-dir build/kernelbind-sanitize -R \
  '(kernel_(history|imports|review)|loader_api|closure_hir|nominal_hir)' \
  --output-on-failure --parallel 4
```

Focused generated/GCC SDK tests passed **8/8** and fallback Clang ASan/UBSan
SDK tests passed **7/7** on the numerical profiles. After adding the explicit
minor-unit/holdout steps and separate C workflow comparison, both
`ctest --test-dir build/kernelbind-gcc -R 'on1x.kernel_history_sdk'`
and the same run in `build/kernelbind-sanitize` passed **1/1**. An intermediate
fixture failed to parse because it used C rather than On1x comment syntax;
both final reruns include the correction. See the
[numerical profile](docs/kernel-numerics.md) for the supported surface and
independent review workflow. Test keys are not production authorizations.
The final full generated/GCC rerun (`ctest --test-dir build/kernelbind-gcc
--output-on-failure --parallel 4`) had **58 passed, 4 failed, 1 skipped**.
All four failures stop on the preexisting Elvi-scrape manual pin mismatch
described below. Category was skipped with native PPX disabled.
Statsprice, Money, demand/ensemble, dynamic buffers, JSON file I/O, owned
handles, callbacks, scheduler integration, reusable installed locks, and
full Shylock parity remain open. §9.5 is explicitly not admitted.

## KernelBind Package C: independently authorized scalar build mechanism

Baseline MuLIB `2da654cae071226ad246415d2fad4ba43f6f9ec1`, plus this working-tree
change; configured LibStone `9c44917b6518c03ef33c544c0ef93b2d864fe4fa`
at `on1x/third_party/libstone`, unchanged. Linux 6.18.52 x86-64, glibc 2.44,
GCC 16.2.1 (`x86_64-pc-linux-gnu`), Clang/libclang 22.1.8, Python 3.14.7,
OpenSSL 3.6.4. This implements the
[reviewed scalar mechanism](docs/kernel-review.md), not all of
`KERNEL_DIRECT_USAGE.md`. The shipped contract remains unreviewed.

Before implementation:

```sh
ctest --test-dir build/kernelbind-gcc \
  -R '(kernel_imports|scanner_parity)' --output-on-failure
```

**2 passed, 1 failed**: the SDK discovery test failed because selecting JSON
validated an unrelated stale `elvi-scrape` manual pin. The planner now validates
the selected contract and dependency closure; it does not refresh or bypass
the stale profile's pins. The same SDK test subsequently passed.

Implementation files:

- `include/on1x/{ast,clone,loader,kernel}.hpp`,
  `src/{loader,sema,codegen,cli,ppx_ast,safety}.cpp`,
  `src/canonical/values.cpp`: typed scalar foreign calls, unresolved staging,
  shape validation, preserved cloning/evaluation order, and closed PPX/checker
  boundaries.
- `tools/{on1x-build,on1x-exotract}.py`: real CDecl/ExoML validation,
  lossless-sidecar checks, operator-selected Ed25519 review verification,
  strict C11 ABI probes, single canonical implementation archive, executable
  comparison gate, source/implementation drift checks, atomic bundle publication.
- `tools/on1x-contracts.py`, `stdlib/mulib/housekeeping.py`: selected-contract
  planning fix.
- `contracts-exoml/clamp.exocontract.json`: unreviewed canonical v2 annotation.
- `examples/kernel/clamp_safe.on1x`, `tests/test_kernel_review_sdk.py`,
  `tests/test_loader.cpp`, `tests/test_kernel_imports_sdk.py`, `CMakeLists.txt`:
  runnable fixture, real SDK tests, malformed HIR and regression integration.

The safe source fixture calls actual `mulib_clamp_add_i64` and
`mulib_clamp_sub_i64` using native `import kernel`, without application externs.
Tests generate a fresh **test authority** key and exercise real signature
verification, strict GCC/Clang C11, direct C equivalence, two independent
extractions, dependency aliases, closure wrappers, left-to-right argument
evaluation, and one-object archive linkage. Every admitted build executes
1,348 oracle comparisons. Negative tests cover wrong/missing review, stale
identity/flags, wrong keys, duplicate JSON fields, silently discarded sidecar
fields, unsupported effects/resources, ABI drift, wrong types/arity,
non-callable members, duplicate implementation TUs, missing adapter definitions,
failed executable probes, changing module resolution, changing implementation
inputs, and non-overwriting publication. Malformed typed HIR rejects at C11
ingress. No production review is created or claimed by these tests.

Commands used:

```sh
cmake --build build/kernelbind-gcc --parallel 6
ctest --test-dir build/kernelbind-gcc --output-on-failure --parallel 4
cmake --build build/kernelbind-clang --parallel 6
ctest --test-dir build/kernelbind-clang --output-on-failure --parallel 4
ctest --test-dir build/kernelbind-clang \
  -R '(kernel_imports|loader_api)' --output-on-failure
cmake --build build/kernelbind-sanitize --parallel 6
ctest --test-dir build/kernelbind-sanitize \
  -R '(kernel_(imports|review)|loader_api|closure_hir|nominal_hir)' \
  --output-on-failure --parallel 3
git diff --check
```

`kernelbind-gcc` uses the generated scanner and real Lua/ExoML/Exotract SDK.
`kernelbind-clang` uses the fallback scanner without the SDK. The sanitizer
configuration uses Clang, the fallback scanner and real SDK with both
`CMAKE_C_FLAGS` and `CMAKE_CXX_FLAGS` set to
`-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all`.
CTest propagates these flags into emitted C11, adapters, implementation archives
and executable probes.

Final results:

| Configuration/check | Result |
|---|---|
| Generated scanner, GCC, full SDK suite | **57 passed, 4 failed, 1 skipped** |
| Fallback scanner, Clang, full non-SDK suite | **53 passed, 2 failed, 1 skipped** |
| Final fallback loader/import regressions | **2/2 passed** |
| Clang ASan/UBSan, compiler/runtime/SDK focused suite | **6/6 passed** |
| Generated/fallback scanner parity | **passed**, included in GCC suite |
| Whitespace and Python syntax checks | **passed** |

The remaining GCC failures are `on1x.exotract`, `on1x.contracts`,
`on1x.mulib`, and `on1x.contracts_sdk`; Clang fails the latter configuration's
`on1x.contracts` and `on1x.mulib`. All stop on the preexisting
`manual/mulib-elvi-scrape.md` source/manual fingerprint mismatch, also observed
in the pre-change discovery baseline. No source pins were refreshed to hide
this failure. Category skips because native PPX is disabled in these profiles.
Initial runs also exposed two test-fixture issues (the copied helper needed
runtime pin inputs; typed diagnostics use “wrong number of arguments”), both
corrected and rerun successfully. The full suite is therefore **not green**.

Remaining gates: independently issued production scalar review; numeric
spans/records/enums/output/error lowering; affine foreign handles and cleanup;
callbacks/capabilities/transport; independent checker and MIR integration;
installed/reusable reviewed import locks; and the §9.5 Shylock application.
No 32-bit, non-Linux or cross-target admission, native backend, or general safe
FFI is claimed. TSAN is inapplicable to this resource-free synchronous profile.

## PrettyPrint: paragraph optimization, maps, math, and incremental formatting

MuLIB `9f52595d958c4377915c92302501cb97b3813b03`; the checkout's LibStone
gitlink is `9c44917b6518c03ef33c544c0ef93b2d864fe4fa`. The On1x checks below
use the existing Lua-enabled `build/kernelbind-gcc` SDK binaries, not a fresh
SDK build. Linux x86_64; GCC 16.2.1 and Clang 22.1.8.

```sh
mkdir -p build/prettyprint-next-tmp
export TMPDIR="$PWD/build/prettyprint-next-tmp"
make -f prettyprint.mk BUILD_DIR=build/prettyprint-next test
make -f prettyprint.mk BUILD_DIR=build/prettyprint-next-san CC=clang \
  CFLAGS='-fsanitize=address,undefined -fno-sanitize-recover=all' test
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
  tests/test_linebreak.c -lm -ldl -o build/prettyprint-next/test_linebreak
build/prettyprint-next/test_linebreak
sdk="$PWD/build/kernelbind-gcc"
make -f prettyprint.mk BUILD_DIR=build/prettyprint-next-sdk \
  ON1XC="$sdk/on1xc" ON1X_BIND="$sdk/on1x-bind" \
  EXOTRACT="$sdk/libstone-moonstone/exotract/exotract" \
  test-on1x test-exotract test-contract test-kernel-discovery
```

The linebreak test compares global optima with exhaustive enumeration on 250
small inputs, including separate first-line widths. PrettyPrint tests cover
ribbons, discretionary hyphens, infeasible layout, UTF-8 map rebasing,
incremental/full equivalence on independent blocks, semantic matrix output
through `eqn` and `pdflatex`, and `.ppg` compilation and failure cases.
The actual On1x fixture runs the new paragraph and math modes through both
single-TU and Exotract archive linkage. The raw companion signature is
unchanged; kernel discovery still does not admit calls. Repository-wide manual
link validation remains blocked by missing `MULIB_VMMPROTO.md`.

## PrettyPrint: Unicode scalar column widths

MuLIB `db7330a875208752ff3b982a5a6474c4e7c8a8bb` and read-only LibStone
`c7cfeca5a5d5086b0c25dad08264fc450c03f85b`; Linux x86_64, GCC 16.2.1,
Clang 22.1.8, Python Unicode database 16.0.0. From the repository root:

```sh
mkdir -p build/prettyprint-followup-tmp
export TMPDIR="$PWD/build/prettyprint-followup-tmp"
python3 tests/generate_pretty_width.py
make -f prettyprint.mk BUILD_DIR=build/prettyprint-followup test
make -f prettyprint.mk BUILD_DIR=build/prettyprint-followup-san CC=clang \
  CFLAGS='-fsanitize=address,undefined -fno-sanitize-recover=all' test
sdk="$PWD/build/kernelbind-gcc"
make -f prettyprint.mk BUILD_DIR=build/prettyprint-followup \
  ON1XC="$sdk/on1xc" ON1X_BIND="$sdk/on1x-bind" \
  EXOTRACT="$sdk/libstone-moonstone/exotract/exotract" \
  test-on1x test-exotract test-contract test-kernel-discovery
```

The native, CLI and SDK targets passed: narrow, wide and nonspacing scalars
affect breaks without changing UTF-8 bytes or annotation offsets. The On1x
boundary executes with both the generated implementation TU and the one-object
Exotract archive. CDecl/ExoML Gate B checks the raw companion; KernelBind
discovery still rejects unadmitted calls. No change to the bridge ABI or claim
of safe FFI, grapheme shaping, or full streaming/optimal layout.

## KernelBind continuation: callable unsafe scalar profile

MuLIB baseline `db7330a875208752ff3b982a5a6474c4e7c8a8bb` and pinned
LibStone `c7cfeca5a5d5086b0c25dad08264fc450c03f85b`, native Linux x86-64,
GCC 16.2.1 and Clang/libclang 22.1.8. See [KernelBind](docs/kernel-bind.md)
for the exact example command and profile limits.
The LibStone checkout moved concurrently to
`9c44917b6518c03ef33c544c0ef93b2d864fe4fa`; the diff does not touch the
ExoML/Exotract sources used here. This change did not modify the gitlink.

`examples/kernel/clamp.on1x` calls the actual `mulib_clamp_add_i64` and
`mulib_clamp_sub_i64`. The SDK test compares ordinary and saturating inputs
with a separate strict C11 caller, asserts exactly one planned implementation
TU, and tests rejection of a duplicate implementation TU. It also rejects
an incompatible signedness change extracted from an isolated checkout,
an implementation macro on the compile command, a safe spelling of the
import, and overwriting an existing executable. The compiler test covers
scope, lambdas, first-class use, wrong members, arity/types, raw externs and
PPX rejection. Run from the repository root with the configurations below:

```sh
mkdir -p build/kernelbind-tmp
export TMPDIR="$PWD/build/kernelbind-tmp"
cmake --build build/kernelbind-gcc --parallel 6
ctest --test-dir build/kernelbind-gcc -j4 --output-on-failure
cmake --build build/kernelbind-sanitize --target on1xc --parallel 4
ctest --test-dir build/kernelbind-sanitize -R '^on1x\.(kernel_imports|kernel_imports_sdk)$' --output-on-failure
```

The profile is explicitly unsafe, not a safe-FFI admission. No legacy
`admitted-extern` certificate is consumed. No staged resource-bearing
stdlib port or Shylock workflow is claimed executable.

## KernelBind Packages A/B baseline: native requests and declaration discovery

MuLIB baseline `db7330a875208752ff3b982a5a6474c4e7c8a8bb`, with concurrent
PrettyPrint worktree changes; configured read-only LibStone
`c7cfeca5a5d5086b0c25dad08264fc450c03f85b` at `on1x/third_party/libstone`,
matching the gitlink (`git submodule status` cannot enumerate it because
there is no mapping in this checkout). Linux 6.18.52-1-lts x86_64, glibc
2.44, GCC/g++ 16.2.1, Clang/clang++/libclang 22.1.8, native compiler triple
`x86_64-pc-linux-gnu`, CMake 4.4.3, Python 3.14.7. The external sibling
LibStone checkout was not used or changed.

The new `kernel_imports` test exercises contextual identifiers and compiles
and runs an ordinary On1x positive control, checks deterministic source
coordinates and hashes, transitive rejection, aliases, malformed source and
budgets, no publication on rejection, and rejection of forged PPX nodes.
The optional `kernel_imports_sdk` test executes **real** LibStone Lua
`exolib.new` and ExoML discovery. Canonical fixture inventory: Histprice 7,
Statsprice 3, Priceopt 5, Money 6, JSON 20 C functions; all remain
`callable:false`. It verifies source pins, deduplication, deterministic reports
and no-replace publication, unknown profiles, shadowed headers, macro
injection rejection, and edited sidecars (forged review string, duplicate JSON
key, stale review/revision, wrong C symbol). JSON is the only attached semantic
sidecar and remains unreviewed.

An additional `mulib.prettysexpr` smoke discovery used
`build/kernelbind-gcc/prettysexpr-discovery.on1x` containing
`import kernel mulib.prettysexpr as Pretty;` and `fn main(){}` with the same
`--inspect-kernels --no-prelude --cc gcc --resource-dir` command documented in
[the KernelBind manual](docs/kernel-bind.md). It verified both required canonical
declarations and `MULIB_IMPL_PRETTYDOC`, `MULIB_IMPL_PRETTYEMIT`,
`MULIB_IMPL_PRETTYSEXPR` in that order. No semantic sidecar was attached;
neither export is callable.

The same SDK test independently uses the existing Exotract archive command
to compile Histprice and Statsproc in one implementation TU (verified with
`ar t`), then compiles `tests/fixtures/kernel_histprice_boundary.c` as a
strict separate caller TU. The C probe checks the actual Holt function
prototype, leaves point/variance unchanged on rejected input, and returns
point 18, variance NaN on its valid series. It is **not** KernelBind-generated
code, a proof of arbitrary C behavior, or the Shylock release gate.

Commands from the MuLIB root (use the NVMe `TMPDIR` if system `/tmp` is full):

```sh
mkdir -p build/kernelbind-tmp
export TMPDIR="$PWD/build/kernelbind-tmp"
cmake -S on1x -B build/kernelbind-gcc -DBUILD_TESTING=ON \
  -DON1X_BUILD_NATIVE_PPX=OFF -DON1X_REQUIRE_LIBSTONE=ON \
  -DON1X_LIBSTONE_ROOT="$PWD/on1x/third_party/libstone" -DON1X_WITH_EXOML=ON \
  -DON1X_WITH_EXOML_EXTRACT=ON -DON1X_WITH_EXOTRACT=ON -DON1X_WITH_EXOTRACT_LUA=ON
cmake --build build/kernelbind-gcc --parallel 4
ctest --test-dir build/kernelbind-gcc --output-on-failure --parallel 4
ctest --test-dir build/kernelbind-gcc -R '(kernel_imports|scanner_parity)' --output-on-failure --parallel 3
python3 on1x/tools/on1x-build.py on1x/examples/hello.on1x \
  --on1xc build/kernelbind-gcc/on1xc --cc gcc -o build/kernelbind-gcc/on1x-hello
build/kernelbind-gcc/on1x-hello

CC=clang CXX=clang++ cmake -S on1x -B build/kernelbind-clang -DBUILD_TESTING=ON \
  -DON1X_BUILD_NATIVE_PPX=OFF -DON1X_USE_FALLBACK_SCANNER=ON
cmake --build build/kernelbind-clang --parallel 4
ctest --test-dir build/kernelbind-clang --output-on-failure --parallel 4

san='-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all'
CC=clang CXX=clang++ cmake -S on1x -B build/kernelbind-sanitize -DBUILD_TESTING=ON \
  -DON1X_BUILD_NATIVE_PPX=OFF -DON1X_USE_FALLBACK_SCANNER=ON \
  -DON1X_WITH_EXOML=ON -DON1X_WITH_EXOML_EXTRACT=ON \
  -DON1X_WITH_EXOTRACT=ON -DON1X_WITH_EXOTRACT_LUA=ON \
  -DCMAKE_C_FLAGS="$san" -DCMAKE_CXX_FLAGS="$san"
cmake --build build/kernelbind-sanitize --target on1xc on1x-exotract --parallel 4
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/kernelbind-sanitize -R kernel_imports --output-on-failure
```

Focused GCC/generated SDK + parser + scanner tests **3/3**; 303 exact
scanner-parity cases. Fallback/Clang parser test **1/1**; fallback/Clang
ASan+UBSan focused **2/2**, instrumenting the frontend, SDK, and C oracle and
archive (not the system libclang DSO). `hello` printed `21`. Early builds in
`/tmp/opencode/kernelbind-*` failed because `/tmp` was full. Baseline concurrent
worktree suites were not clean: pre-existing `on1x.mulib` fixture fingerprint
failure, then-in-progress PrettyPrint `sexpr_nat` resolution in
`on1x.libraries`, and input-drift rejections when another writer changed
`MANIFEST.yaml`. No tests or root artifacts were changed to mask failures.
After the concurrent inputs stopped drifting, full GCC/generated SDK
**59 passed, 1 failed, 1 skipped** (61), and Clang/fallback
**54 passed, 1 failed, 1 skipped** (56). In each, the only failure was
`on1x.mulib` (stale `tools/embed_units.py` outputs), and Category was skipped
because native PPXs were disabled.

At this baseline checkpoint the native compiler gate was closed for both safe and unsafe calls. The safe
scalar, raw unsafe, ownership/callbacks, aggregate/target ABI, link selection
from On1x and Shylock release gates are not exercised or met. See
[KernelBind discovery](docs/kernel-bind.md) for the exact input and output
contract.

## M5/M7: integrated MuLIB package, search lists and manifests

Baseline MuLIB `b11af815facb35a791d9721c4c654df21b877d94`; actual configured,
read-only LibStone `c7cfeca5a5d5086b0c25dad08264fc450c03f85b` at
`on1x/third_party/libstone`. Linux 6.18.52-1-lts x86_64, glibc 2.44,
GCC/g++ 16.2.1, Clang/clang++ 22.1.8, CMake 4.4.3, Python 3.14.7.
No dependency checkout, kernel, source contract or generated fixture was updated.

### Behavior delivered

- Build, extraction, installation and tests use the existing `stdlib/mulib`
  package, replacing stale references to the removed `std/mulib`. Its source API
  remains `std.mulib`, with the existing raw trust boundary.
- `ON1X_LIB_PATH`, `ON1X_STDLIB_PATH`, `ON1X_PPX_PATH`,
  `ON1X_STDPPX_PATH`, `ON1X_MULIB_PATH` and matching repeatable CLI options
  provide bounded colon lists. Existing `-I`, explicit source/PPX paths,
  `--stdlib`, `ON1X_PATH`, `ON1X_STDLIB` and `ON1X_MULIB_ROOT` remain supported.
- The canonical loader resolves manifested library source directories and
  standard-library roots. `std.*`, selective `from std` and the implicit
  prelude agree. The helper preserves mixed CLI path order and asks the compiler
  for Basis resources, including fallback standard roots.
- `src/discovery.cpp` reuses the existing S-expression reader for declarative
  version-1 `On1xfile` metadata. The standard library and MuLIB package advertise
  their source roots; native PPX build/install bundles advertise executables and
  schemas. All 24 shipped Python processors have terse second-line manifests.
  New discovery rejects missing/malformed metadata, name/protocol/version
  mismatches, duplicate/unknown fields, escaping/missing manifest inputs and
  resource exhaustion. Explicit-path legacy scripts remain compatible.
- Name-based PPX activation (`--use-ppx`, `--stdppx`, `--ppx LABEL NAME` and
  authorized source registrations) uses configured search directories without
  granting implicit execution. Native schemas are supplied per process.
  Category's pinned extension registry is independent of ordinary PPX lookup.
- The shared MuLIB planner records actual selected header paths/hashes and
  include ordering across directories. Implementation code, C11 callers and
  Exotract discovery use those headers. MANIFEST-backed macro/dependency and
  companion planning, source-contract pins and archive drift checks remain in
  force; exactly one kernel implementation TU is retained.

The [discovery contract](docs/search-paths.md) documents manifest syntax, exact
precedence, compatibility and unsupported cases. There is no new language
syntax, runtime ABI, resource proof, safe import or native-machine backend.

### Baseline and measured results

The pre-change GCC/generated suite was **49 passed, 7 failed** of 56:
install/Category failed on the absent `std/` install directory, and MuLIB,
stdlib-extension and three concurrency suites hit stale `std/mulib` paths.
Baseline XML: `/tmp/opencode/on1x-values-gcc/results-paths-baseline.xml`.

| Final profile | Result |
|---|---|
| Strict GCC Debug, generated scanner, native PPXs, complete suite | **56 passed, 1 failed** (57 tests) |
| Strict Clang Debug, fallback scanner, native PPXs, complete suite | **55 passed, 1 failed** (56 tests) |
| Clang generated ASan/UBSan, affected discovery/modules/loader/PPX/install/extensions/parity | **7/7**, no sanitizer findings |
| Real existing Exotract executable, selected-header archive and compiled On1x caller, GCC | **PASS** |
| Same focused SDK boundary, instrumented compiler/archive/adapter/program, Clang ASan/UBSan | **PASS**, no sanitizer findings |

The sole complete-suite failure is `on1x.mulib`, now reaching the previously
documented unrelated `tools/embed_units.py` stale-unit-profile fixture check.
No failing test was disabled, no fixture hash was weakened, and no unrelated
generated kernel was regenerated.

`tests/test_discovery.py` compiles actual On1x clients and executes library,
prelude, Basis-runtime, script/native PPX and multi-root kernel paths through
strict C11. It tests CLI/environment ordering, empty/missing directories,
spaces, mixed `-I`/`--lib-path`, explicit-root compatibility, namespace imports,
fallback resources, source-PPX authorization, dependency-only nonexecution,
native schema lookup, installed relocation, preserved output on rejection,
manifest traversal/symlink attacks, missing/wrong metadata and finite budgets.
Kernel callers use the real bitset implementation via a separate C adapter;
reports check exact selected headers and single implementation switches.

Development reruns exposed a test-fixture-state error: a deliberately created
first-root header from one run affected the next run. Discovery tests now use a
fresh automatically cleaned workspace. An additional mixed-path regression
first reproduced `42` instead of expected `41`; preserving helper CLI order
fixes it. Earlier `results-paths-final.xml` reports retain these development
failures; `results-paths-complete.xml` contains the complete final suites above.
A tool-output log hit disk exhaustion while several old `/tmp` trees coexisted.
Only this task's discovery artifacts were moved into ignored
`build/on1x-paths-evidence/`; later temporary compilations use the directory
below. No unrelated build trees or user files were removed.

### Exact commands and configurations

Existing `/tmp/opencode/on1x-values-{gcc,clang}` configurations were retained:
Debug, `BUILD_TESTING=ON`, `ON1X_BUILD_NATIVE_PPX=ON`,
`ON1X_LIBSTONE_ROOT=/mnt/nvme/mulib/on1x/third_party/libstone`,
`CMAKE_{C,CXX}_FLAGS=-Wall -Wextra -Wpedantic -Werror`.
GCC uses generated scanning (`ON1X_USE_FALLBACK_SCANNER=OFF`), Clang uses fallback
(`ON1X_USE_FALLBACK_SCANNER=ON`). Optional SDK/diagnostic adapters are OFF.
The sanitizer tree `/tmp/opencode/on1x-category-sanitize` retains Clang Debug,
generated scanning, native PPXs and testing, adds
`-fsanitize=address,undefined -fno-omit-frame-pointer` to both compiler flag sets,
and sets
`ON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer`
to instrument generated programs and C adapters as well.

For each tree `/tmp/opencode/on1x-values-gcc` and
`/tmp/opencode/on1x-values-clang`:

```sh
cmake --build <tree> --parallel 4
TMPDIR=/mnt/nvme/mulib/build/on1x-paths-tmp \
  ctest --test-dir <tree> --output-on-failure --parallel 2 \
  --output-junit <tree>/results-paths-complete.xml
```

The temporary directory was created explicitly before these commands. Sanitizer:

```sh
cmake --build /tmp/opencode/on1x-category-sanitize --parallel 4
TMPDIR=/mnt/nvme/mulib/build/on1x-paths-tmp \
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir /tmp/opencode/on1x-category-sanitize --output-on-failure \
  --parallel 2 -R '^on1x.(discovery|modules|loader_api|ppx|install|stdlib_extensions|scanner_parity)$' \
  --output-junit /tmp/opencode/on1x-category-sanitize/results-paths-complete.xml
```

After retaining the Windows-only legacy `ON1X_PATH` semicolon parser, each of
the three compilers was rebuilt with `--target on1xc`; discovery/modules were
rerun with the same environments and `-R '^on1x.(discovery|modules)$'`
(`results-paths-last.xml`). Windows itself was not executed.

Focused SDK reproduction (`test_kernel_paths.py` is also registered when
`ON1X_WITH_EXOTRACT=ON`):

```sh
TMPDIR=/mnt/nvme/mulib/build/on1x-paths-tmp \
  python3 on1x/tests/test_kernel_paths.py \
  /tmp/opencode/on1x-values-sdk/libstone-moonstone/exotract/exotract \
  /tmp/opencode/on1x-values-gcc/on1xc "$PWD/on1x" \
  "$PWD/build/on1x-paths-kernel-sdk" gcc
TMPDIR=/mnt/nvme/mulib/build/on1x-paths-tmp \
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  python3 on1x/tests/test_kernel_paths.py \
  /tmp/opencode/on1x-values-sdk/libstone-moonstone/exotract/exotract \
  /tmp/opencode/on1x-category-sanitize/on1xc "$PWD/on1x" \
  "$PWD/build/on1x-paths-kernel-sanitize" clang \
  --cflag=-fsanitize=address,undefined --cflag=-fno-omit-frame-pointer
```

The real Exotract executable was reused, not rebuilt or claimed to certify the
entire current SDK. Its SHA-256 is
`27d04e5011c8e024de971e5a5bdad87f951870db5eaa7c7bb0bece280888a566`.
The focused test extracts the selected external header, builds an actual
one-object archive, runs the linked On1x client, then mutates the header and
requires drift rejection with the old executable intact. Other optional
Chisel/Nerotypical/Novostone/Syren checks were **UNSUPPORTED/not exercised** in
these core profiles, not reported as green SDK coverage.

Python compilation and `git diff --check` pass. New discovery remains
**EXECUTABLE**, not VERIFIED general packaging: non-Linux/cross-host PPX,
32-bit targets, new static/shared runtime profiles, native-source auto-building,
download/lockfile/dependency resolution, unknown unmanifested kernel recipes,
process sandboxing and general foreign-resource safety were not implemented or
certified. TSAN is not applicable evidence: no threaded runtime was changed.

## Category M2 continuation: checked concrete evidence, processor v2

Baseline MuLIB `2ff743469c62b6a369b21bcbfb045e05b30d27f3`; actual configured,
read-only LibStone `c7cfeca5a5d5086b0c25dad08264fc450c03f85b`. Linux
6.18.52-1-lts x86_64, glibc 2.44; GCC/g++ 16.2.1, Clang/clang++ 22.1.8,
CMake 4.4.3, Python 3.14.7. No dependency revision was updated.

### Implemented behavior

- Native On1x `stdlib/ppx/category/{metadata,typed}.on1x` now builds scoped
  concrete member requests and plans against host-inferred signatures.
  `stdppx/native/protocols.on1x` runs separately bounded syntax/evidence phases.
- `src/extension_evidence.cpp` independently checks substitution, completeness,
  local canonical-head uniqueness, provenance, exact signature equality and
  direct selector forwarding. `check`, C11 emission and MIR lowering enforce
  this admission. Erasing evidence or redirecting a selector to a different
  same-typed function rejects.
- Loader renaming, cloning and ordinary PPX reconstruction retain host-owned
  requests. `--emit-category-plan` exposes sorted choices and source/artifact
  fingerprints. Plans are read-only responses bound to the live snapshot,
  never accepted as cached proofs or authority to change types.
- Existing `Unit` v1 PPX framing remains compatible; additive evidence schema
  roots match in the canonical/standalone copies. Bundled Protocols binary/pin
  version is 2; old installations require a coordinated rebuild.
- Actual `eq.on1x`, `ordering.on1x` and new `evidence.on1x` execute under strict
  C11. The last prints `9\n3\n3\n9\ntrue\n7\n`, exercising reordered multiple
  members, named alternatives, function values, void results and single
  left-to-right operand evaluation. Eq also runs through MIR→C11.

The native planner has an independently compiled authoring-API client.
Negative packets cover type mismatches, missing/extra facts, missing members,
unknown interfaces, binder/version/origin/digest errors and fact exhaustion.
Host tests reject stale source snapshots, missing/reordered/forged choices,
erased or nonlocal requests, mutated types/dispatch/argument order and null
selector operands at both C11 and MIR ingress. Integration tests also cover
ordinary PPX composition, CLI/source activation, alias identity, changed source
digests, hostile phase/schema environment, preserved output on errors and
relocated installations.

### Reproduction and results

Existing Debug configurations were rebuilt: `/tmp/opencode/on1x-values-gcc`
uses GCC/generated scanner, `/tmp/opencode/on1x-values-clang` uses
Clang/fallback scanner. Both enable native PPXs and testing, use
`-Wall -Wextra -Wpedantic -Werror` for C/C++, and disable optional SDK adapters.
Configured LibStone root: `/mnt/nvme/mulib/on1x/third_party/libstone`.

```sh
cmake --build /tmp/opencode/on1x-values-gcc --parallel 4
ctest --test-dir /tmp/opencode/on1x-values-gcc --output-on-failure --parallel 4 \
  --output-junit /tmp/opencode/on1x-values-gcc/results-category-final.xml
cmake --build /tmp/opencode/on1x-values-clang --parallel 4
ctest --test-dir /tmp/opencode/on1x-values-clang --output-on-failure --parallel 4 \
  --output-junit /tmp/opencode/on1x-values-clang/results-category-final.xml
```

The pre-change GCC/generated baseline was **54 passed, 1 failed**.
The initial Clang run overlapped schema edits: **51 passed, 3 failed**, including
two temporary schema-copy mismatches; it is not a clean baseline.
The final GCC/generated suite is **55 passed, 1 failed**; the final
Clang/fallback suite is **54 passed, 1 failed**. Its schema-copy
failures are resolved. The remaining failure, also present in the GCC baseline,
is `on1x.mulib`: `fixture/generated kernel mismatch: tools/embed_units.py`
(`stale unit profiles`). No test was disabled to conceal this failure.
Focused GCC Category integration and evidence HIR tests pass **2/2**.

Fresh core-only Clang ASan/UBSan configuration and exact command:

```sh
cmake -S on1x -B /tmp/opencode/on1x-category-sanitize \
  -DBUILD_TESTING=ON -DON1X_BUILD_NATIVE_PPX=ON -DON1X_REQUIRE_LIBSTONE=ON \
  -DON1X_LIBSTONE_ROOT=/mnt/nvme/mulib/on1x/third_party/libstone \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug \
  '-DCMAKE_C_FLAGS=-Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer'
cmake --build /tmp/opencode/on1x-category-sanitize --parallel 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir /tmp/opencode/on1x-category-sanitize --output-on-failure \
  --parallel 2 -R 'category|extension_evidence|ppx|meta_hir|scanner_parity' \
  --output-junit /tmp/opencode/on1x-category-sanitize/results-category-final.xml
```

The prior SDK-enabled sanitizer configuration was also attempted with
`cmake --build /tmp/opencode/on1x-values-sanitize --parallel 4`; compilation
stopped in unchanged `src/safety.cpp:271` because `TaskGroup`/`CspScope` cases
are unhandled under `-Werror,-Wswitch`. No SDK sanitizer tests ran. Optional
Nerotypical/Chisel Category checks report **UNSUPPORTED** in the core profiles.

### Remaining scope

This is a concrete **M2 slice**, not completion of `CATEGORY.md`. Imported or
persisted declaration/witness interfaces, constrained generics, associated
types, multi-parameter source protocols, signatures/functors/sealing, derives/
laws, ownership/effect-aware auto protocols and advanced abstractions remain
unsupported. Reports describe host/type compatibility, not target ABI, laws or
safety proofs. Other platforms/cross-host PPX execution, 32-bit targets and
separate compilation were not exercised. No new threaded runtime or foreign
ABI is introduced; TSAN is not applicable to this slice.
See [the manual and phase/status table](docs/category.md).

## Monotonic `taskgroup within` (CSP ABI v4)

Base MuLIB `2da654cae071226ad246415d2fad4ba43f6f9ec1`, LibStone
`9c44917b6518c03ef33c544c0ef93b2d864fe4fa`; Linux 6.18.52-1-lts x86_64,
glibc 2.44, GCC/g++ 16.2.1, Clang/clang++ 22.1.8, CMake 4.4.3, Python 3.14.7.
The starting parser rejected `taskgroup within`. The bounded C11 profile now
checks a one-shot `i64` duration (`ms`, `s`, or bare milliseconds), retains
normal explicit affine consumption, and exits 73 at the expiry source point
only after cooperative cancellation and draining. There is no typed timeout
or auto-join. See [the exact deadline semantics](docs/cancellation-deadlines.md#taskgroup-within).

`tests/fixtures/taskgroup_within.on1x` executes a nested timed rendezvous,
disarms deadlines on return/break/continue, and prints `10\n42\n7\n8\n`.
`test_taskgroup_within.py` runs strict C11 (`-Wall -Wextra -Wpedantic
-Werror`) with the real MuLIB scheduler, deterministic link-time monotonic
clock injection and a real-clock captured `/bin/cat` timeout. Tests exercise
duration evaluation once, optimized C11, zero before admission, blocked
join/receive/poll cancellation, nested precedence and ties, busy loop
observation, unrelated operands not continuing, explicit cancel retiring a
deadline, shielded cleanup, negative/overflow/bad clock/OOM, foreign
rejection, affine rejection, and transactional MIR rejection. CSP HIR tests
round-trip the source PPX carrier and reject forged type/binder/unsafe markers
at verification and C11 emission. The existing ABI mismatch test rejects a
v3 header when compiling v4 C. The generated C's scheduler entry symbol is
versioned, so stale runtime objects cannot silently link.

```sh
common='-DBUILD_TESTING=ON -DON1X_BUILD_NATIVE_PPX=OFF'
cmake -S on1x -B build/on1x-csp-within $common -DON1X_USE_FALLBACK_SCANNER=ON
cmake -S on1x -B build/on1x-csp-within-generated $common -DON1X_REQUIRE_LIBSTONE=ON
cmake -S on1x -B build/on1x-csp-within-sanitize $common -DON1X_USE_FALLBACK_SCANNER=ON \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer'
for dir in build/on1x-csp-within{,-generated,-sanitize}; do
  cmake --build "$dir" --parallel 4 --target on1xc on1x_concurrency_hir_tests
  if [[ "$dir" == *-generated ]]; then
    cmake --build "$dir" --parallel 4 --target on1x_reference_tokens on1x_generated_tokens
  fi
  ctest --test-dir "$dir" -R 'on1x\.(concurrency(_hir)?|stream_poll|cancellation_deadlines|taskgroup_within|scanner_parity)$' \
    --output-on-failure -j 4
done
```

Focused results: **5/5** GCC fallback, **6/6** GCC generated including scanner
parity, **5/5** Clang fallback ASan/UBSan (compiler, generated C and runtime).
After the separate example migration, the GCC fallback cancellation-deadline
and stream-poll suites passed again (**2/2**).
Intentional non-unwinding fault-injection paths disable only leak detection;
ordinary expiry retains it. No claim of a full unrelated suite, SDK proof,
TSan/native worker coverage, other supported targets or general async.

## Cooperative cancellation and monotonic poll deadlines (historical CSP ABI v3)

Baseline MuLIB `1f027c48bc6363dce5752c00057830777325ff47`; configured
read-only LibStone `55755485acaded7c4ed804c0946bc8c7363f6763`, unchanged.
Linux 6.18.52-1-lts x86_64, glibc 2.44; GCC/g++ 16.2.1, Clang/clang++
22.1.8. Existing CMake 4.4.3 / Python 3.14.7 configurations below were reused.
Baseline focused fallback concurrency/HIR/stream-poll: **3/3**. Earlier source
diagnostics rejected `after` and any nonadjacent internal cancellation.

**Scope delivered:** [running/blocked cooperative cancellation and monotonic
poll deadlines](docs/cancellation-deadlines.md), not the entire request.
**Async/await is not implemented.** The existing MIR has no suspension/frame/
future/effect descriptors; no stackful wrapper is represented as stackless async.
In this ABI-v3 increment, taskgroup `within` and typed cancelled join outcomes
were still unsupported; the ABI-v4 section above supersedes the former only.

### Behavior and boundaries

- `cancel` consumes a same-parent internal handle, requests cancellation and
  waits for cleanup; true before completion, false after completion. Pre-dispatch
  cancel skips the body as before. Running/blocked tasks return through generated
  cleanup edges: function defers LIFO, remaining temporary/lexical owners, call
  depth accounting and nested taskgroup cleanup. No longjmp or discarded C stack.
  RC-return/indirect-call cancellation cannot invoke a dummy null callable.
  A 262,144-unit generated-cleanup budget bounds edge/release expansion and
  rejects exhaustion with located `ON1X-CSP-LIMIT` metadata before publication.
- Cleanup is shielded. Nested children are cancelled/joined and owned external
  processes use the existing SIGTERM/250ms/SIGKILL/reap path. Noncooperative code
  and nonterminating cleanup cannot be forcibly interrupted. Already committed
  communication is not rolled back. Fatal errors still do not unwind.
- Each bounded task owns a private cancel channel. A blocked communication offers
  it alongside its ordinary cases in **one existing MuLIB atomic selection**.
  Cancelled/losing registrations are removed before generated cleanup can release
  storage. Ordinary join/process waits observe cancellation and return into the
  same cleanup mechanism.
- One `after` accepts i64 milliseconds or `ms`/`s` units. Duration evaluates once
  after send operands. The supervisor closes a private timer channel at its
  absolute CLOCK_MONOTONIC deadline; expired timer/default/communications use the
  kernel's ready-choice arbitration. Timer-only polls are supported. No change to
  MuLIB's UTC park/select API, no readiness-probe loop or duplicate scheduler.
  Supervisor idle waits use bounded relative nanosleep, not UTC fiber sleep.
- Private CSP ABI is **v3**; generated C checks its header. Rebuild compiler,
  generated C and runtime together. PPX-v1 adds source operation `offer_after`
  within the existing generic constructor, never typed/canonical authority.
  Malformed/duplicate carriers reject with located errors.

Source changes: `src/{parser,infer,sema,ppx_ast,concurrency,codegen}.cpp`,
`runtime/on1x_concurrency.[ch]`, CMake. Tests:
`tests/test_cancellation_deadlines.py`, `test_concurrency.{cpp,py}` and
`fixtures/poll_{wallclock,allocator}.c`; example:
`examples/cancellation_deadlines.on1x`.
No MuLIB kernel, LibStone checkout, raw/safe FFI admission or MIR version change.

### Commands and results

The three `on1x-csp-*` trees retain the exact configurations from the preceding
stream-poll section below: GCC default/fallback, GCC default/generated, and Clang
fallback with both compiler and generated-program
`-fsanitize=address,undefined -fno-omit-frame-pointer`
(`ON1X_TEST_C_FLAGS` explicitly instruments emitted C and fixtures).

```sh
cmake --build /tmp/opencode/on1x-csp-check --parallel 2
ctest --test-dir /tmp/opencode/on1x-csp-check --output-on-failure --parallel 4 \
  --output-junit /tmp/opencode/on1x-csp-check/results-cancellation-final.xml
cmake --build /tmp/opencode/on1x-csp-generated --parallel 2
ctest --test-dir /tmp/opencode/on1x-csp-generated --output-on-failure --parallel 4 \
  --output-junit /tmp/opencode/on1x-csp-generated/results-cancellation-final.xml
cmake --build /tmp/opencode/on1x-csp-sanitize --parallel 2 \
  --target on1xc on1x_concurrency_hir_tests
ctest --test-dir /tmp/opencode/on1x-csp-sanitize --output-on-failure \
  -R '^on1x.(concurrency_hir|concurrency|stream_poll|cancellation_deadlines)$' \
  --output-junit /tmp/opencode/on1x-csp-sanitize/results-cancellation-final.xml
cmake --build /tmp/opencode/on1x-values-release --parallel 2 \
  --target on1xc on1x_concurrency_hir_tests
ctest --test-dir /tmp/opencode/on1x-values-release --output-on-failure \
  -R '^on1x.(concurrency_hir|cancellation_deadlines)$' \
  --output-junit /tmp/opencode/on1x-values-release/results-cancellation-final.xml
```

| Profile | Result |
|---|---|
| GCC fallback, full suite | **52 passed, 1 failed, 1 skipped** (54 registered) |
| GCC generated, full suite | **53 passed, 1 failed, 1 skipped** (55 registered) |
| Clang ASan+UBSan, focused | **4/4** |
| Strict GCC Release fallback, focused | **2/2** |

The sole full-suite failure remains the baseline stale `tools/embed_units.py`
fixture fingerprint in `on1x.mulib`. Category skips with native PPX disabled.
No tests were disabled: the previous `after`/late-cancel rejection fixtures were
replaced by executable positives plus wrong-duration/use-after-consumption
negatives. Sanitizers cover compiler, generated C, runtime, actual kernels and
fixtures. Normal paths retain leak checking; only deliberate terminal allocation/
negative-duration failures disable it. ASan's known makecontext support warning
is accepted, not an error report. TSAN is not applicable evidence to this
one-native-thread runtime; MuLIB has no TSAN fiber annotations.

Tests include 16 repeated blocked send/receive/timed-select cancellations,
loop and nested-call resumption, LIFO defers and RC callable cleanup, nested
children, process/byte-wait cleanup, timer/default/communication arbitration,
duration side effects/order/once, completed-cancel behavior, preservation of
outputs on errors, malformed HIR/wire and MIR refusal. Link-time `timespec_get`
perturbation jumps UTC between epochs while a real monotonic deadline expires.
Every calloc failure of a bounded timer program is injected, including new
cancel-signal, timer and kernel selection registration allocations.
The suite also rejects an incompatible CSP ABI header and a deliberately
oversized cleanup expansion, preserving an existing output file. An async
declaration remains rejected; this negative test is not async implementation.

Standalone strict Clang C11:

```sh
python3 on1x/tools/on1x-build.py on1x/examples/cancellation_deadlines.on1x \
  --on1xc /tmp/opencode/on1x-csp-generated/on1xc --cc clang \
  -o /tmp/opencode/on1x-cancel-example --cflag=-O2
/tmp/opencode/on1x-cancel-example
ldd /tmp/opencode/on1x-cancel-example
python3 on1x/tests/scanner_parity.py \
  /tmp/opencode/on1x-csp-generated/on1x_generated_tokens \
  /tmp/opencode/on1x-csp-generated/on1x_reference_tokens \
  on1x/examples /tmp/opencode/on1x-cancel-parity
```

Output: `90`, `7`, `8`. Only libc/Linux loader dependencies; no LibStone, Lua,
C++ runtime or compiler library. Exact scanner parity: **302 cases**.
The same example executes through freshly rebuilt native identity PPX and strict
GCC Debug/generated (the existing `on1x-values-gcc` strict configuration):

```sh
cmake --build /tmp/opencode/on1x-values-gcc --parallel 2 --target on1x_ppx_identity
ON1X_PPX_SCHEMA="$PWD/on1x/grammar/PPX.schema.json" \
  python3 on1x/tools/on1x-build.py on1x/examples/cancellation_deadlines.on1x \
  --on1xc /tmp/opencode/on1x-values-gcc/on1xc --cc gcc \
  --ppx identity /tmp/opencode/on1x-values-gcc/stdppx/on1x-ppx-identity \
  -o /tmp/opencode/on1x-cancel-native-ppx
/tmp/opencode/on1x-cancel-native-ppx
```

Unsupported: async/await/futures and verified suspension frames, taskgroup-wide
deadlines, general error/effect/ownership MIR, native workers, other platforms
and independent SDK safety checking. These capabilities are not promoted by
this increment. Status is EXECUTABLE, not VERIFIED full M4 conformance.

## G4 continuation — exact mixed i64/u8 atomic process-stream poll

Re-inspected clean baseline MuLIB `d4cc478df76bfe9b5b8e6485f42d56fde81e8a68`
and actual configured LibStone
`55755485acaded7c4ed804c0946bc8c7363f6763` at
`on1x/third_party/libstone` (unchanged/read-only). Linux 6.18.52-1-lts
x86_64, glibc 2.44, GCC/g++ 16.2.1, Clang/clang++ 22.1.8,
CMake 4.4.3, Python 3.14.7. No SDK revision, MuLIB kernel or FFI admission change.

Scope: [atomic process-stream poll](docs/stream-poll.md), not completion of
`ON1X_CORE_SYNTAX_ADDITIONS.md`. The specification now has an explicit
per-feature implementation ledger, including the existing function-defer
profile's difference from proposed scope-exit semantics. Baseline compilation
of `examples/cooperative_stream_poll.on1x` failed at its byte send:
`8:13: expected sender<i64>, got sender<u8>`. Baseline fallback/GCC full suite:
**50 passed, 1 failed, 1 skipped** (52 registered); JUnit:
`/tmp/opencode/on1x-csp-check/results-byte-poll-baseline.xml`.

Inference, sema and independent affine/canonical admission now reconstruct each
offer's exact direction/payload and sum result type. The C11 emitter explicitly
converts the selected transport value to its checked receive type. Runtime
private ABI v2 retains int64 transport, but each byte case points to a separate
actual uint8_t object alive through the whole atomic selection; it does not
alias an endian-dependent byte of the int64 representation. No allocation,
second scheduler/channel, shell interpreter or new PPX wire format is added.
One MuLIB lock still commits at most one offer and removes losers. Wrong
runtime byte direction/range rejects before a copy. Existing kernel budgets,
process ownership and fatal non-unwinding error behavior remain unchanged.

Tests:

- `tests/test_stream_poll.py`: seeded (`2026111801`) permutation of all 256
  bytes twice, NUL/high-bit/255 round-trip at stream capacity 0/1/7, strict C11
  `-O0`/`-O2`, mixed i64/u8 sends/receives, wide negative i64 preservation,
  losing offers/payload once/order, endpoint reuse, default, FIFO, independent
  stdout/stderr EOF, moved reader fibers and post-process-cancel close.
- Source negatives reject wrong direction, wrong payload/binder/result type,
  duplicate/missing close, moved endpoints and unsupported scalar MIR, preserving
  existing outputs. Python identity PPX is compiled and executed.
- `tests/test_concurrency.cpp`: source/wire/canonical round-trip, exact u8 result
  type and private HIR mutations (wrong result/payload/direction, missing child)
  rejected by verification and emission. Existing i64 concurrency tests remain.
- `tests/fixtures/stream_poll_boundary.c`: separately compiled actual runtime/
  kernel/C and C++ callers; malformed negative/256 byte values, direction errors
  and injected selection-registration OOM reject with located exit-73 failures.
  One fixture TU owns all three MuLIB implementation switches.

Exact configurations/commands from the repository root:

```sh
cmake -S on1x -B /tmp/opencode/on1x-csp-check -DBUILD_TESTING=ON \
  -DON1X_BUILD_NATIVE_PPX=OFF -DON1X_USE_FALLBACK_SCANNER=ON
cmake --build /tmp/opencode/on1x-csp-check --parallel 4
ctest --test-dir /tmp/opencode/on1x-csp-check --output-on-failure --parallel 4 \
  --output-junit /tmp/opencode/on1x-csp-check/results-byte-poll-final.xml

cmake -S on1x -B /tmp/opencode/on1x-csp-generated -DBUILD_TESTING=ON \
  -DON1X_BUILD_NATIVE_PPX=OFF -DON1X_USE_FALLBACK_SCANNER=OFF
cmake --build /tmp/opencode/on1x-csp-generated --parallel 4
ctest --test-dir /tmp/opencode/on1x-csp-generated --output-on-failure --parallel 4 \
  --output-junit /tmp/opencode/on1x-csp-generated/results-byte-poll-final.xml

cmake -S on1x -B /tmp/opencode/on1x-csp-sanitize -DBUILD_TESTING=ON \
  -DON1X_BUILD_NATIVE_PPX=OFF -DON1X_USE_FALLBACK_SCANNER=ON \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer'
cmake --build /tmp/opencode/on1x-csp-sanitize --parallel 4 \
  --target on1xc on1x_concurrency_hir_tests
ctest --test-dir /tmp/opencode/on1x-csp-sanitize \
  -R '^on1x.(concurrency_hir|stream_poll|concurrency)$' --output-on-failure \
  --output-junit /tmp/opencode/on1x-csp-sanitize/results-byte-poll-final.xml
```

Additional strict GCC Debug/generated/native-PPX and Release/fallback trees
retain the `on1x-values-{gcc,release}` configurations recorded below
(`-Wall -Wextra -Wpedantic -Werror` for C/C++; Debug enables native PPX,
Release disables it). Commands executed:

```sh
cmake --build /tmp/opencode/on1x-values-release --parallel 4 \
  --target on1xc on1x_concurrency_hir_tests
ctest --test-dir /tmp/opencode/on1x-values-release \
  -R '^on1x.(stream_poll|concurrency_hir)$' --output-on-failure \
  --output-junit /tmp/opencode/on1x-values-release/results-byte-poll-final.xml

cmake --build /tmp/opencode/on1x-values-gcc --parallel 4 \
  --target on1x_ppx_identity
ON1X_PPX_SCHEMA="$PWD/on1x/grammar/PPX.schema.json" \
  python3 on1x/tools/on1x-build.py on1x/examples/cooperative_stream_poll.on1x \
  --on1xc /tmp/opencode/on1x-values-gcc/on1xc --cc gcc \
  --ppx identity /tmp/opencode/on1x-values-gcc/stdppx/on1x-ppx-identity \
  -o /tmp/opencode/on1x-byte-poll-native-ppx
/tmp/opencode/on1x-byte-poll-native-ppx

python3 on1x/tools/on1x-build.py on1x/examples/cooperative_stream_poll.on1x \
  --on1xc /tmp/opencode/on1x-values-gcc/on1xc --cc clang \
  -o /tmp/opencode/on1x-byte-poll-clang --cflag=-O2
/tmp/opencode/on1x-byte-poll-clang
ldd /tmp/opencode/on1x-byte-poll-clang
python3 on1x/tools/on1x-build.py on1x/examples/hello.on1x \
  --on1xc /tmp/opencode/on1x-values-gcc/on1xc --cc gcc \
  -o /tmp/opencode/on1x-byte-poll-hello
/tmp/opencode/on1x-byte-poll-hello
```

Strict Release/fallback focused: **2/2**. Native identity PPX/GCC and standalone
Clang C11 both print `1`, `213`, `0`; the latter links only libc and the Linux
loader, not LibStone, On1x, C++, Lua or a shell runtime. Hello retains output `21`.
Clang ASan+UBSan focused: **3/3**, including instrumented compiler, generated
programs, runtime/kernels and C/C++ consumers. Normal paths retain leak checking;
only intentional terminal non-unwinding failures disable it. The known ASan
makecontext/swapcontext support warning is not a sanitizer finding.
Exact scanner parity: **301 cases**, including the new source example:

```sh
python3 on1x/tests/scanner_parity.py \
  /tmp/opencode/on1x-csp-generated/on1x_generated_tokens \
  /tmp/opencode/on1x-csp-generated/on1x_reference_tokens \
  on1x/examples /tmp/opencode/on1x-byte-poll-parity
python3 -m py_compile on1x/tests/test_stream_poll.py on1x/tests/test_concurrency.py
git diff --check
```

Completed final suites (JUnit files are `results-byte-poll-final.xml` in the
trees above):

| Profile | Result |
|---|---|
| GCC / fallback, complete suite | **51 passed, 1 failed, 1 skipped** (53 registered) |
| GCC / generated, complete suite | **52 passed, 1 failed, 1 skipped** (54 registered) |
| Clang / fallback ASan+UBSan, focused concurrency/stream-poll | **3/3** |
| Strict GCC Release / fallback, focused HIR/stream-poll | **2/2** |

The sole complete-suite failure is unchanged from baseline:
`on1x.mulib` rejects a stale `tools/embed_units.py` fixture fingerprint.
`on1x.category` skips with native PPX disabled. No test was disabled to obtain
these results. The first final build+suite commands exceeded their shell
120-second timeout; the completed CTest-only reruns used a 600-second command
timeout and supersede those interrupted attempts.
An additional seed-`2026111802` truncation/single-byte-deletion corpus exercised
101 variants of the stream-poll example with each final scanner/compiler:
no crashes, located errors and preserved outputs on rejection. This is bounded
robustness evidence, not general fuzzing or conformance proof.

Development attempts: a Type/Type::Kind mismatch was caught by compilation and
fixed. An unrolled full-byte corpus exhausted the canonical flow-state budget;
the fixture now uses bounded source loops, without raising compiler limits.
Two negative-test diagnostic expectations and the PID prefix on ASan's known
makecontext warning were corrected; sanitizer errors remain failures. Strict
compiler builds also exposed three existing misleading-indentation warnings
in CSP parser/PPX decoding; statements were split without semantic changes.

Unsupported: monotonic `after`, running/blocked internal cancellation and unwind,
general byte-channel construction/duplex/cloning, owning payloads, full portable
shell grammar, async/await/generators/actors/native threads, effectful MIR and
independent SDK safety models. Optional SDK integrations are not exercised by
this increment. TSAN is not applicable evidence for this single-native-thread
profile (the underlying fiber runtime lacks TSAN fiber annotations).
The bounded feature is **EXECUTABLE**, not full-specification VERIFIED.

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

## Bounded diagnostics, scrutiny, and PPX continuation — 2026-11-18

Validated from MuLIB `681dacbdd419b71ae86042d0c307180cbd5dea5e`
plus the working-tree changes, with configured LibStone
`57113dc48d815359431dce63a6b865a21dd993cd` left unchanged. Host:
Linux 6.18.52-1-lts x86-64, GCC 16.2.1 and Clang 22.1.8.

This continuation adds the LibStone-independent catalog/registry core, bounded
Wetstone translation, Syren presentation and repair ranking, scrutiny
precedence, and descriptor-3 PPX advisories described in
[diagnostics.md](docs/diagnostics.md). The analysis accepts only conservative
straight-line checked `i64`/`bool` prefixes. Checked overflow, dynamic checked
add/subtract/multiply, negation of `INT64_MIN`, and `INT64_MIN / -1` stop the
translation rather than fabricating a value or finding. Repairs remain
compiler-validated proposals, not intent or semantic-equivalence proofs.

The final security pass additionally verifies that PPX messages and captured
stderr cannot inject terminal controls, forged lines, or bidi overrides.
Malformed UTF-8 is escaped. PPX-controlled locations cannot authorize host
filesystem reads. Source snapshots remain 2 MiB each and share a monotonic
16 MiB bundle/lookup budget, so repeated findings cannot amplify snapshot
retention or lookup work without bound. JSON/SARIF remain deterministic and
SARIF uses UTF-16 columns only when a matching loaded-graph snapshot is retained.

| Configuration | Measured result |
|---|---|
| `/tmp/opencode/on1x-diagnostics-build`: GCC Debug, generated scanner, native PPXs, Syren and Wetstone | **67/69 passed** |
| `/tmp/opencode/on1x-diagnostics-fallback`: GCC Debug, fallback scanner, native PPXs, no Syren/Wetstone | **64/66 passed** on the final complete rerun |
| `/tmp/opencode/on1x-diagnostics-sanitize`: Clang Debug, generated scanner, Syren/Wetstone, ASan/UBSan | Focused diagnostics/analysis/PPX/inference/loader **7/7 passed**, no sanitizer finding |
| Strict `-Wall -Wextra -Werror -pedantic` GCC fallback and Clang generated builds | Focused **2/2** and **4/4 passed** |

The generated and fallback policy/compatibility selections passed **10/10** and
**8/8**, covering diagnostics, analysis, adversarial PPX, inference, loader and
all project build/fetch/driver tests available in each profile. An earlier
parallel fallback run saw a GCC internal compiler segfault while compiling
unchanged `runtime/on1x_runtime.c` for `poly_extended`; the isolated test passed,
the 64-test run excluding the known fingerprint checks passed **64/64**, and the
final complete rerun produced the **64/66** result above.

In both complete profiles the only final failures are `on1x.contracts` and
`on1x.mulib`. Both reject the unrelated
`manual/mulib-elvi-scrape.md` source/manual fingerprint drift. The audited
fingerprint was not refreshed, bypassed, or weakened.

Reproduction:

```sh
cmake --build /tmp/opencode/on1x-diagnostics-build --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-diagnostics-build \
  --output-on-failure --parallel 3 \
  --output-junit /tmp/opencode/on1x-diagnostics-build/results-diagnostics-final.xml

cmake --build /tmp/opencode/on1x-diagnostics-fallback --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-diagnostics-fallback \
  --output-on-failure --parallel 3 \
  --output-junit /tmp/opencode/on1x-diagnostics-fallback/results-diagnostics-final-rerun.xml

ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir /tmp/opencode/on1x-diagnostics-sanitize \
  -R '^on1x\.(diagnostics(_api)?|analysis_api|ppx|inference(_engine)?|loader_api)$' \
  --output-on-failure --parallel 2

ctest --test-dir /tmp/opencode/on1x-diagnostics-strict-gcc \
  -R '^on1x\.(analysis_api|ppx)$' --output-on-failure
ctest --test-dir /tmp/opencode/on1x-diagnostics-strict-clang \
  -R '^on1x\.(diagnostics(_api)?|analysis_api|ppx)$' --output-on-failure
```

This is **EXECUTABLE**, not a whole-language verifier or mature diagnostics
platform. CFG/loop/interprocedural reasoning, ownership/resource proofs,
incremental/LSP publication, cross-platform PPX sidebands, and whole-suite
sanitizer coverage remain unsupported.

## Catalog-v2 data ownership — 2026-11-18

This is the follow-up to the bounded-diagnostics continuation above. Its earlier
67/69 and 64/66 totals are historical, not validation of this refactor. Current
checkout identity: MuLIB `681dacbdd419b71ae86042d0c307180cbd5dea5e` plus
uncommitted/concurrent work, configured LibStone
`57113dc48d815359431dce63a6b865a21dd993cd` unchanged. Linux
6.18.52-1-lts x86-64; GCC/g++ 16.2.1 (`20260810`), Clang/clang++ 22.1.8.

### Implemented and exercised boundary

- `stdlib/diagnostics/catalog.json` uses `on1x.diagnostic-catalog/v2`. It owns
  new diagnostic IDs, levels and failure behavior, message templates, provider/
  origin labels, category/phase/guidance/remediation, scrutiny names/checks/
  activation/overrides, upstream mappings, source-repair policies and replacement
  lists, advisory labels, validation/command-failure text and PPX declarations.
  Implementation enums select numeric closed mechanics, not user-visible names.
- CMake validates the trusted catalog with
  `tools/generate-diagnostic-catalog.py` before generating its embedded header.
  Malformed external catalogs report the trusted embedded role entry, never
  partially parsed untrusted bootstrap text. A new reporting-limit entry handles
  oversized legacy message expansion without recursive exceptions/abort.
- Compiler `match` keys are independent of presented IDs. Runtime replacements
  change compiler and PPX codes, levels and messages, metadata/colors, scrutiny/
  check names and activation, level failure semantics and repair candidates,
  without rebuilding the compiler or native diagnostic PPX.
- PPX emit records contain **only ID and location** after registration. Free-form
  message payloads reject. Native declaration ID and failure text come from the
  host-selected catalog. Fatal PPX findings keep their registered identity and
  are reported once. PPX paths still do not authorize source-file reads.
- Wetstone repair analysis remains default-on but its rationale/proposal notes
  are explicitly advisory, not compiler-revalidated edits. `--repair` separately
  recompiles each Syren edit through load/parse/check/C-emission with the selected
  catalog, before any “validated” label or exclusive publication. Legacy compiler
  exceptions conservatively yield inconclusive outcomes instead of classification
  by message substrings. Intent/equivalence is never proved.
- New unconditional CTests `on1x.diagnostic_catalog_source` and
  `on1x.diagnostic_catalog_runtime` cover both reporting configurations. The
  former source-lints feature/upstream code literals, diagnostic level/provider/
  policy text, repair replacement policy, AST defaults and PPX signatures, and
  exercises status-only generator rejection and compiles a non-BMP Unicode
  embedded-header regression. The latter checks replacement/bootstrap,
  oversize-message handling, optional analysis and native PPX replacement.
  API tests additionally cover catalog copy/move ownership, bounded numeric
  colors, invalid edit/cost rejection, validator outcomes and snapshot limits.

These changes also fixed two regressions exposed during verification:
compiler-owned ANSI prefixes were being escaped as untrusted text, and oversized
legacy diagnostic text could throw while reporting. Both have executable
regressions; no warning or test was disabled to hide them.

### Final measured results

| Configuration | Result after the final code changes |
|---|---|
| `/tmp/opencode/on1x-diagnostics-build`: GCC Debug, generated scanner, Syren/Wetstone and native PPXs | Full suite **70/71 passed** |
| `/tmp/opencode/on1x-diagnostics-fallback`: GCC Debug, fallback scanner, no Syren/Wetstone, native PPXs | Full suite **67/68 passed** |
| `/tmp/opencode/on1x-diagnostics-strict-gcc`: GCC Debug/fallback, no optional analysis/presentation or native PPXs, `-Wall -Wextra -Werror -pedantic` | Focused **4/4 passed** |
| `/tmp/opencode/on1x-diagnostics-strict-clang`: Clang Debug/generated, Syren/Wetstone, native PPXs OFF, same strict flags | Focused **6/6 passed** |
| `/tmp/opencode/on1x-diagnostics-sanitize`: Clang Debug/generated, Syren/Wetstone and native PPXs, ASan/UBSan | Focused **9/9 passed**, no sanitizer findings |

The only failure in each full suite is now `on1x.mulib`,
`tests/test_mulib.py:109`: the OptML fixture build rejects
`fixture/generated kernel mismatch: tools/embed_units.py` / `stale unit profiles`.
The previously recorded manual-fingerprint failure is not the current failure:
concurrent work changed that boundary, and `on1x.contracts` passes in these runs.
Unrelated artifacts, fingerprints and sandbox/TTY/Git/prelude work were not
regenerated, reverted or repaired by this follow-up.

Reproduction (existing trees retain the settings in the table):

```sh
cmake --build /tmp/opencode/on1x-diagnostics-build --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-diagnostics-build \
  --output-on-failure --parallel 3 \
  --output-junit /tmp/opencode/on1x-diagnostics-build/results-catalog-v2.xml
cmake --build /tmp/opencode/on1x-diagnostics-fallback --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-diagnostics-fallback \
  --output-on-failure --parallel 3 \
  --output-junit /tmp/opencode/on1x-diagnostics-fallback/results-catalog-v2.xml

cmake --build /tmp/opencode/on1x-diagnostics-strict-gcc --parallel 4
ctest --test-dir /tmp/opencode/on1x-diagnostics-strict-gcc \
  -R '^on1x\.(diagnostic_catalog_(source|runtime)|analysis_api|ppx)$' \
  --output-on-failure \
  --output-junit /tmp/opencode/on1x-diagnostics-strict-gcc/results-catalog-v2.xml
cmake --build /tmp/opencode/on1x-diagnostics-strict-clang --parallel 4
ctest --test-dir /tmp/opencode/on1x-diagnostics-strict-clang \
  -R '^on1x\.(diagnostics(_api)?|diagnostic_catalog_(source|runtime)|analysis_api|ppx)$' \
  --output-on-failure \
  --output-junit /tmp/opencode/on1x-diagnostics-strict-clang/results-catalog-v2.xml

cmake -S on1x -B /tmp/opencode/on1x-diagnostics-sanitize \
  -DON1X_BUILD_NATIVE_PPX=ON \
  '-DON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer'
cmake --build /tmp/opencode/on1x-diagnostics-sanitize --parallel 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-diagnostics-sanitize \
  -R '^on1x\.(diagnostics(_api)?|diagnostic_catalog_(source|runtime)|analysis_api|ppx|inference(_engine)?|loader_api)$' \
  --output-on-failure --parallel 2 \
  --output-junit /tmp/opencode/on1x-diagnostics-sanitize/results-catalog-v2.xml
```

The sanitizer tree additionally uses
`CMAKE_C_FLAGS` and `CMAKE_CXX_FLAGS` =
`-fsanitize=address,undefined -fno-omit-frame-pointer -g`; thus the compiler,
generated native PPXs, generated C11 clients and their runtime TUs are
instrumented, not just the test driver. Source-lint/generator checks and Python
syntax compilation also pass.

### Truthful limits

This remains **EXECUTABLE**, not whole-language diagnostic migration or proof.
Legacy compiler phases, old runtime/SDK errors and project-driver/OS failures
still contain literal messages/IDs. They are outside this feature except for
catalog routing; unknown compiler events use the data-defined fallback.
Normal suggestions remain candidates, not prevalidated edits. JSON is supported;
there is no YAML catalog parser. No whole-suite sanitizer, TSAN, AArch64,
non-Linux, CFG/interprocedural/resource analysis or LSP evidence is claimed.
See [diagnostics.md](docs/diagnostics.md) for the schema/protocol migration.

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

## Pervasive generic Option/Result facade

Validated 2026-11-18 from MuLIB
`681dacbdd419b71ae86042d0c307180cbd5dea5e` plus the working-tree changes,
with configured LibStone
`57113dc48d815359431dce63a6b865a21dd993cd` left unchanged. Host:
Linux 6.18.52 x86-64, glibc 2.44, GCC 16.2.1, Clang 22.1.8,
Python 3.14.7 and CMake 4.4.3.

The default pervasive environment now exports canonical bounded nominal
`Option<T>`/`Result<T,E>` declarations and the documented `option_*`/`result_*`
helpers. They are ordinary demand-loaded On1x declarations. No parser,
typechecker, runtime or ABI change was required. The dedicated test exercises
every exported helper, generic specializations, branch laziness, contextual
phantom inference failures, Result propagation, explicit/custom-prelude and
local shadowing, `--no-prelude`, direct core imports, and all three legacy
representations.

Measured results:

- Fallback-scanner Debug build completed. Focused CTest
  (`prelude|modules|libraries|nominal_hir|nominal_adts|defer_error_hir|defer_errors|inference|inference_engine|install`)
  passed **10/10**.
- Generated-scanner Debug build completed, including all native PPX generated
  C programs. The same focused set plus scanner parity passed **11/11**;
  native/Python standard-PPX integration and protocol tests passed **2/2**.
- `test_prelude.py` passed with generated scanning and strict Clang C11
  `-fsanitize=address,undefined -fno-omit-frame-pointer
  -fno-sanitize-recover=all`; no sanitizer finding was reported.
- `on1x-build.py examples/pervasive_options.on1x` compiled and linked the
  example, which printed `42`. `on1x.libraries` admitted all 132 current
  non-test `.on1x` library modules, and `stdlib/tests/check_package.py` retained
  unique exported implementation names across 138 total `.on1x` files.
- The complete fallback CTest run was **51 passed, 10 failed, 1 skipped**.
  `on1x.prelude` and every focused compatibility suite above passed. The failures
  were outside this change: seven concurrent diagnostics/PPX expected-text
  mismatches (`compiler`, `functions`, `closures`, `tuples`,
  `parameter_patterns`, `macros`, `ppx`), the pre-existing
  `manual/mulib-elvi-scrape.md` fingerprint drift (`contracts`, `mulib`), and
  concurrent native-input drift in `stream_poll`. These excluded areas were not
  modified as part of this facade.
- A strict warnings-as-errors generated-compiler build was additionally
  attempted. It stopped in unrelated concurrent `loader.cpp`, `sema.cpp` and
  `analysis.cpp` warning failures; rebuilding that same generated profile
  without host `-Werror` succeeded, and generated applications themselves were
  compiled with strict warning flags by the focused tests.

Reproduction:

```sh
cmake -S on1x -B /tmp/opencode/on1x-prelude-build \
  -DBUILD_TESTING=ON -DON1X_USE_FALLBACK_SCANNER=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/opencode/on1x-prelude-build --parallel 4
ctest --test-dir /tmp/opencode/on1x-prelude-build \
  -R '^on1x\.(prelude|modules|libraries|nominal_hir|nominal_adts|defer_error_hir|defer_errors|inference|inference_engine|install)$' \
  --output-on-failure --parallel 4

cmake -S on1x -B /tmp/opencode/on1x-prelude-generated \
  -DBUILD_TESTING=ON -DON1X_USE_FALLBACK_SCANNER=OFF \
  -DON1X_WITH_SYREN=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/opencode/on1x-prelude-generated --parallel 4
ctest --test-dir /tmp/opencode/on1x-prelude-generated \
  -R '^on1x\.(prelude|modules|libraries|nominal_hir|nominal_adts|defer_error_hir|defer_errors|inference|inference_engine|install|scanner_parity)$' \
  --output-on-failure --parallel 4

python3 on1x/tests/test_prelude.py \
  /tmp/opencode/on1x-prelude-generated/on1xc "$PWD/on1x" \
  /tmp/opencode/on1x-prelude-clang-sanitize clang \
  --cflag=-fsanitize=address,undefined --cflag=-fno-omit-frame-pointer \
  --cflag=-fno-sanitize-recover=all
```

This remains the nonrecursive, trivially copied nominal profile. Managed or
resource-bearing payloads/drop, Option propagation, recursive ADTs, and
aggregate MIR/SDK lowering remain unsupported.

## Earlier artifact-only verification (historical)

The original extension artifact reported the following before local compiler
integration. These statements retain their historical scope, not current status:

- GitHub source/API read of current `Chubek/mulib` plus `janestreet/base/src` and `ocaml-batteries-team/batteries-included/src` directories.
- C bridge tested with GCC and Clang using `-std=c11 -Wall -Wextra -Werror -pedantic -fsyntax-only` **against signature-matching local mock headers**. This proves only C syntax and basic type compatibility of the bridge under these mocks.
- The On1x compiler was not executable locally; no native On1x unit tests or PPX binaries were compiled/run.
- Actual MuLIB archive generation, linkage and ABI/contract validation are pending.
- No claimed complete Base/Batteries parity, no crypto/security certification, and no generalized safe FFI.
