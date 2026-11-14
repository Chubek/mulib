# On1x P0 / P1 verification report — 2026-11-14

## Environment and dependency identity

- Linux x86_64, glibc 2.44; compiler target `x86_64-pc-linux-gnu`.
- GCC/G++ 16.2.1 (20260810); Clang/Clang++ 22.1.8.
- CMake 4.4.3, Python 3.14.7, Perl 5.42.2.
- Supplied LibStone checkout: `on1x/third_party/libstone`, commit
  `aafc6994e9322c9a46ac8f99854c00a8a37a3d5a`; `asdl.pl` and `aurocks.pl` 2.0.
  Only read/generator invocations were performed; the checkout was not changed.
- Canonical MuLIB kernels came from this repository's parent `include/`, not
  LibStone's vendored MuLIB snapshot.

## Executed checks

The table covers the current patterns/self-hosting/MSP continuation. Subsequent
milestone sections record earlier increments; their former feature limitations
and fingerprint snapshot are historical, not current feature claims.

| Configuration | Result |
|---|---|
| GCC generated ASDL/Aurocks frontend (fresh continuation build) | 21/21 CTest tests PASS |
| GCC independent fallback frontend | 20/20 PASS |
| Clang generated ASDL/Aurocks frontend | 21/21 PASS |
| Clang independent fallback frontend | 20/20 PASS |
| Clang ASan+UBSan generated frontend and generated C11 programs | 21/21 PASS |
| Clang ASan+UBSan independent fallback and generated C11 programs | 20/20 PASS |
| Standalone GCC fallback without LibStone or parent MuLIB headers | 19 PASS; JSON SKIP |

Compiler builds and emitted C11 programs use
`-Wall -Wextra -Wpedantic -Werror`. Sanitized configurations additionally use
`-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all`,
including the real MuLIB kernel implementation TUs. No test was skipped in the
six parent-MuLIB configurations. A separately staged standalone source copy
verified that absent canonical headers produce an explicit CTest JSON SKIP.

Generator builds visibly generated and compiled `on1x_ast.c`,
`on1x_parser.c` and `ppx_ast.c`. The differential corpus compares **258** exact token/error
cases, including byte offsets, invalid strings, comments, NUL and source limits.
Scanner API tests cover non-terminated bounded buffers, copied arena-owned
tokens, repeated scans after failures and serialized concurrent calls.

### Current continuation: recursive patterns, native authoring and typed MSP

Fresh required-generator build: `/tmp/opencode/on1x-stages-gcc-generated`.
The other final build directories are `on1x-gcc-fallback`,
`on1x-clang-generated`, `on1x-clang-fallback`,
`on1x-clang-sanitize-generated` and `on1x-clang-sanitize-fallback`, all beneath
`/tmp/opencode`. Both actual LibStone generators ran in the fresh build, and all
generated C sources and six native On1x PPX targets compiled. The checkout's
commit/scripts remain unchanged. No skips occurred in the six parent-MuLIB builds.
The final independent source stage is
`/tmp/opencode/on1x-stages-final-standalone-6crq8hbt`, with 19 passes and explicit
JSON SKIP. Macros, PPX, staging and self-hosting suites additionally pass with
`-O2` emitted C under GCC/Clang and `-O2` Clang ASan/UBSan; the sanitized compiler
is instrumented too. Logs are under `/tmp/opencode/on1x-stages-optimized`.

Recursive R7RS-style pattern/template coverage includes list/vector/tuple/call/
operator structure, wildcard and binding-sensitive literal keywords, custom and
escaped ellipses, dotted forwarding, fixed suffixes, multiple/nested repetitions,
empty dimensions, repeated identifier binders and mathematical numeric data.
`on1x.macros` checks **28 semantic/syntax rejections**, a 10,000-slot flat pattern
without per-slot stack recursion, and an adversarial ambiguous match stopped by
the explicit work budget. This is the pattern/template family over On1x syntax,
not Scheme's complete macro binding system or local `let-syntax` support.

`on1x.selfhost` independently compiles all **six processors from On1x** and runs
a native chain. The S-expression parser/printer, visitor, JSON schema reader,
validator/extensions, constructors and fresh-name tools are actual On1x code.
The authoring library also compiles through its own native identity processor
with byte-identical C. Tests cover canonical byte escapes, visitor paths, fresh
names, schema/extension errors and Unicode surrogates, checked numeric bounds,
160 differential folds, a 100,000-node iterative drop chain, allocation failure,
managed ABI rejection and runtime storage/index limits. Normal-exit sanitizer
leak checks stay enabled; fatal non-unwinding test children disable leak checking
only for their intentionally abandoned language owners.

MSP uses the selected **typed closed-code AOT** model, not JIT/native runtime
compilation. `on1x.staging` tests construction/run effect timing, nested quotes,
splice/lift, mutable snapshots, void/function/tuple/managed results, local/global
run shadowing, schemes and explicit universal code parameters, macro composition,
loop cleanup and normalized public types. **16 semantic rejections** plus stage
limits include future-binding extrusion (including re-entry at equal depth),
implicit cross-stage capture and code-bearing occurs checks. `on1x.meta_hir`
adds staged wire/C equivalence, idempotence and transactional lowering/inference
failure. Existing functional/ownership/ABI suites all remain green.

Independent `tools/on1x-build.py` builds/runs pass for **16 examples and all six
native processors with both GCC and Clang**, with identical outputs and no
LibStone/scanner/C++/Python runtime linkage. The C++ compiler itself is not
self-hosted. Native processors load the authoritative schema JSON data file
through `ON1X_PPX_SCHEMA`; their application runtime is C11 only.

Closure ABI version 1 is preserved; data primitives add internal
`ON1X_DATA_ABI_VERSION=1`. Dynamic NUL-free strings intentionally use a bounded
64 MiB process-lifetime snapshot pool, reclaimed at normal exit; managed syntax
and buffer nodes release promptly. No general owning String, foreign lifetime
proof, effect analysis or fatal-error unwinding is claimed. See
[patterns/PPX](docs/metaprogramming.md), [MSP](docs/staging.md) and
[native authoring/ownership](docs/selfhosting.md).

C11 output **and** public type listings for all 16 examples plus six native
processors are byte-identical across the six final compiler configurations.
All five ASDL/Aurocks generated artifacts are identical across the three
generated builds. Reproduction artifacts, 95 source fingerprints and 49 artifact
fingerprints are in `/tmp/opencode/on1x-stages-reproducibility` (`source.sha256`,
`artifacts.sha256`). These are build artifacts, not vendored/generated sources.

The compiler suite includes 25 negative source cases, eight checked arithmetic
traps and native precedence/evaluation-order tests. Separate input-limit tests
reject 20,000-level blocks, parentheses, unary expressions, binary chains and
calls without crashing, now including long postfix-call chains and nested
function types. The original deeply nested block input crashed before the
parsing/tree-depth guards.

### Initial P1: first-class monomorphic functions

`on1x.types` checks structural monotype equality/ordering and owned signature
storage. `on1x.functions` compiles and runs eleven programs, with 30 semantic
rejections separated from three syntax rejections and an argument-location
regression. The same function suite also passes with `-O2` under GCC and Clang.

Coverage includes mutable function aliases, nested higher-order parameters and
returns, zero-argument/void/string/bool signatures, recursion, lexical shadowing,
imports, callee-before-argument evaluation, short circuiting, raw-call wrappers,
typedef/extern name collisions and lexical JSON cleanup through indirect calls.
Function-valued extern ABIs, raw extern values, intrinsic values and nested
resource-bearing signatures are rejected, not silently adapted. These regression
programs now use the closure-capable representation described below, while
top-level function values still need no environment allocation.

### P1 continuation: bounded monomorphic inference

`on1x.inference` compiles/runs twelve programs and separates 33 semantic
rejections from two syntax rejections. It covers omitted parameter annotations,
explicit/nested `_` holes, inferred returns, higher-order constraints, mutual
recursion, forward references, imports, lexical shadowing and JSON cleanup.
The suite also passes with `-O2` under GCC and Clang. `--types` output is checked
for exact concrete signatures, explicit raw-extern labels and deterministic
file/stdout behavior.

`on1x.inference_engine` checks idempotent admission of grounded HIR, absence of
escaped holes, occurs-check rejection, inclusive budget boundaries and
transactional failure under lowered node/work/depth/expanded-size budgets.
Separate source cases reject 300-level inferred type chains and exponential
type growth before recursive semantic/backend traversal. A finite shared type
graph still compiles natively, and long function names do not amplify per-
parameter diagnostic metadata.

All types are grounded before the ordinary semantic/resource/ABI checks run.
Tests reject inferred resource/view escapes, aliases, invalid intrinsic domains,
void bindings/parameters, missing returns and raw-function escapes. Extern
signatures must remain concrete. No type schemes, generalization, guessed
defaults, effect inference or foreign safety proof is claimed.

### P1 continuation: immutable lexical closures

`on1x.closures` compiles/runs twenty native programs, separating 29 semantic
rejections from four syntax rejections. Coverage includes anonymous functions,
escaping scalar/string/function captures, nested/transitive captures, function-
valued returns, aliases/self-assignment, shadowing, snapshots, local mutation,
top-level recursion, imports, raw calls, JSON-local cleanup and generated-name
collisions. Allocator hooks verify zero live environments at normal exit and
precise loop/condition/discard cleanup. Nested short-circuit operands preserve
the lifetime of previously evaluated callees and arguments.

The suite releases a 50,000-environment generated capture chain without
invoking it recursively. An isolated C11 runtime test also checks a 100,000-
environment destruction chain, null operations, reference initialization,
overflow/underflow and invalid allocation sizes. Failure injection on the
first/second capture allocation verifies fatal status 72; these errors do not
promise unwinding. The function, inference and closure suites all pass with
`-O2` under GCC and Clang; closures additionally pass with `-O2` plus Clang
ASan/UBSan on emitted C and the runtime.

`on1x.closure_hir` verifies concrete inferred lambda types, deduplicated first-
use/transitive captures, shadowed bindings, idempotent checking/emission and
transactional inference failure. Input-limit tests exercise lambda-body depth,
the inclusive 2,048-lambda / 16,384-capture boundaries, and the 2 MiB copied-name
budget against transitive long-name amplification. Scanner token grammar needs
no new tokens; both real generators remain in the verified frontend path.

Function values now use exact typed invocation pointers plus optional
reference-counted environments. `ON1X_CLOSURE_ABI_VERSION` guards the internal
ABI, and a deliberately incompatible runtime header fails to compile. Emitted
C and runtime must be rebuilt together; no stable extension or foreign callback
ABI is claimed. Mutable/JSON captures, recursive `let`, polymorphism, concurrent
reference counting and general foreign lifetime/effect analysis remain absent.
See [closure ownership/runtime manual](docs/closures.md).

### P1 continuation: tuples and local destructuring

`on1x.tuples` compiles/runs twenty-three native programs, with 34 semantic
rejections separated from eight syntax rejections. Coverage includes ordered
product types, grouping vs tuples, pattern-driven inference, nested patterns,
shadowing, ordinary `_` bindings, left-to-right/single evaluation, mutable whole
bindings, function-valued fields, returns/copies/self-assignment, tuple captures,
transitive closures, short-circuit/condition/discard cleanup, raw-call wrappers,
imports, generated-name collisions and lexical JSON cleanup.

Counting hooks verify zero live environments at normal exit and release of a
50,000-environment tuple-capture chain. Deep 64-level ownership helpers and
finite shared type graphs compile natively with bounded emitted C; 300-level
inferred tuple chains and exponential type growth fail before backend traversal.
Input-limit tests reject 20,000-level tuple types/values/patterns and verify
inclusive nesting boundaries. `on1x.tuple_hir` checks ground types, tuple capture
metadata, occurs checks, idempotence and transactional type-budget failure.
`on1x.types` now also covers tuple shape/order/arity and cached ownership metadata.

The tuple suite passes with `-O2` under GCC/Clang and with `-O2` plus Clang
ASan/UBSan on emitted C/runtime. Strict Clang testing caught trivial tuple self-
assignment producing a C `-Wself-assign` error; assignments now snapshot trivial
values too, with scalar/tuple regressions. Exact tuple `--types` output and
deterministic emitted C are checked. JSON/void elements, mutable patterns,
mutable captures and aggregate extern ABIs fail closed; no general matching or
let-polymorphism is claimed.

Tuples use deduplicated C11 records by value, retaining/releasing nested function
fields through generated const-pointer helpers. Pattern projections use borrowed
intermediate pointers instead of amplified access paths. The environment runtime
and its ABI version 1 are unchanged; no foreign aggregate layout is inferred.
See [tuple ownership/lowering manual](docs/tuples.md).

### P1 continuation: function/lambda parameter patterns

Ordinary functions and lambdas now accept nested tuple parameter patterns with
optional annotations. Each pattern denotes one argument; extracted bindings are
immutable and share the function body scope. Arguments remain borrowed, while
entry-prologue field copies retain their own closure references. Returning or
capturing extracted fields preserves their environments after the call.
Internal argument identifiers are unlexable by source and never emitted as C
identifiers. Raw extern patterns were rejected in this increment; the explicit
scalar-flattening contract below now admits them without an aggregate ABI.

`on1x.parameter_patterns` compiles five native programs and separates fourteen
semantic rejections from accepted pattern syntax. It checks inference, nesting,
duplicate names across parameters/body locals, shadowing, escaping fields,
branch/loop cleanup, monomorphism, resource/extern rejection, deep-input limits
and deterministic C. `on1x.tuple_hir` additionally verifies grounded parameter
types and repeated admission/emission. The suite passes with GCC/Clang `-O2`.
The independent native helper builds/runs the example with both compilers,
printing three lines of `42`, without LibStone/scanner/C++ runtime linkage.

The fresh required-generator build for this continuation is
`/tmp/opencode/on1x-parameters-gcc-generated`; use the reproduction commands
below with that build-directory name. Both generator scripts visibly generated
and compiled the scanner/AST there. All six integrated configurations pass;
the independent standalone fallback has twelve passes and an explicit JSON
SKIP. No general matching, polymorphism, resource capture or new runtime ABI is
claimed.

### Local let schemes and explicit flattened extern patterns

The initial scheme continuation added value-restricted generalization to named immutable
nonexpansive local lets. Free environment/global/return variables and weak
overloaded operands remain shared, while uses instantiate fresh constraints.
Checked generic prototypes are retained in HIR; only concrete static-use
specializations are emitted at the original declaration point. Inference and
elaboration commit transactionally from a cloned unit. Its original restriction
to named local schemes is extended by the global/pattern/universal work below.

Explicit extern tuple patterns now flatten scalar leaves depth-first and
left-to-right. A logical tuple argument becomes scalar C parameters, not a C
struct. Only concrete i64/bool/string leaves are accepted. Tests verify matching
shape, duplicate-name rejection, unsupported leaves, and once-only ordered
evaluation. Raw ABI/ownership obligations remain the caller's responsibility.

`on1x.poly` compiles/runs nine native programs, with fifteen semantic rejections
and three independent growth/error inputs. Coverage includes aliases, nested
schemes, annotations, generic tuples, weak variables, unused-invalid bodies,
mutable/effectful restriction, resource failures and escaping closure cleanup.
It passes with GCC/Clang `-O2`, also with Clang ASan/UBSan on optimized emitted
C/runtime. `on1x.inference_engine` verifies quantified prototypes, idempotence,
transactional zero-instance/clone budgets and an independently bounded recursive
specialization test. Scheme/environment/instance/AST budgets fail with diagnostics.

The fresh strict required-generator build is
`/tmp/opencode/on1x-schemes-gcc-generated`; both generator scripts visibly ran
and their C output compiled. All six integrated configurations above pass with
no skips. A separately staged standalone build passes thirteen tests and skips
JSON explicitly. Independent GCC/Clang native-helper builds pass for eight
examples (hello, JSON, higher-order, inference, closures, tuples, parameter
patterns and polymorphism), plus the flattened-extern fixture; `ldd` shows no
LibStone/scanner/C++ runtime linkage. The closure C runtime and ABI version 1
remain unchanged. See [the scheme/extern-pattern manual](docs/polymorphism.md).

### Global and destructuring schemes; explicit universal parameters

The preceding continuation generalized ordinary global declarations (including
imports) after shared strongly connected recursive-component inference. Each
static external use specializes the component together. Self/mutual recursion
within an instance remains monomorphic: no polymorphic recursion is inferred.
Lexical name resolution determines dependencies, so local shadows are not global
edges. Nonexpansive immutable destructuring independently generalizes eligible
leaf variables, with concrete siblings and weak captures retaining their shared
constraints and original lexical sampling point.

`forall(a) fn(a) -> a` explicitly requests a parameter usable at multiple types
inside one body. The predicative implementation checks introductions against
distinct rigid variables, forbids specializing monomorphic environments, and
checks even unused universal arguments. A scalar-only implementation cannot
masquerade as a universal function merely because observed calls are scalar.
The syntax admits closed function bodies without holes/nested binders; ordinary
unannotated parameters do not infer higher-rank schemes. Fully annotated expected
function types also propagate universal parameter information into lambdas.

Universal runtime values use deterministic whole-unit C11 bundles of exactly
typed closure instances, with fixed-point demand discovery before type freezing.
Forwarding, return, tuple extraction and immutable capture retain/release fields
through the existing ownership machinery. No erased values, function-pointer
casts, foreign universal ABI or runtime closure ABI change is introduced.
Dependency depth, scheme/environment metadata, specialization count/nesting,
inference work/type size, AST cloning and demand rounds are all bounded.

`on1x.poly_extended` now compiles/runs **25 native programs**, checks **23
semantic/syntax rejections**, and exercises dependency-depth failure,
deterministic emission/type listings, two injected allocation failures, loop
cleanup, escaping universal captures, and function-valued instantiations.
HIR/type tests check rigid proofs versus mere instances, layout demand,
idempotence and transactional demand-budget failure. Former monomorphic-global
and destructuring negatives are replaced by positive scheme tests or strengthened
mutable/weak rejections; overloaded/resource/ABI restrictions are preserved.

The fresh strict required-generator build is
`/tmp/opencode/on1x-universal-gcc-generated`; both supplied generator scripts ran
and their C output compiled. All six final integrated configurations above pass
without skips. Both scheme suites additionally pass GCC/Clang `-O2` and Clang
`-O2` ASan/UBSan (compiler, generated C and runtime). A separately staged standalone
fallback build has fourteen passes and explicit JSON SKIP. Final independent
GCC/Clang native-helper builds pass for eleven examples, adding
`global_polymorphism`, `destructuring_polymorphism`, and `polymorphic_parameters`
to the earlier eight; `ldd` shows no LibStone/scanner/C++ runtime linkage.
`--types` now lists normalized public global schemes and hides internal names.
See [the updated polymorphism manual](docs/polymorphism.md).

### Initial typed hygienic AST macros and external PPX core (historical)

The user selected a core-first increment and explicit opt-in for source-directed
PPX execution. The implementation now expands ordered, typed declarative rules
after parsing, using syntax-context marks and lexical binding resolution rather
than textual substitution. General/granular categories share the schema hierarchy
(`Literal` → `NumericLiteral` → `IntegerLiteral` → `HexLiteral`); template capture,
binding and repetition positions are checked even in unused definitions. One
variadic tail, Block splicing, binding-sensitive identifier literals, imported
macros and deterministic fresh names are implemented. Full R7RS structural
patterns, local syntax bindings and nested/arbitrary ellipses are not claimed.

External programs receive one S-expression AST over stdin and return one over
stdout. Source `@ppx` registrations require `--allow-source-ppx`; explicit CLI
registrations authorize their own programs. Labeled expression/Block carriers,
function attributes and namespaced schema extensions are validated and must be
lowered before source AST reconstruction and normal type/ownership checking.
The compiler embeds the actual `grammar/PPX.schema.json` file. Generated builds
also materialize/structurally validate received wire trees through the real
`scripts/asdl.pl` output for `grammar/PPX.asdl`; arena ownership does not escape.

The shell-free POSIX runner supports host-native executables and readable
shebang scripts, forwards the assigned label in the environment, multiplexes
bounded streams, prevents parent SIGPIPE, and times out/kills the process group.
Tests verify that source registrations (also imports) cannot execute without
permission, text executables never get an implicit shell fallback, an early
stdin close is handled, and a descriptor-retaining descendant is terminated.
Malformed/oversized/deep output, internal HIR constructors, invalid schemas,
remaining extensions and ordinary semantic errors produce no partial C; a
pre-existing output file remains unchanged. Both macro expansion and the entire
PPX AST rewrite are transactional; external process side effects are not.

`on1x.macros` runs **10 native programs** and **20 rejections**, including caller/
definition-site hygiene, exposed Identifier binders, literal binding identity,
syntax subtyping, empty/recursive variadics, hex boundaries and closure cleanup.
`on1x.ppx` runs **10 generated native cases** plus a native C transformer and
protocol/permission/resource/schema regressions. All six real processors are
exercised: identity, checks, control, unroll, derive and fold. Generated assertions
resist local shadowing, and folding preserves overflow/division traps. The Python
sexpr/visitor/ppx bootstrap tools round-trip every applicable source example
with byte-identical C (or the same missing-main error). `on1x.meta_hir` checks
codec/schema/AST equivalence, subtype laws, idempotence and transactional failures.

Fresh required-generator build: `/tmp/opencode/on1x-meta-gcc-generated`.
All six final configurations above pass without skips. The macro and PPX suites
also pass GCC/Clang `-O2` and Clang `-O2` ASan/UBSan (compiler, emitted C and native
transformer fixture). A newly staged standalone fallback has seventeen passes
and explicit JSON SKIP. Independent GCC/Clang native-helper builds pass for
**13 examples**, adding macros and the five-stage source-registered PPX example;
`ldd` shows no LibStone/scanner/C++/Python runtime linkage in those applications.
The application closure ABI remains version 1.

The authoring libraries are Python-hosted bootstrap tools, not self-hosted On1x
or safe MuLIB bindings. IPC, the missing self-hosting foundations, the larger
processor catalog, inline C/assembly and ArchML are deferred rather than counted
as stubs. Source opt-in is execution authorization, **not a sandbox**. Only Linux
host execution is verified; PE/Mach-O execution on Linux and non-POSIX transport
are not claimed. See [the metaprogramming manual](docs/metaprogramming.md).

The **separately registered** `on1x.json` test exercises the real MuLIB example,
nested document lifetimes, loop/return cleanup, i64 conversions, malformed
input, missing members/null values, and parse/stringify allocation failures. The old
CTest registration omitted this test entirely; its stale example expectation
has also been corrected. Fatal `or_die` error paths still terminate rather than
providing general resource unwinding. The suite now also compiles/runs an
inferred string-to-size function and a string-capturing closure with locally
owned JSON and borrowed child locals against the real canonical kernel, under
the same sanitizer flags. It also returns scalar tuple results through a closure
after dropping its JSON locals, without embedding any JSON resource in a tuple.

Independently of CTest, `tools/on1x-build.py` built and ran `examples/hello.on1x`,
`examples/json.on1x` and `examples/higher_order.on1x` using both GCC and Clang.
Results were `21`, `3` followed by `{"items":[1,2,3],"ok":true}`, and the lines
`41, 42, 14, 10, 18, 12, 21`, respectively. `ldd` showed no LibStone, scanner or
C++ runtime linkage. This verifies the source-tree native build workflow, not
a system-wide installation/package.

The same independent native-helper check also built and ran
`examples/inference.on1x` with both compilers: output `42`, `true`, `42`, with
no LibStone/scanner/C++ runtime linkage. This increment adds no application
dependency beyond the small C11 runtime.

`examples/closures.on1x` was likewise built/run with both native compilers:
output `42`, `42`, `hello from a closure`. All five native-helper examples were
rebuilt after the closure ABI change; no LibStone/scanner/C++ runtime linkage
was present. Environment allocation uses only the C runtime, not a vendored
allocator or foreign closure library.

`examples/tuples.on1x` likewise prints `42`, `true`, `42`, `38` under both native
compilers. All six examples above were rebuilt through the independent helper
for this increment; `ldd` still showed no LibStone/scanner/C++ runtime linkage.

## Reproduction

From the MuLIB root, using the supplied non-default LibStone location:

```sh
cmake -S on1x -B /tmp/opencode/on1x-stages-gcc-generated \
  -DON1X_REQUIRE_LIBSTONE=ON -DBUILD_TESTING=ON \
  -DON1X_LIBSTONE_ROOT="$PWD/on1x/third_party/libstone" \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'
cmake --build /tmp/opencode/on1x-stages-gcc-generated --parallel 4
ctest --test-dir /tmp/opencode/on1x-stages-gcc-generated --output-on-failure
python3 on1x/tools/on1x-build.py on1x/examples/hello.on1x \
  -o /tmp/opencode/on1x-hello --on1xc /tmp/opencode/on1x-stages-gcc-generated/on1xc
/tmp/opencode/on1x-hello
python3 on1x/tools/on1x-build.py on1x/examples/json.on1x \
  -o /tmp/opencode/on1x-json --on1xc /tmp/opencode/on1x-stages-gcc-generated/on1xc
/tmp/opencode/on1x-json
python3 on1x/tools/on1x-build.py on1x/examples/higher_order.on1x \
  -o /tmp/opencode/on1x-higher-order --on1xc /tmp/opencode/on1x-stages-gcc-generated/on1xc
/tmp/opencode/on1x-higher-order
python3 on1x/tools/on1x-build.py on1x/examples/inference.on1x \
  -o /tmp/opencode/on1x-inference --on1xc /tmp/opencode/on1x-stages-gcc-generated/on1xc
/tmp/opencode/on1x-inference
python3 on1x/tools/on1x-build.py on1x/examples/closures.on1x \
  -o /tmp/opencode/on1x-closures --on1xc /tmp/opencode/on1x-stages-gcc-generated/on1xc
/tmp/opencode/on1x-closures
python3 on1x/tools/on1x-build.py on1x/examples/tuples.on1x \
  -o /tmp/opencode/on1x-tuples --on1xc /tmp/opencode/on1x-stages-gcc-generated/on1xc
/tmp/opencode/on1x-tuples
/tmp/opencode/on1x-stages-gcc-generated/on1xc --types on1x/examples/tuples.on1x
```

Use a separate directory with `-DON1X_USE_FALLBACK_SCANNER=ON`, omitting
`ON1X_REQUIRE_LIBSTONE`, for independent fallback testing. The sanitizer
configuration and generated-program flags are documented in
[tests/README.md](tests/README.md).

The current macro/PPX examples can be reproduced with the fresh compiler:

```sh
python3 on1x/tools/on1x-build.py on1x/examples/macros.on1x \
  --on1xc /tmp/opencode/on1x-stages-gcc-generated/on1xc -o /tmp/opencode/on1x-macros
/tmp/opencode/on1x-macros
python3 on1x/tools/on1x-build.py on1x/examples/ppx.on1x --allow-source-ppx \
  --on1xc /tmp/opencode/on1x-stages-gcc-generated/on1xc -o /tmp/opencode/on1x-ppx
/tmp/opencode/on1x-ppx
python3 on1x/tools/on1x-build.py on1x/examples/staging.on1x \
  --on1xc /tmp/opencode/on1x-stages-gcc-generated/on1xc -o /tmp/opencode/on1x-staging
/tmp/opencode/on1x-staging
ON1X_PPX_SCHEMA="$PWD/on1x/grammar/PPX.schema.json" \
  /tmp/opencode/on1x-stages-gcc-generated/on1xc --check on1x/examples/staging.on1x \
  --ppx native /tmp/opencode/on1x-stages-gcc-generated/stdppx/on1x-ppx-identity
```

## Current continuation: selected SHA-256 inputs

Paths are relative to `on1x/`. These identify current implementation inputs,
not a complete foreign-contract inventory:

```text
b46dfdde0530aa1d2883a3375202aff43d31e090d254b5dfc42bae4ba1c68feb  grammar/On1x.g
bc76775b14770bce313860d9952a3ab3a86a051095765da9666b4b6bee4db2d4  grammar/PPX.schema.json
44894b0ab568c4272ec80faa265b257af8236d220dcff5461261962f10801313  include/on1x/type.hpp
bdad01f26b4157e773ed99830fa1f32d733a799e30e931f42a187a6601ed92ae  src/cli.cpp
78ee08e5d6866dd123240b3fc5e246c9ec975c5c436e8ca19529adaa08e4a20e  src/macro_rules.cpp
9c1ab22fc819a506b90d345c7cc50edd67d59cdacda5309117d8be9f3d407bad  src/staging.cpp
76d37b84620fb8ac945be6164c8d7264f71b2b2b23297bf551d2567ea2fee9a4  runtime/on1x_runtime.c
ad0c8532c3558a63915c63da81c225e5f686c7632ca2e9a02a5bebf2a7122985  runtime/on1x_runtime.h
b7b72cee7ec9a58c4b6b121d15e69e1dc14e40821210af8100ba6849b8ab4ec0  stdlib/sexpr/sexpr.on1x
2badbd3d6ef4ce83bfb1102a70d9005a867a377f962a1812d3e59514d8e338bf  stdlib/visitor/visitor.on1x
38676c5836c9bbdee6a45cdf255849ea5ee73ec7e74e4ce48fd09f1d1f01a718  stdlib/ppx/json.on1x
597b7de37ecdfce63ac3c1e244f4eb33f1d6dc0938b5df06d8ad4229acbfaf9f  stdlib/ppx/schema.on1x
ff081d6a5a3889fa8d770afaafd26090e58b58ae7799367c97d83e7b1155b074  stdlib/ppx/ppx.on1x
9ebaf2880331764ddd06200fc4e92c0f2a7472270a9f0337fa58e99aa9f814c7  stdlib/ppx/numbers.on1x
aa6fd96e4442c224d96d08a8a95340e86f6b2fde403de2a2c4f62a05bffc33c2  stdlib/ppx/signature.on1x
```

Selected current output hashes (artifact-directory-relative paths):

```text
16874c463dd377dfcdcd36f4c56dc952198efe37723d6081855c349f6bb31065  example-staging.c
ca94b66ef71e477945598c412f1929eacb03ab41b0e9955c2496c16c8d0baf6d  processor-identity.c
cf25372a34c32bc19e3ff33c4d15d4f2909917dacba5656cddb30403b7e60fd9  on1x_parser.c
```

## Historical core-first SHA-256 snapshot

The following snapshot belongs to the earlier macro/PPX-core milestone, before
recursive patterns/native authoring/MSP changed these inputs and emitted programs.
It is retained as historical evidence, **not** as current-file checksums.

These identify the tested generator/grammar/header inputs, not a complete
foreign-contract or transitive-header inventory:

```text
1d04c59d3c8f90565eefa0d661a9d4650ac56e8968eedba3e12fd1fadab4d0c1  scripts/asdl.pl
882d5c1ed69e46ef84488c86485bbec47f1237878d47c4bb58d1d039065f6396  scripts/aurocks.pl
453a2855098c50545220de54ef053a65f485d6c70e4fc6d5aed4c83961b47098  grammar/On1x.asdl
e04c9e28f6acc2cd63e29aaf2b87bb2ac78b2ff0f5873fe7bf24dd5e273ea577  grammar/On1x.g
2924f9ca9ddfaf1ae1f6637625ba4d7a3df934e812913d91fbb5f69c28313cd0  grammar/PPX.asdl
9c47754ad582152e035cd5ca80beb0e7ca1133217edd82d873dae721c6dc32f6  grammar/PPX.schema.json
e9e233759cfa04eed6f5d05e98ca619118c42b6dc71e1f94a1d0aa882a5585e0  ../include/mulib-json.h
```

Generated artifacts were byte-identical between independent GCC and Clang
sanitizer build directories:

```text
311c93a4e3972e01bc551b43b2d99adc22aab27091ded9a8500256e80cebd455  on1x_ast.h
7cde3b01f5cf68d82d4c6452587828394c3af9e739985a90a54b257bbcecd958  on1x_ast.c
253058351879ae296070706e2510ffc4089deda85bc0a402b5b28190d5e670af  on1x_parser.c
205126bec9d02ee0ebff4ad1a6b64c2935beffe70774692f88169f5fce19e7a1  ppx_ast.h
6fda83f71dc338d754f596d0add27c1e1b71c1ce8034de30475d62aa7233aff0  ppx_ast.c
```

Selected P1 inputs and emitted programs:

```text
c9ae05763b9f4700b3fcd82ff6ce2f03749e594ec81435dfa87bb28259223dbb  include/on1x/type.hpp
f80b0a9d294549b2c71d603b7b134cd8caa044c1b21acfad0d838f731c3c5522  include/on1x/ast.hpp
6e9d4f026192fced839ed14c0b97ced3a237cf9574bcb9c6132d79f3b1fe29f6  src/type.cpp
99c008615d1a2f21eef189fa9f8a81dd0608874792e33f9d80db5e27d88f3e33  include/on1x/infer.hpp
53c353a5ed3246c81856694fca8906d9c375e735b07fb579bedb64dba378707a  include/on1x/clone.hpp
8409a239c418d1dd563a2c88177df4a47e227438106f25cdca33da24e725a0ab  include/on1x/meta.hpp
a6e3978d7f29f1b8718db3b5d591b6e97266740f58dc7d3d16f25bb9de175074  src/infer.cpp
b1daec2bf0a26a5e7257224e901bb89adc6029566358464207b54070335d0903  src/parser.cpp
2d2b8a54d19e3af1067712b148cfe0c47132dd845a89c6065c4eaca4145e86e4  src/sema.cpp
8cce45e7f6560e91b361a3cec2683dce4c6325789994edd5bea336c14a731ec2  src/codegen.cpp
6eac009ae6cc886eb0ce5760d23d84c45d4d7842e7787c3e2d0429dd2fad963a  src/cli.cpp
a7d37868bfcb370b9f800f62fefc3e648d9cc0c7b1093770398320567bfa1ce1  src/lexer_fallback.cpp
7b1955767b5fd252cd5ba3198593631e293c8ef87e3e328310339c95f923f0cc  src/macros.cpp
f892274e070d0a04e28e356c61836f804994e698b4c0a7d22b133e1b7d7e75f5  src/sexpr.cpp
6d2b106fb749786f358a4a6771e55226101aceb5e4536c7c95fc006e89482a04  src/ppx.cpp
b7474af157c05060fdfa4dcabbfa3c9babcb30f3979fce5f4004767b478895d2  src/ppx_ast.cpp
3499e02909fb34e71543bceaa6174d7a248b4a2796fc05b7b559099c9e4bbe18  src/ppx_schema.cpp
0db1d344c18a523b123419d604aaebc98dd8be50459094deb745a9ea945e341f  src/ppx_asdl.c
c1df59c6f47d215464dea616bdfb8500d7fb42ec5a5ffad2e081b23f86a8860c  stdlib/sexpr/__init__.py
0581c0c2b569aa72261a0cb227f8291876a82344e20e0f89d4b68bb9f474b72e  stdlib/visitor/__init__.py
6650b12fb041a6289cb95a76d8d9368f4781bb81edfd1a75f2642e7d5dbc83c2  stdlib/ppx/__init__.py
aad9c57ef61c1ee8ad90f8a0e273b0d002dd7a227ad0878ffc39cfbe91f47e6c  stdppx/passes.py
c9fdfc3637ffd16fc82ccf144d25a53f742b3f4c3925a12ec275d9fa0725c757  runtime/on1x_runtime.h
7e32987339cbd50d57797e387c7af8ba1bb15fa4bcbfa38ac395d84056393ceb  runtime/on1x_runtime.c
a257238b5ff05f364205540a7754e0c8c810bdd8caab83c99eb678312ccb8478  examples/higher_order.on1x
6dc205b1df541a06025fcb35ad5fffa44d952b3812c60a8b8dcc990bc4c2dbdb  examples/inference.on1x
53233cff843337b87de0c2f4450b55c65288f95155d4f8fd3e2289fe189e8f87  examples/closures.on1x
c0905287bc38ef1ab4ef1c0d48d8b7d23511f3280385c4b342fd7a08a2d7cfe0  examples/tuples.on1x
7c0f573ee7c952a538aa367e85457d8d4fbedc311e233b58a6c8007ba90a4e00  examples/parameter_patterns.on1x
64294f2695b6073013e7f7c05c88698aaa31088b2736c06677ccc31d121d008d  examples/polymorphism.on1x
11e03b28ec1520a102043bdf2ea102f762385660070a546eff189220f136861a  examples/global_polymorphism.on1x
1ed2f3149366acb7bcdd6e8f9746cf206527354483b1ee8121be8aa859387126  examples/destructuring_polymorphism.on1x
2d7b61a01ead9b30ba5acbffa35d31693795c7c1054b811754178d86cfe685ea  examples/polymorphic_parameters.on1x
77f9c7ce0cb70457adfbff58eb25da03d5a210168518d1598ecad32c6f0240bf  examples/macros.on1x
3fa5301b93b0d23c9326cc2d23d9f7d3e0cda795d515d39aec9c9026e68f9e18  examples/ppx.on1x
a4ec4688c18118f2481c3d22c3d3c369b14fc76f1246a65391f396fc5139d9f3  higher_order.c
e2610ca75efcf93726b5358273d739e64c598570aa7eefa1eb5014d3fb216c33  inference.c
d557e5e2182db4384d5a655d901fbf498bd340882078c8360bd3fd6be13dfd9f  closures.c
7fc4ac628457e0d4eb3f1ddfe98885351d2833b6cc3298b9d36e14f6672fe693  tuples.c
5f76d7cb671c4498df95e5dc3e081ba4ad4d48c560bcd753bdf8298ee61ac9bf  parameter_patterns.c
656603029afbd16579c1be10dedbb6bc12ff024f0054b617a485f1121651bad2  polymorphism.c
785e8fc6c4e4ab00f2729657055cf964e187f949096abee939c3d35f1f0566bf  extern_patterns.c
b178e097a1d5aa542cb68517a7c4f81d43cfe4ae7718b4dbbd7f89643d3e1e4d  types.txt
1f0d77502393681529b7991d3d0c2b42fa0a70f2d0a94959b764c3d6b207c0c0  global_polymorphism.c
3325b0c5e5f4ab157d75c0b4516caf92cf797762e09357124185a6cb1381ba7e  destructuring_polymorphism.c
316aff68612fc8da5721b93422bea3cb5d17cd7b9b62b269b9099075901d6046  polymorphic_parameters.c
f6a8b60a05c35ec770f2dc9bfc2cfed6db5f23181c98c1aedb5439e9d2d57691  macros.c
f3858fb9e52aa7979910f67b35ac0968b7319ef182bfb2334a4a1633a3304c14  stdppx.c
```

All twelve emitted programs and the `--types` test artifact were byte-identical
across all six integrated configurations, including generated/fallback scanners
and GCC/Clang compiler builds. They are test artifacts, not checked-in generated
sources.

## Not verified / not claimed

The optional ExoML-linked `on1x-bind` target and full contract/ABI gates were
not exercised by this P0/P1 iteration. Cross-target behavior, system
packaging, unrestricted higher-rank inference, nested forall binders, full Hindley–Milner, mutable/resource
captures, recursive let, concurrent closures, ADTs,
safe FFI, Chisel/Wetstone analysis, general error cleanup and native/VM compiler
backends remain future work. Complete R7RS language/macro binding support, a
self-hosted compiler, JIT/runtime native compilation, IPC, the larger processor
catalog, inline C/assembly, ArchML integration and non-POSIX transformer transport
are also not claimed. Native PPX authoring and recursive pattern/templates are
implemented as described in the current-continuation section above.
