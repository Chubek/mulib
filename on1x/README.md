# On1x — an ML-family frontend for MuLIB

On1x compiles a deliberately small **statically checked** language to ISO C11,
linking directly to MuLIB kernels. The C++20 compiler lives in this directory;
LibStone is a user-supplied *source dependency*, discovered by default at
`<mulib>/third_party/libstone`. Other checkout locations use `ON1X_LIBSTONE_ROOT`.

**This is an executable milestone, not a complete ML compiler or safe binding
framework.** First-class monomorphic functions, immutable lexical closures, tuples
and local destructuring,
higher-order calls, unification of parameter/result type holes, local inference,
recursion, structural function types, control flow, imports, error
diagnostics, ordered C11 emission, checked i64 arithmetic, externally linked
native functions, and an experimental JSON resource facade are implemented and
tested. Mutable/resource captures, generalized Hindley–Milner inference, type classes,
ADTs, refutable/exhaustive pattern matching, polymorphic modules/functors, exceptions, non-lexical
lifetimes, effect inference, callbacks, generalized FFI ownership, native/VM
codegen, and complete MuLIB binding generation remain work in progress.

## Layout and dependencies

```text
mulib/
  include/mulib-*.h
  on1x/                    <- put this ZIP's on1x directory here
  third_party/libstone/     <- clone separately; user supplies
    scripts/asdl.pl
    scripts/aurocks.pl
    moonstone/exoml/...
```

The **default** CMake build chooses the real LibStone generators whenever both
scripts exist. These generate `on1x_ast.h`, `on1x_ast.c`, and `on1x_parser.c` in
CMake's binary directory. The generated parser is built as a C11/C99 library
and supplies tokens to the C++ parser. The ASDL token AST is arena-owned and
never leaks into the typed C++ HIR. ASDL/Aurocks output is a real part of the
compiler, not an unused example. A handwritten lexer fallback is retained for
bootstrapping and builds without the LibStone checkout; it is separately tested.

The P0 generated/fallback baseline has now been verified with GCC, Clang and
ASan/UBSan against the canonical parent MuLIB headers. Exact token bytes,
byte offsets and error locations are compared on a deterministic corpus. See
[verification results and tool versions](VERIFICATION.md); this does not extend
the foreign safety guarantees. P1 adds monomorphic functions, immutable lexical
closures, product types and bounded inference/specialization with value-restricted
named local lets, not general
captured-resource lifetimes.

## Metaprogramming and multi-stage programming

Typed, hygienic AST macros use `macro`, ordered `rule` clauses and `name!(...)`.
Syntax categories form a hierarchy such as `Expression → Literal → NumericLiteral
→ IntegerLiteral → HexLiteral`; this is separate from ordinary value typing.
Recursive list/vector/tuple/AST patterns, wildcards, literal keywords, dotted
tails and arbitrary/nested ellipses preserve hygiene and repetition shape.

External AST transformers use `@ppx label "path"` (requires
`--allow-source-ppx`) or explicit `--ppx LABEL PROGRAM` options. The compiler
validates their S-expression output against `grammar/PPX.schema.json`, then
performs normal type/ownership checking. Labeled extension nodes and namespaced
schema extensions must lower completely before code generation.

The S-expression/visitor/PPX authoring libraries and all six standard processors
now have **native On1x implementations**, alongside the Python bootstrap versions.
CMake builds native tools in `BUILD/stdppx`; set `ON1X_PPX_SCHEMA` to the absolute
`grammar/PPX.schema.json` path when running them. The compiler remains C++20.
See [native APIs, ownership and limits](docs/selfhosting.md).

MSP adds `code<T>`, `quote { ... }`, `splice(...)`, `lift(...)` and contextual
`run(...)`. Code is typed, closed and ahead-of-time compiled C11: generation-time
operands execute at construction; quoted effects wait for run. No JIT or unchecked
eval is involved. See [MSP semantics](docs/staging.md).

Examples: `examples/{macros,structural_macros,ppx,sexpr,staging}.on1x`.
See the [metaprogramming manual](docs/metaprogramming.md). Local syntax bindings,
complete R7RS language support, IPC, inline C/assembly, ArchML and thirty processors
are not claimed.

Build **with** LibStone:

```sh
cmake -S on1x -B on1x/build \
  -DON1X_REQUIRE_LIBSTONE=ON -DBUILD_TESTING=ON
cmake --build on1x/build -j4
ctest --test-dir on1x/build --output-on-failure
```

For the supplied checkout at `on1x/third_party/libstone`, add
`-DON1X_LIBSTONE_ROOT="$PWD/on1x/third_party/libstone"` to the configure command.
The checkout is used read-only; generated sources go into the build directory.

Build **without** LibStone (fallback only):

```sh
cmake -S on1x -B on1x/build-fallback \
  -DON1X_USE_FALLBACK_SCANNER=ON -DBUILD_TESTING=ON
cmake --build on1x/build-fallback -j4
ctest --test-dir on1x/build-fallback --output-on-failure
```

Requirements: C++20, C11, CMake 3.20+, and Perl 5.14+ for the generated
frontend. Python 3 is used by the test driver and optional build helper. The
checked JSON profile requires a real `mulib/include/mulib-json.h` and its
implementation dependencies. No system-wide installation is necessary.

## Run the compiler

```sh
on1x/build/on1xc --check on1x/examples/hello.on1x
on1x/build/on1xc --tokens on1x/examples/hello.on1x
on1x/build/on1xc --types on1x/examples/inference.on1x
on1x/build/on1xc --emit-c on1x/examples/hello.on1x -o /tmp/hello.c
cc -std=c11 -Ion1x/runtime /tmp/hello.c on1x/runtime/on1x_runtime.c -o /tmp/hello
/tmp/hello
# 21
```

Or use the non-shell Python build helper:

```sh
python3 on1x/tools/on1x-build.py on1x/examples/hello.on1x \
  -o /tmp/on1x-hello --on1xc on1x/build/on1xc
/tmp/on1x-hello
```

For the JSON example, from the MuLIB root:

```sh
python3 on1x/tools/on1x-build.py on1x/examples/json.on1x \
  -o /tmp/on1x-json --on1xc on1x/build/on1xc
/tmp/on1x-json
```

This links `on1x_json.c`, `on1x_mulib_impl.c`, and the actual MuLIB JSON,
Parsec, strbuf and UTF8 modules. The implementation switches are defined in
exactly one translation unit. The JSON example prints an array size and the
serialized document; root cleanup runs on normal and explicit-return paths.

The compiler and ExoML are *not* linked into generated applications. A
standalone generated executable contains C11 glue, a small runtime and the
selected MuLIB kernels. Native backends, LLVM, or the LibStone interpreter are
not required to run this emitted C11 code.

## Syntax — implemented subset

```on1x
import "library.on1x";

extern fn c_abi_function(x: i64) -> i64;

fn square(x: i64) -> i64 { return x * x; }

fn main() -> i64 {
  let answer: i64 = square(6) + 6;
  var sum = 0;
  while sum < answer {
    sum = sum + 1;
  }
  if sum == 42 { println("success"); }
  else { println("failure"); }
  return 0;
}
```

- Top level: `import "relative-file.on1x";`, `fn`, `extern fn`.
- Types: `i64`, `bool`, `string`, `void`, `json`, `json_view`, structural
  function types such as `fn(i64) -> i64`, tuples such as `(i64, bool)`,
  `code<T>`, `sexpr` and `buffer`.
- Local inference for `let`/`var`, optional ordinary-function parameter types,
  and `_` type holes. Omitted return annotations still mean `void`; `-> _`
  opts into return inference. Extern signatures must be concrete and explicit.
- `let` is immutable; `var` supports simple assignment.
- Local `let (a, (b, c)) = value;` and function/lambda parameter patterns support
  immutable tuple destructuring.
- Statements: `let`, `var`, assignment, return, if/else, while, nested blocks,
  and expression statements.
- Expressions: literals, function/local names, anonymous `fn(params) { ... }`
  expressions, postfix calls, `!`, unary minus, arithmetic, numeric
  ordering, equality, short-circuit logical operators, tuples and parentheses.
- `println` and `assert` are checked intrinsics.
- Lexical scopes and source locations. Import cycles and duplicated functions
  are errors. No function overloading or generics yet.
- Source files are limited to 2 MiB each. Active block/expression/type/pattern parsing
  nesting and expression-tree depth are limited to 256 levels; excessive
  nesting is a located compile-time error, not a compiler stack overflow.
- Integers are signed i64; arithmetic operations detect overflow and invalid
  division/modulo and exit with code 70 rather than causing signed overflow UB.
  Integer literals exceeding `INT64_MAX` are rejected at compilation.
- Strings retain the NUL-terminated C-string ABI; embedded NUL is rejected.
  Byte operations, bounded immutable snapshots and buffers support native tools;
  dynamic text snapshots have a 64 MiB process-lifetime budget, not a general
  owning String or Unicode abstraction. See [the data manual](docs/selfhosting.md).
- External function declarations are a *raw*, explicitly ABI-exact escape
  hatch: the user must supply a matching C symbol and signature. They are not
  subject to generalized effects/ownership safety checks. Resource arguments
  and returns are disallowed for ordinary `fn`/`extern fn`.

The On1x compiler's source grammar is currently a C++ recursive-descent/
precedence-climbing grammar over a LibStone-generated token stream. A future
On1x semantic AST may be moved to ASDL once the language surface has stabilized.

## First-class functions — initial P1 subset

```on1x
fn increment(x: i64) -> i64 { return x + 1; }
fn twice(f: fn(i64) -> i64, x: i64) -> i64 { return f(f(x)); }
fn choose() -> fn(i64) -> i64 { return increment; }
fn main() {
  let step = increment;
  println(twice(step, 40));
  println(choose()(41));
}
```

Top-level On1x functions can be stored, passed, returned and called through
arbitrary expressions. `var` function bindings may be reassigned to another
function of exactly the same structural type. Names resolve lexically, so
locals/parameters shadow global functions and intrinsics. Top-level intrinsic
names cannot be redefined. Call targets evaluate first, then arguments from
left to right; logical operators still short circuit.

Function values use typed C11 invocation pointers with optional reference-counted
environments; code always has static lifetime. Function signatures are concrete and
concrete after specialization; nonexpansive named immutable lets can generalize.
Equality, ordering
and printing are not defined for function values. JSON resources/views are
forbidden anywhere inside function signatures. Raw `extern fn` names and
intrinsics are direct-call-only: wrap them in an ordinary On1x function when a
function value is needed. Such a wrapper does **not** make raw foreign behavior
safe. Function-valued extern parameters/returns (foreign callbacks/factories)
remain unsupported.

See `examples/higher_order.on1x` and [language semantics](docs/language.md).

## Monomorphic inference — next P1 increment

```on1x
fn increment(x) -> _ { return x + 1; }
fn apply(operation, value) -> _ { return operation(value); }
fn main() { println(apply(increment, 41)); }
```

The compiler allocates one monotype per declaration, gathers constraints from
all bodies/call sites (including imports and mutual recursion), performs an
occurs check, and grounds every type before ordinary semantic checking and C11
emission. `_` can also appear inside function types or local annotations, such
as `let action: fn(_) -> _ = increment;`. Holes are fresh at declarations;
generalized schemes additionally instantiate fresh variables at uses.
`--types` prints public global signatures with normalized quantified binders.

Eligible global variables now generalize: `fn identity(x) -> _ { return x; }`
can be called at both `i64` and `bool`, including through aliases. Recursive
components infer shared monotypes before generalizing; recursive calls within
each instance stay monomorphic. Unconstrained weak/mutable variables remain
errors. There is no subtyping, effect inference or guessed numeric/ABI default.
Literals and arithmetic already have fixed `i64` types.

Default inference budgets are 65,536 graph nodes, 4,194,304 work steps, 257
resolved type levels (including the declaration signature), and 16,384 expanded
nodes per type. Cycles, unconstrained holes and excessive growth are located
errors, not code-generation placeholders. Inference does not relax JSON
ownership, void-parameter, definite-return or raw-FFI restrictions.

See `examples/inference.on1x` and [inference semantics](docs/language.md).

## Anonymous functions and escaping closures

```on1x
fn add_by(amount) -> _ {
  return fn(value) -> _ { return amount + value; };
}
fn main() { let add_two = add_by(2); println(add_two(40)); }
```

Lambdas capture free immutable values, including tuples and other functions. Environments
survive their defining scopes and release automatically on normal lexical
exit/return. Nested functions forward transitive captures; calls preserve the
same callee-first, left-to-right order. Noncapturing functions do not allocate.
Mutable function assignment retains before releasing, including self-assignment;
temporary environments release per expression/condition rather than accumulating
through loops. Long capture chains are destroyed iteratively.

Capturing a `var` or JSON owner/view is rejected. Take an explicit immutable
scalar/function/tuple snapshot when needed. Lambdas may mutate their own local
variables or create/drop local JSON roots without capturing them. There is no
recursive `let`, capture mutation, tracing GC, thread-safe reference counting,
foreign callback interface or unrestricted higher-rank inference. Explicit
`forall` parameters/values are supported as described below. Literal-derived strings are
static C strings; capturing a raw foreign string does not own its buffer or
verify its lifetime.

The closure representation changes the **internal** generated-code ABI.
Rebuild emitted C and `on1x_runtime.c` together; a version guard rejects
incompatible headers. Allocation/reference-count errors terminate with status
72, without general unwinding. This is not a stable C extension ABI or a
general ownership/effect proof.

See `examples/closures.on1x`, [language semantics](docs/language.md), and the
[closure runtime/ownership manual](docs/closures.md). Capture metadata is bounded
to 16,384 entries and 2 MiB of copied names per unit; the per-file anonymous-
function count is bounded at 2,048 in addition to existing parser/type limits.

## Tuples and local destructuring

```on1x
fn swap(pair) -> _ { let (a, b) = pair; return (b, a); }
fn main() {
  let (flag, answer): (bool, i64) = swap((42, true));
  assert(flag); println(answer);
}
```

Tuples are ordered, exact structural products of at least two elements.
Construction evaluates left-to-right; nested local `let` patterns evaluate the
initializer once and bind immutable copies. Tuple types and patterns participate
in monomorphic inference, including `_` holes and occurs checks. `(value)` and
`(type)` are grouping, not singleton tuples; empty tuples and trailing commas
are unsupported. `_` remains an ordinary name, not a wildcard. A `var` may hold
and replace a whole tuple, but mutable patterns/fields are unsupported.

C11 records store tuples by value without a separate allocation. Nested function
fields retain/release their environments on copies, returns, pattern bindings
and closure captures. Scalar-only products need no drops. JSON/void elements and
raw tuple-valued extern ABIs are rejected; no foreign aggregate layout is guessed.
Tuple equality/ordering/printing, field projections and
general pattern matching are not implemented. Extract fields with local `let`.

See `examples/tuples.on1x`, [language semantics](docs/language.md), and the
[tuple ownership/lowering manual](docs/tuples.md).
`examples/parameter_patterns.on1x` demonstrates inferred/nested function and
lambda parameters, including escaping captured fields. Raw extern parameter
patterns require explicit scalar flattening; tuple results and foreign aggregate
layouts remain unsupported.

## Polymorphism and flattened extern patterns

`let id = fn(x) -> _ { return x; }; id(42); id(true);` now uses a generalized
local scheme with fresh instantiations. Immutable nonexpansive names, lambdas
and tuples can generalize, including immutable destructuring leaves.
Mutable/effectful bindings do not infer schemes. Globals (including recursive
components/imports) also generalize; parameters vary between instances.
Unused generic definitions are still checked. Concrete C11 specializations are
created at the original binding point under explicit compiler budgets.

For multiple types **within one body**, annotate a universal parameter:

```on1x
fn use(id: forall(a) fn(a) -> a) { println(id(42)); println(id(true)); }
fn main() { use(fn(x) -> _ { return x; }); }
```

Universal arguments undergo rigid checking, not just checking observed calls.
The bounded predicative C11 implementation uses typed closure bundles with
normal field ownership, not erased values or function-pointer casts. Nested
`forall` binders and inferred polymorphic recursion remain unsupported.

`extern fn raw((x, flag): (i64, bool)) -> i64;` explicitly flattens to the raw C
ABI `int64_t raw(int64_t, bool)`. Arguments evaluate once, nested patterns
flatten left-to-right, and all annotations/leaves must be concrete scalars.
This does not guess a struct ABI or make the foreign function safe.

See `examples/global_polymorphism.on1x`, `examples/destructuring_polymorphism.on1x`,
`examples/polymorphic_parameters.on1x` and the
[scheme/specialization and extern-pattern manual](docs/polymorphism.md).

## JSON ownership pilot

The checked JSON pilot offers builtins:

| On1x operation | Underlying behavior |
|---|---|
| `json_parse_or_die(string) -> json` | Create owned JSON root; terminate on error |
| `json_get_or_die(json|json_view, string) -> json_view` | Borrow child; terminate if missing |
| `json_size(json|json_view) -> i64` | Return count with bounds check |
| `json_number(json|json_view) -> i64` | Checked integral conversion of JSON number |
| `json_print(json|json_view) -> void` | Serialize then free temporary text |

The `json` root is a scope-owned resource: the generated C11 code emits
`on1x_json_drop` at lexical exits and returns. `json_view` cannot be returned,
passed as a user-function argument, reassigned, or copied into another local.
Borrowing operations require a named, still-in-scope root/view rather than a
temporary. There is no mutating JSON API, avoiding borrow invalidation in this
initial subset. There is **no** `Option`/`Result` error return here: `or_die`
operations terminate on error. This is intentionally not yet the promised full
Chisel-backed ownership and effect system. See `docs/roadmap.md`.

## ExoML integration (opt-in)

ExoML's `cdecl::extract_c11` and `contract::validate_contract` are implemented
in LibStone. Use `exoml-bind extract` and `exoml-bind validate` to obtain a
versioned C declaration inventory and reviewed semantic sidecar. To build the
On1x binding assistant with LibStone's `Moonstone::ExoML` SDK:

```sh
cmake -S on1x -B on1x/build-exoml \
  -DON1X_REQUIRE_LIBSTONE=ON -DON1X_WITH_EXOML=ON
cmake --build on1x/build-exoml -j4
on1x/build-exoml/on1x-bind verify file.cdecl.json file.exocontract.json
```

`on1x-bind emit DECL CONTRACT OUT_PREFIX` generates `OUT_PREFIX.on1x` and
`OUT_PREFIX.c` for a **restricted** subset: reviewed contracts on signed-integer
C ABI functions with no ownership/callback/error contracts, whose parameters
and return are fixed-size signed scalar integers or `void`. Each generated C11
adapter checks input integer ranges, result/parameter sizes, and includes the
real C header. It is still an **unsafe raw extern import**, not an automatically
safe foreign value. All pointer, vector, aggregate, varargs, callback, and
ownership-bearing signatures are explicitly skipped with a count. The entire
MuLIB API cannot be generated this way today.

Example integration:

```on1x
import "generated/SomeKernel.on1x";
fn main() -> i64 {
  println(on1x_ffi_example_i64(42));
  return 0;
}
```

You must link the generated C adapter and the corresponding MuLIB implementation
sources, e.g. `--extra-c generated/SomeKernel.c` and an implementation TU with
correct `MULIB_IMPL_*` macros. The existing build helper only automatically
selects the documented JSON implementation recipe, not arbitrary modules.

**Do not** interpret a reviewed contract as a formal proof of the C body's
behavior. ABI matching and ownership claims require native build verification,
tests, and eventual Chisel/Wetstone usage checks. The ExoML gate in this tool
checks basic reviewed-state/revision/fingerprint consistency; it is not the full
On1x safe-export gate described in `AGENTS.md`.

## Testing and current limitations

`ctest` registers scanner API, structural types, higher-order functions, inference,
closure HIR/native runtime, tuple HIR/native ownership,
compiler, input-limit and live MuLIB JSON tests,
plus differential scanner tests in generator-backed builds. Scanner tests cover
arena-independent token storage and repeated/concurrent calls. JSON tests use
the real kernel for nested lifetimes, numeric bounds, malformed inputs and
injected allocation failures. Without canonical `../include/mulib-json.h`, the
separate `on1x.json` test is reported **SKIP**, not PASS.

Generated programs always compile with `-Wall -Wextra -Wpedantic -Werror`.
`ON1X_TEST_C_FLAGS` supplies extra compiler/linker options as a CMake
semicolon-separated list, so sanitizers can cover the generated programs and
MuLIB kernels as well as the compiler. See [tests](tests/README.md) for the
test layout and [verification](VERIFICATION.md) for reproducible commands.

This is a first vertical slice. It does **not** claim full On1x language
coverage, production-grade memory safety, total MuLIB coverage, direct native
code generation, or completion of all recommendations in the ExoML AGENTS.md.

See [development instructions](AGENTS.md), [architecture](docs/architecture.md),
[roadmap](docs/roadmap.md), and [language reference](docs/language.md).
