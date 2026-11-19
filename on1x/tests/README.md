# Tests

CTest registers independent tests with separate artifact directories:

- `on1x.concurrency_hir`: source-wire round-trip, canonical CSP admission,
  mutation/provenance rejection, C11 emission and explicit MIR refusal.
- `on1x.concurrency`: compiled taskgroup/fiber and 1:1 i64 channel programs,
  atomic `poll`, immediate internal and process-group cancellation, compiler-parsed
  command literals, captured byte stdin/stdout/stderr, rendezvous/FIFO/close,
  repeated lifecycle, backpressure/EPIPE, defer/return cleanup, negative affine/
  capability/syntax profiles and located deadlock/budget/spawn failures. Both
  scanner modes and focused sanitizer runs are recorded in
  [verification](../VERIFICATION.md); broader async/cancellation/shell profiles
  remain unsupported.

- `on1x.defer_error_hir`: source-wire round-trip, transactional lowering,
  idempotence, private thunk shape/capture mutations, tag/initialization and
  exact-return/function-boundary checks, rejected private wire nodes.
- `on1x.defer_errors`: Go-style function-scoped LIFO registration and nominal
  Result propagation through strict C11, seeded outcome/LIFO oracles at O0/O2,
  captures/loops/recursion, modules/generics/macros/PPX/staging, counted RC/JSON
  cleanup, C/C++ separate-TU callers, runtime/fault/budget/trap boundaries and
  explicit unsupported MIR/SDK rejections. See [the contract](../docs/defer-errors.md).

- `on1x.nominal_hir`: named declaration/type-reference wire round-trips,
  nominal/phantom/forall identity, layout/catalog tampering, malformed binders,
  transactional rejection and deterministic rechecking.
- `on1x.nominal_adts`: actual nonrecursive records/parameterized enums through
  strict C11, C/C++ separate translation units sharing a generated interface,
  seeded Python result oracles, generic/forall/import identity, source-order
  constructors, guarded/exhaustive matching, macros/staging/native and Python
  PPX, closure capture cleanup/OOM and ownership/layout/budget/ABI negatives.
  Not recursive ownership, separate On1x compilation or aggregate foreign ABI.

- `on1x.nominal_hir`: declaration/type source-wire round-trip, nominal and phantom
  identity, forall argument sharing, malformed wire/layout/catalog mutations,
  transactional failure and deterministic rechecking.
- `on1x.nominal_adts`: strict C11 execution of named records and parameterized
  enums, generics/forall/imports, matching, value loops, macro hygiene/staging,
  closure cleanup/OOM, C/C++ separate-TU aggregate calls using one generated
  interface, seeded value oracles, output preservation, bounded layout failures
  and MIR rejection. This is not recursive ownership or error propagation.

- `on1x.recursive_owning_adts_hir`: finite `own<Named>` identity, checked
  whole/projected transfer metadata, canonical moved-state mutation rejection,
  explicit MIR/PPX closure, unique-runtime C++ ABI checks and 20,000-node
  iterative destruction.
- `on1x.recursive_owning_adts`: generic recursive lists and mixed shared/unique
  nominal values through strict C11; modules, calls/returns, overwrite and
  control-exit cleanup, active sum arms, owner-bearing Result propagation with
  non-owning defers, nested/error/partial-construction and allocator-failure
  cleanup, 50,001-node iterative consumption and destruction, ABI mismatch,
  shadowing and unsupported-boundary negatives.
  See [the ownership contract](../docs/recursive-owning-adts.md).

- `on1x.value_loop_hir`: additive loop/value-break source-wire round-trip,
  deterministic rechecking, result initialization, conditional/repeatable writes,
  header/backedge mutations, nearest lexical targets and function boundaries.
- `on1x.value_loops`: actual intrinsic expression loops through strict C11 and
  plain/optimized MIR C11/evaluation/serialization; ten independent seeded search
  oracles, nested/labeled exits, for latches, numeric/aggregate/closure results,
  cleanup/fault injection/JSON, macros/identity PPX/staging/generics/imports,
  raw separate-TU calls and unsupported profiles. This is not owning lists,
  defer/typed errors, concurrency or nominal ADTs. See [value loops](../docs/value-loops.md).

- `on1x.stdlib_extensions`: validated persistent Option/Result/maps/graphs, signed
  scanning and no-snapshot searches, canonical MuLIB data/hash adapters with
  recoverable errors, per-group C compilation, counted allocation/budget failure
  cleanup, numeric-IPv4 loopback binary reception, EOF and reused/stale handles.
- `on1x.mulib`: kernel/fixture inventory, real YANTLR provider execution, OptML's
  shipped policy, separate Shiraz runtime/registry linkage, embedded lmuw3, shared
  JSON, native
  metadata and host-size bounds, binary fixtures, relocation, bundle fingerprints,
  stale embedding/missing-file/symlink rejection and preserved outputs on failure.
- `on1x.loader_api`: module/depth/source budgets, deterministic dependencies and
  invocation-local caches that observe edited files.
- `on1x.modules`: named/selective/relative/quoted imports, isolated namespaces,
  package initializers and re-exports, canonical symlink/diamond identity, cycles,
  search roots, automatic/custom preludes, shadowing, compatible raw externs,
  macros/staging, PPX ordering/authorization, adapter linking and output preservation.
- `on1x.libraries`: compiler admission of all library modules, four Basis On1x
  programs, native adapters, Python arithmetic oracle, generic helpers, callback
  ordering, malformed-list/bounds errors, snapshot budgets and allocation failures.
- `on1x.stdppx`: all 24 Python processors through compiler-generated C11 execution,
  all fifteen native processors, seven new native/Python behavior comparisons,
  compatibility forms and composed examples, bounded unroll, mutable
  callbacks, early-return timing, injected clock boundaries/failures and
  post-transform type failures.
- `on1x.stdppx_protocol`: Python unittest suite for all standard processors,
  subprocess protocol, canonical schema parity and rejected malformed transforms.
- `on1x.install`: install into a temporary prefix, relocate it, resolve installed
  dependencies and compile/run clients with the installed helper, PPX, MuLIB
  housekeeping, a standard-library INI consumer and an external MuLIB kernel/fixture checkout.

- `on1x.relational_hir` (Novostone enabled): seven Datalog queries against
  independent MIR graph/liveness algorithms and the upstream naive evaluator,
  40 seeded CFGs, recursive calls, loop phis, parallel edges, full-width IDs and
  constants, fact/catalog tampering, revision isolation, deletion/cache invalidation,
  snapshot lifetime, transactional failure and bounded queries.
- `on1x.relational`: opt-in CLI, deterministic source/saved-MIR artifacts and exact
  provenance, query exhaustion and output preservation. Disabled builds check
  explicit rejection. See [analysis builds/API](../docs/backend-analysis.md).
- `on1x.mir_hir`: typed scalar MIR verification, malformed CFG/SSA/phi/effect/slot
  negatives, canonical round-trip, source identity, independent evaluator budgets,
  simultaneous loop-phi transfers and transactional lowering failure.
- `on1x.mir`: HIR-C11/MIR-C11/reference-evaluator differentials; source and saved-IR
  execution, 120 seeded arithmetic cases, boundary traps, ordered effects,
  branches/loops/recursion, imports/macros/PPX, unsupported-subset rejection and
  independent build-helper execution. Generated artifacts include SHA-256 digests.

- `on1x.scanner`: bounded C buffers, copied token lifetime, exact byte locations,
  source limits, repeated failures/successes and concurrent serialized scans.
- `on1x.compiler`: native strict C11 lowering, imports, recursion, shadowing,
  evaluation order, precedence, type errors, lexical drops and arithmetic traps.
- `on1x.types`: structural function/tuple equality, ordering, owned shape storage,
  nested types, cached hole/ownership metadata, diagnostics and API guards.
- `on1x.inference_engine`: concrete higher-order grounding, idempotent checking,
  occurs-check rejection, all four lowered budget failures and transactional
  failure behavior of the inference pass.
- `on1x.inference`: twelve native programs covering omitted annotations,
  nested holes, higher-order calls, mutual recursion, imports, lexical shadowing,
  aliases and cleanup. Twenty-eight semantic and two syntax rejections, graph
  depth/expanded-size failures, bounded long-name diagnostics, and deterministic
  `--types` inspection exercise inferred types and deferred admission checks.
- `on1x.functions`: eleven native first-class/higher-order function programs,
  including aliasing, mutable bindings, lexical shadowing, function factories,
  nested signatures, callee/argument order, recursion, imports and lexical JSON
  cleanup. Thirty semantic rejection cases are separate from three syntax
  cases; raw function values, callbacks and resource signatures fail closed.
- `on1x.closure_hir`: grounded lambda signatures, first-use/transitive captures,
  lexical shadowing, transactional inference failure and idempotent checking.
- `on1x.closures`: twenty native programs covering escaping immutable captures,
  nested closures, function captures/results, aliases/self-assignment, scalar
  types, snapshots, local mutation, recursion, imports and name collisions.
  Counting allocator hooks check normal-exit leaks and precise expression/loop
  release timing. Twenty-nine semantic and four syntax rejections are separate.
  Failure injection, runtime count/size guards, internal ABI-version rejection,
  deterministic emission/type listings, and destruction of 50,000 generated /
  100,000 isolated runtime environments exercise the ownership/ABI boundary.
- `on1x.tuple_hir`: pattern-driven structural inference, capture ownership
  metadata, idempotent checking, occurs checks and transactional type-budget
  failures, including the outer declaration signature.
- `on1x.tuples`: twenty-three native programs covering scalar/nested products,
  grouping, patterns, shadowing, inference, higher-order signatures, mutable
  whole bindings, evaluation order, escaping/transitive captures, shared closure
  fields, expression/condition cleanup, imports, JSON-local cleanup, generated
  names and a 50,000-environment tuple-capture chain. Thirty-four semantic and
  eight syntax rejections are separated. Deep products/shared type DAGs also
  exercise pointer-based helpers, bounded emitted C, and type depth/size limits.
- `on1x.limits`: inclusive source/tree limits, excessive blocks, parentheses,
  unary/binary expressions, postfix calls, function/tuple types, tuple values,
  local patterns and lambda bodies,
  plus inclusive lambda-count/capture-count and transitive capture-name budgets.
  Successful bounded trees/function types/lambdas also compile natively.
- `on1x.json`: canonical parent MuLIB JSON integration, nested views and lexical
  cleanup (also through inferred functions, closure-local roots and tuple results), numeric
  bounds, malformed inputs and allocation failures. The
  `json_alloc.c` fixture replaces the normal implementation TU rather than
  linking a second copy. Fatal `or_die` paths do not promise unwinding.
- `on1x.parameter_patterns`: five native programs, thirteen semantic rejections,
  nested function/lambda bindings, inference, lexical collisions/shadowing,
  escaping fields, borrowed-argument cleanup, loops, deterministic C and deep
  pattern limits. `on1x.tuple_hir` also checks parameter-pattern idempotence.
- `on1x.poly`: nine native programs for local schemes, aliases, nested schemes,
  annotations, tuple values, weak variables, escaping capture cleanup, and flat
  scalar extern calls. Fifteen semantic rejections plus three specialization/
  environment/depth growth regressions distinguish invalid/expansive/mutable
  values and unsupported foreign leaves. `on1x.inference_engine` additionally
  checks prototype typing/idempotence, transactional instance/clone failures and
  an independently bounded specialization recursion test. Existing closure
  negatives now test mutable lambdas rather than rejecting admitted schemes.
- `on1x.poly_extended`: twenty-five native programs cover generic globals/imports,
  self/mutual/overlapping recursive components, nested leaf schemes, explicit
  universal function/lambda/pattern parameters, forwarding/return/local annotations,
  rigid proofs, function-valued instantiations and universal bundles. Counting
  allocators check escaping captures, loop cleanup and fatal allocation failures.
  Twenty-three semantic/syntax rejections check predicativity, distinct rigid
  binders, value restriction, overload domains, weak/captured environments,
  resources, raw ABI rejection and forbidden inferred polymorphic recursion.
  Dependency-depth diagnostics and deterministic public type listings/C output
  are checked separately. HIR tests also check forall idempotence and transactional
  failure of demand elaboration. Old monomorphic-global/destructuring negatives
  now exercise mutable/weak cases or are replaced by positive scheme tests.
- `on1x.meta_hir`: canonical C++/ASDL-backed S-expression round trips, source AST
   reconstruction/C11 equivalence, syntax subtyping, idempotent checking and
   transactional macro/staging/inference failure. Typed code shapes and staged
   wire round trips are checked. Malformed wire input fails before reconstruction.
- `on1x.macros`: native macro examples, context-mark hygiene across lambdas,
  definition-site globals/intrinsics, caller-owned identifier binders, lexical
  literal matching, syntax subtyping, zero/recursive variadics, imports, hex i64
   boundaries and closure cleanup. Recursive list/vector/tuple/AST patterns,
   wildcards, dotted forwarding, keyword/custom/escaped ellipses, nested/empty
   repetition shapes, repeated binding templates and numeric datum equivalence
   are covered. Twenty-eight negative cases distinguish static
  template-category/ellipsis errors, expansion limits and ordinary value errors.
- `on1x.ppx`: native/shebang execution, ordered chains, label forwarding, all six
  standard processors, schema extensions, byte/Unicode round trips, permission
  gates (also through imports), process/size/time failures, early stdin closure,
  descendant process-group cleanup, bounded labels/registrations, preservation of
  existing output on failure, no shell fallback, complete extension lowering and
  post-rewrite semantic errors.
  Standard assertion generation resists caller shadowing; constant folding keeps
  overflow/division traps. Every example without a source PPX chain round-trips
  through the Python toolkit with identical C11 (or the same missing-main error).
- `on1x.staging`: native code construction/run timing, splice vs lift, nested
  quotations, function-valued/polymorphic code, explicit snapshots, void code,
  managed-field ownership, loop cleanup, contextual run shadowing and macro
  composition/universal parameters; sixteen semantic rejections plus quotation limits. HIR tests
  separately verify idempotent checking and transactional failure.
- `on1x.selfhost`: all six processors compiled from On1x, native chain and own
  identity bootstrap, canonical byte/codec parity, visitor paths/fresh names,
  schema JSON/extensions and failure cases, 160 differential arithmetic folds,
  data allocation/bounds limits and 100,000-node iterative destruction. Normal
  leak checks remain enabled; fatal non-unwinding paths are tested separately.
- `on1x.local_syntax`: nonrecursive/recursive groups, mutual calls, shadowing,
  definition-site value/syntax and literal identity, captured groups, schemes,
  closures/cleanup, staging and deterministic expansion. Negative cases cover
  scope/future-binding escapes, unused invalid templates, duplicate/undefined
  macros, recursive limits, mutable captures and generated local definitions.
- `on1x.contracts`: canonical header/manual hashes and complete profile inventories,
  explicit exactly-once implementation plans, no-replace snapshot publication,
  actual JSON transfer/replacement/borrows, Vec realloc/self-alias/reuse, Parsec
  borrowed descriptors/captures and counted partial-allocation failure cleanup.
- `on1x.contracts_sdk` (opt-in `ON1X_WITH_EXOML`): public v2 CDecl/contract transport,
  strict malformed/unknown metadata rejection, all review/safe-export gates,
  native scalar adapters and narrowing failures, compiler-rejected ABI drift and
  full IO/ownership metadata retention. With `ON1X_WITH_EXOML_EXTRACT`, also tests
  real libclang discovery, macro/target fingerprints, i386 declaration discovery,
  hostile compile-command rejection, all 45 canonical functions, reproducible
  snapshots and artifact/source/manifest drift. SDK-only builds explicitly use
  synthetic metadata fixtures, not fabricated discovery or semantic review.
- `on1x.contract_strings` (requires `ON1X_WITH_EXOML_EXTRACT` and
  `ON1X_WITH_CHISEL`): real extracted immutable-string ABI, directional v4
  evidence, checked On1x/C11 and separate-TU C/C++ execution, seeded byte-length
  oracle, strict pointer signature/layout drift, review/lifetime/effect rejection,
  null/byte-limit failures, multiline observations, checked build-helper use and
  unsupported-MIR diagnostics. Review flags are synthetic test inputs only;
  production standard-library adapters are not certified by these tests.
- `on1x.scanner_parity` (generated builds only): deterministic cases comparing
  exact token bytes, offsets, line/column positions and errors. The test probes
  both scanners with hex-encoded token text, including comments, CRLF, escaped
  strings, UTF-8, invalid bytes, NUL, dense input and the source-size boundary.

`on1x.json` and `on1x.contracts` exit 77 when their canonical parent headers are absent; CTest
reports **SKIP**, not PASS. The counting JSON fixture in `on1x.compiler` is only
a codegen test double, not a substitute for real-kernel integration. The same
fixture in `on1x.functions` checks owner cleanup through indirect calls and
function-valued returns, not captured-resource lifetimes. Closures reject all
JSON captures; `on1x.json` separately checks lambda-local roots against MuLIB.

The generated frontend requires the actual LibStone
`scripts/{asdl,aurocks}.pl`. Configure with `-DON1X_REQUIRE_LIBSTONE=ON` and set
`ON1X_LIBSTONE_ROOT` for a non-default checkout. Independently configure
`-DON1X_USE_FALLBACK_SCANNER=ON` in another build directory to test bootstrapping.

All native test programs use `-std=c11 -Wall -Wextra -Wpedantic -Werror`.
`ON1X_TEST_C_FLAGS` is a semicolon-separated CMake list of additional
compiler/linker flags passed as separate arguments, without a shell. For
ASan/UBSan, instrument both the C/C++ compiler build and generated programs:

```sh
cmake -S on1x -B on1x/build-sanitize \
  -DON1X_REQUIRE_LIBSTONE=ON \
  -DON1X_LIBSTONE_ROOT="$PWD/on1x/third_party/libstone" \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all' \
  '-DON1X_TEST_C_FLAGS=-g;-fsanitize=address,undefined;-fno-omit-frame-pointer;-fno-sanitize-recover=all'
cmake --build on1x/build-sanitize --parallel 4
ctest --test-dir on1x/build-sanitize --output-on-failure
```

The optional ExoML tool has its own SDK test above. No
generalized safe FFI, Chisel/Wetstone proof or cross-target executable ABI coverage is
implied. See [verification](../VERIFICATION.md) for executed configurations.
