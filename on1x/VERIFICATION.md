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

| Configuration | Result |
|---|---|
| GCC generated ASDL/Aurocks frontend (fresh universal build) | 16/16 CTest tests PASS |
| GCC independent fallback frontend | 15/15 PASS |
| Clang generated ASDL/Aurocks frontend | 16/16 PASS |
| Clang independent fallback frontend | 15/15 PASS |
| Clang ASan+UBSan generated frontend and generated C11 programs | 16/16 PASS |
| Clang ASan+UBSan independent fallback and generated C11 programs | 15/15 PASS |
| Standalone GCC fallback without LibStone or parent MuLIB headers | 14 PASS; JSON SKIP |

Compiler builds and emitted C11 programs use
`-Wall -Wextra -Wpedantic -Werror`. Sanitized configurations additionally use
`-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all`,
including the real MuLIB kernel implementation TUs. No test was skipped in the
six parent-MuLIB configurations. A separately staged standalone source copy
verified that absent canonical headers produce an explicit CTest JSON SKIP.

Generator builds visibly generated and compiled `on1x_ast.c` and
`on1x_parser.c`. The differential corpus compares **250** exact token/error
cases, including byte offsets, invalid strings, comments, NUL and source limits.
Scanner API tests cover non-terminated bounded buffers, copied arena-owned
tokens, repeated scans after failures and serialized concurrent calls.

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

The current continuation generalizes ordinary global declarations (including
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
cmake -S on1x -B /tmp/opencode/on1x-tuples-gcc-generated \
  -DON1X_REQUIRE_LIBSTONE=ON -DBUILD_TESTING=ON \
  -DON1X_LIBSTONE_ROOT="$PWD/on1x/third_party/libstone" \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'
cmake --build /tmp/opencode/on1x-tuples-gcc-generated --parallel 4
ctest --test-dir /tmp/opencode/on1x-tuples-gcc-generated --output-on-failure
python3 on1x/tools/on1x-build.py on1x/examples/hello.on1x \
  -o /tmp/opencode/on1x-hello --on1xc /tmp/opencode/on1x-tuples-gcc-generated/on1xc
/tmp/opencode/on1x-hello
python3 on1x/tools/on1x-build.py on1x/examples/json.on1x \
  -o /tmp/opencode/on1x-json --on1xc /tmp/opencode/on1x-tuples-gcc-generated/on1xc
/tmp/opencode/on1x-json
python3 on1x/tools/on1x-build.py on1x/examples/higher_order.on1x \
  -o /tmp/opencode/on1x-higher-order --on1xc /tmp/opencode/on1x-tuples-gcc-generated/on1xc
/tmp/opencode/on1x-higher-order
python3 on1x/tools/on1x-build.py on1x/examples/inference.on1x \
  -o /tmp/opencode/on1x-inference --on1xc /tmp/opencode/on1x-tuples-gcc-generated/on1xc
/tmp/opencode/on1x-inference
python3 on1x/tools/on1x-build.py on1x/examples/closures.on1x \
  -o /tmp/opencode/on1x-closures --on1xc /tmp/opencode/on1x-tuples-gcc-generated/on1xc
/tmp/opencode/on1x-closures
python3 on1x/tools/on1x-build.py on1x/examples/tuples.on1x \
  -o /tmp/opencode/on1x-tuples --on1xc /tmp/opencode/on1x-tuples-gcc-generated/on1xc
/tmp/opencode/on1x-tuples
/tmp/opencode/on1x-tuples-gcc-generated/on1xc --types on1x/examples/tuples.on1x
```

Use a separate directory with `-DON1X_USE_FALLBACK_SCANNER=ON`, omitting
`ON1X_REQUIRE_LIBSTONE`, for independent fallback testing. The sanitizer
configuration and generated-program flags are documented in
[tests/README.md](tests/README.md).

## Selected SHA-256 fingerprints

These identify the tested generator/grammar/header inputs, not a complete
foreign-contract or transitive-header inventory:

```text
1d04c59d3c8f90565eefa0d661a9d4650ac56e8968eedba3e12fd1fadab4d0c1  scripts/asdl.pl
882d5c1ed69e46ef84488c86485bbec47f1237878d47c4bb58d1d039065f6396  scripts/aurocks.pl
453a2855098c50545220de54ef053a65f485d6c70e4fc6d5aed4c83961b47098  grammar/On1x.asdl
57e7364ad677342e1ea6d4b4ba3620520f11f1eb5a5010214536810cd2ae9c58  grammar/On1x.g
e9e233759cfa04eed6f5d05e98ca619118c42b6dc71e1f94a1d0aa882a5585e0  ../include/mulib-json.h
```

Generated artifacts were byte-identical between independent GCC and Clang
sanitizer build directories:

```text
311c93a4e3972e01bc551b43b2d99adc22aab27091ded9a8500256e80cebd455  on1x_ast.h
7cde3b01f5cf68d82d4c6452587828394c3af9e739985a90a54b257bbcecd958  on1x_ast.c
d0015efecb388d31546ec471f2aa3725555e07052772088e96b8cda754b4da6e  on1x_parser.c
```

Selected P1 inputs and emitted programs:

```text
c9ae05763b9f4700b3fcd82ff6ce2f03749e594ec81435dfa87bb28259223dbb  include/on1x/type.hpp
6b3c22dad8418863e856c038681be06a94f56776298b5b203c5e9574bf9ea11b  include/on1x/ast.hpp
6e9d4f026192fced839ed14c0b97ced3a237cf9574bcb9c6132d79f3b1fe29f6  src/type.cpp
99c008615d1a2f21eef189fa9f8a81dd0608874792e33f9d80db5e27d88f3e33  include/on1x/infer.hpp
6dc6fa1b24c6ff7768bb9134ceaeead1477318a929f8a380028bf91ea2e08942  include/on1x/clone.hpp
812a8ff67e3dc02102ae07dbcef9ce1cd3991002dd3e197f93a9ecfe6d2dc050  src/infer.cpp
d64c134d5639d4a89d972b1df573572878a0547f7c7124f34f804ea29edbeb54  src/parser.cpp
a10c858952a2979f9c62c2d95828c3a022c9c7d0a90c67d19267067f0055dd03  src/sema.cpp
68d81cfa8d569add0e385c7ab96fc7a0337ffaf20745f672cf6aa2fb0b89c3f3  src/codegen.cpp
71207b89784ce85c14d16d4f81e7374ddadfeb777c86281eb947b9a7ed32651a  src/cli.cpp
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
```

All ten emitted programs and the `--types` test artifact were byte-identical
across all six integrated configurations, including generated/fallback scanners
and GCC/Clang compiler builds. They are test artifacts, not checked-in generated
sources.

## Not verified / not claimed

The optional ExoML-linked `on1x-bind` target and full contract/ABI gates were
not exercised by this P0/P1 iteration. Cross-target behavior, system
packaging, unrestricted higher-rank inference, nested forall binders, full Hindley–Milner, mutable/resource
captures, recursive let, concurrent closures, ADTs,
safe FFI, Chisel/Wetstone analysis, general error cleanup and native/VM compiler
backends remain future work.
