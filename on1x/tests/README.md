# Tests

CTest registers independent tests with separate artifact directories:

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
- `on1x.scanner_parity` (generated builds only): deterministic cases comparing
  exact token bytes, offsets, line/column positions and errors. The test probes
  both scanners with hex-encoded token text, including comments, CRLF, escaped
  strings, UTF-8, invalid bytes, NUL, dense input and the source-size boundary.

`on1x.json` exits 77 when canonical `../include/mulib-json.h` is absent; CTest
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

The optional ExoML `on1x-bind` tool is not covered by this P0/P1 suite. No
generalized safe FFI, Chisel/Wetstone proof or cross-target ABI coverage is
implied. See [verification](../VERIFICATION.md) for executed configurations.
