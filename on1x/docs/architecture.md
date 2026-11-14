# On1x architecture and trust boundaries

## Phases

```
     .on1x source
         │
     LibStone Aurocks scanner (generated C99)
         │       └─ ASDL token AST (generated C11, temporary arena)
         │                  OR portable scanner fallback
         ▼
     C++ token collector → precedence parser / typed HIR
          │
     optional monotype constraint solving / grounding
          │
     static checker: types, scopes, immutable variables,
     resource escape restrictions, simple definite returns
         │
         ▼
     C11 backend → generated program translation unit
         │                 ├─ on1x_runtime.c
         │                 ├─ optional on1x_json.c
         │                 └─ on1x_mulib_impl.c
         ▼
     ISO C11 compiler → native binary with MuLIB kernels
```

The ASDL and Aurocks scripts are external; CMake invokes them from
`<mulib>/third_party/libstone/scripts/`. They are not copied into this tree.
The Aurocks parser's global parser state is serialized by the C++ scanner.
Temporary ASDL arena allocations are destroyed after token callback delivery.
The C++ typed HIR owns its own tokens and tree nodes.

The internal C scanner receives an explicit byte length, not an unbounded
NUL-terminated source. Its C++ entry point enforces the per-file 2 MiB limit and
rejects NUL before entering either scanner. Callback text is temporary and is
copied into C++ storage before arena destruction. API tests exercise bounded
non-terminated buffers, retained tokens, error recovery across calls, and
concurrent entry through the serialized wrapper.

The recursive-descent parser bounds active block/expression/type nesting and HIR
expression depth at 256 levels. Tracking expression depth while constructing
the tree also bounds later checker/backend traversal and unique_ptr destruction
for flat left-associative chains; a source-size limit alone does not do that.
Depth accounting is parser-local and does not alter resource ownership or the
generated program's C ABI.

`src/lexer_fallback.cpp` is a bootstrap implementation of the same token
callback API, not the default when the LibStone generator scripts exist.

## P1 function-value representation and closure conversion

`include/on1x/type.hpp` / `src/type.cpp` provide immutable structural monotypes.
Function parameter/result graphs share owned C++ storage, with structural
comparison and diagnostic rendering; they are not foreign CDecl IDs or a
generalized inference engine. The semantic AST stores an explicit callee for
every call and records whether each name resolves to a local, ordinary
function, raw extern or intrinsic. This avoids re-resolving a shadowed name
differently in the backend.

The C11 backend emits dependency-ordered, deduplicated structs containing an
exactly typed invocation pointer and an `on1x_env *`. The invocation pointer
takes that environment as an explicit first argument. Ordinary top-level
functions use typed adapters and null environments; anonymous functions have
lifted static code and optional heap environments. There are no object-pointer
to function-pointer casts or foreign calling-convention guesses. Generated
symbols avoid declared raw extern names; numbering is structural/deterministic,
not based on allocation addresses. No LibStone application dependency is added.

Semantic checking records immutable free values in first-use order, forwarding
transitive captures through enclosing lambdas. An explicit scope boundary
distinguishes captures from parameters/body locals even under shadowing.
Mutable and JSON owner/view captures fail closed. Capture count/name budgets
bound transitive metadata amplification. Resources/views remain forbidden
recursively throughout function signatures. There is no recursive `let` or
mutation of captured fields, so generated environments form an acyclic graph.

Generated factories copy captures and retain any captured function environments.
Generated field destructors release those references. The small C11 runtime
reference-counts environments and drains zero-count cascades through an
iterative queue. Call arguments borrow; results own references. The backend
tracks owned function temporaries separately from lexical JSON/function locals,
keeping the callee and earlier arguments alive across nested logical operands.
Return values acquire a reference before lexical cleanup. Assignment acquires
the replacement before releasing the previous value. Tests count environments,
inject allocation failures, and exercise deep destruction under sanitizers.

Raw externs and intrinsics remain named direct-call operations. Extern names
cannot escape as values, and all function-valued foreign parameters/results are
rejected. Ordinary wrappers may contain raw calls but inherit their unsafe
behavior; this is not a safe-export or effect-inference gate. Resource captures
still require proper lifetime/drop analysis. The closure ABI has an explicit
version guard but remains internal; emitted C and runtime must be rebuilt
together. It is single-threaded and does not provide error unwinding. See
[closure lowering and runtime manual](closures.md).

## P1 monomorphic inference and its admission boundary

`include/on1x/infer.hpp` / `src/infer.cpp` implement a compiler-local term graph.
Anonymous holes are fresh at declarations, while calls/aliases share monotypes.
The graph uses ranked variable unions, path compression, structural function
constraints and an iterative occurs check. Function roots are not prematurely
merged: doing so could introduce cycles before child constraints are checked.
Term IDs never escape the pass and are not foreign CDecl IDs or type schemes.

Grounding is iterative, memoized and structurally interned. It checks both
depth and expanded type size, so a small shared graph cannot produce an
exponentially expanded semantic type or diagnostic. Hole presence is cached
in immutable structural types, and repeated valid function-signature checks
are memoized. Diagnostics have separate term/depth rendering limits. Default
node/work/depth/size budgets are specified by `InferenceLimits`; tests lower
each budget to exercise its failure path.

AST type fields are replaced only after all inference slots and declaration
signatures resolve successfully. Constraint or budget failure leaves those
fields unchanged. The compiler then runs the **existing** strict type/resource/
ABI/definite-return checker. Intrinsic overload restrictions and resource rules
are not weakened merely because a variable was initially unknown. Codegen sees
only concrete monotypes; inference itself requires no boxing or function
specialization. Anonymous-function annotations and bodies use the same term
graph with independently scoped return constraints. Fully annotated units
bypass the constraint pass.

Compiler clients should call `check(unit)` for admission. `infer_types(unit)`
is an isolated type-solving phase, not an ownership/FFI proof or complete
semantic validation. Extern annotations must already be concrete before
inference. No let-generalization, value-restricted polymorphism or effect
inference is claimed.

## Kernel discovery

- Gate A: ExoML CDecl scanner and ABI/target evidence.
- Gate B: ExoML reviewed semantic contract and drift check.
- Gate C (not yet implemented): C11 ABI adapters for pointers, callbacks,
  error conventions, records, strings, conditional ownership transfers.
- Gate D (not yet implemented): Chisel proof obligations, Wetstone call models,
  borrowed lifetimes and invalidations, and effect summaries.
- Gate E (not yet implemented): idiomatic On1x `Option`/`Result`/`Owned` facades.

The `on1x-bind` prototype can emit raw signed-integer C adapters, **not**
Gate C/D/E safe imports. Functions outside that scalar subset fail closed by
being skipped. Every file that uses an `extern fn` is treated as untrusted raw
interop in the current frontend. It is not permissible to represent an unowned
`void*` as a safe owning `json` value.

## Resource model in vertical slice

- `json` can only originate from `json_parse_or_die` bound to `let`.
- `json` and `json_view` cannot be declared as function arguments/returns.
- Resource assignment, copying, or reinitialization is prohibited.
- JSON owners/views cannot be captured, even by a nonescaping lambda.
- Borrowed views originate only from a named live owner/view.
- No mutating operations on JSON trees are exposed, so existing views do not
  suffer mutation-driven invalidation.
- Drop is emitted in reverse declaration order at lexical scope exit or any
  explicit return. No general exception/unwinding support exists.

The next major step is to replace these ad hoc restrictions with the rich
ExoML `FunctionContract` type, a stateful Chisel foreign-call model and
Wetstone external effects. The coarse `OwnershipContract` projection is
lossy: preserve full contracts for conditional transfer, invalidation and I/O.

## Build correctness

MuLIB modules have single-header implementation switches defined in exactly
one C translation unit. The current JSON pilot uses a single recipe sourced
from MuLIB documentation. Arbitrary kernel build plans should be synthesized
from MANIFEST.yaml plus the module's actual examples/manuals, respecting
implementation-only dependencies, platform/runtime profiles and transitive
linkage. No globally defined `MULIB_IMPL_*` macros are allowed.
