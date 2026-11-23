# On1x architecture and trust boundaries

The [M2 defer/error continuation](defer-errors.md) uses existing canonical value
regions for nominal Result propagation and a private typed deferred thunk after
registration-time operand snapshots. C11 owns a bounded stack per registering
function invocation; error returns and normal returns drain it before remaining
lexical cleanup. The verifier checks exact returns, tag/initialization/move facts
and thunk shapes. The later bounded CSP profile adds explicit
cooperative-cancellation cleanup returns; this is not general exceptions,
fatal-trap unwind, typed cancellation recovery or a resource/effectful MIR
extension. Unsupported MIR/SDK profiles stay closed.

`src/nominal.cpp` validates the source declaration graph before layout expansion
and reconstructs nominal record/sum representations independently of inference.
Names/arguments remain invariant metadata on existing value-data types; source
`Named` references are never an admitted backend type. By-value cycles reject,
while the later [unique-owner profile](recursive-owning-adts.md) permits a cycle
only across finite `own<Named>` indirection. C11 ingress verifies catalog/layout
consistency; MIR still rejects ownership-bearing layouts. See the
[initial profile](nominal-adts.md).

The [intrinsic control bridge](core-syntax.md) canonicalizes statement ranges
after source transforms/staging and resolves loop exits before type checking.
It reassigns identities after specialization and independently verifies targets,
source points and function boundaries before C11/MIR or SDK admission. This is
not the full cleanup/effect/suspension Core proposed by the broader specification.

Value-loop expressions now use this same pipeline: control normalization resolves
their lexical targets, ordinary inference/checking joins break operands, and
value lowering erases them to while/region/initialize/leave operations. Definite
and possible initialization checks protect result joins and repeatable backedges.
No separate execution IR or runtime was introduced; see [value loops](value-loops.md).

`src/patterns.cpp` extends the shared name/tuple pattern model with structural
matching and bounded finite-constructor coverage. After inference/sema,
`src/canonical/values.cpp` transactionally replaces both match spellings with
typed decision regions, single-initialization slots and branches/projections.
`verify_values.cpp` independently checks initialization, tag domination,
projection types and region transfers before emitters/SDKs. Source PPX cannot
carry private regions or checked payload-access facts. This is an extension of
the existing HIR/MIR bridge, not another parser or execution IR.

## Phases

The optional [Firestone and Novostone HIR passes](../hir/README.md) run inside
the processor's checked-HIR transaction, before authored rewrites. They fold
eligible scalar expressions; Firestone can also remove a checked divide by
literal one without dropping its numerator. The processor and C11/MEMGraph
emitter consume the rechecked On1x HIR, not either library's internal representation.

An opt-in backend-neutral [scalar MIR v1](backend-mir.md) now branches from checked
HIR into verified blocks/SSA values, function-local scalar slots and effect
chains. It has deterministic round-trip serialization, a bounded reference
evaluator and C11 lowering. The direct HIR-to-C11 path below remains the default.
MIR rejects types/operations outside its documented subset rather than using
unchecked foreign or closure representations.

The optional [relational analysis layer](backend-analysis.md) projects verified
MIR into a versioned metadata dialect. Novostone's public Datalog frontend parses
the embedded `src/backend/Analysis.dl` catalog; CCDB publishes verified fact deltas
with exact snapshot identity and dependency-aware caches. Queries compute CFG
reachability, call/effect closure and edge-aware block liveness. MIR remains the
semantic authority; these facts do not use Novostone's wrapping-i32 IR or authorize
rewrites. Source and installed-package providers are independently selectable.

The opt-in [scalar pass manager](backend-passes.md) runs checked constant folding,
constant-branch cleanup, local-slot forwarding/overwritten-store removal and dead
pure-value/phi elimination. Each pass edits a
separate candidate and commits after MIR and preservation verification. Shared
budgets bound attempts; exhaustion returns the last verified prefix with explicit
status. Exact input/output MIR, options and versioned pass events form deterministic
provenance. Optional `Store::optimize` publishes that result against the exact CCDB
source snapshot, with cache invalidation through actual fact deltas.

```
     .on1x source
         │
     LibStone Aurocks scanner (generated C99)
         │       └─ ASDL token AST (generated C11, temporary arena)
         │                  OR portable scanner fallback
         ▼
      C++ token collector → precedence parser / typed HIR
            │
      canonical module graph / prelude / lexical namespace lowering
            │
      typed hygienic AST expansion (syntax contexts → lexical names)
           │
      optional authorized PPX chain: S-expression AST → process → validated AST
           │
      quotation-world checks / AOT staging factories
           │
      optional constraint solving / universal elaboration / grounding
          │
     static checker: types, scopes, immutable variables,
     resource escape restrictions, simple definite returns
         │
         ▼
      C11 backend → generated program translation unit
         │                 ├─ on1x_runtime.c
         │                 ├─ optional on1x_json.c
         │                 └─ planned MuLIB implementation TU + native companions
         ▼
     ISO C11 compiler → native binary with MuLIB kernels
```

The ASDL and Aurocks scripts are external; CMake invokes them from
`<mulib>/third_party/libstone/scripts/`. They are not copied into this tree.
The Aurocks parser's global parser state is serialized by the C++ scanner.
Temporary ASDL arena allocations are destroyed after token callback delivery.
The C++ typed HIR owns its own tokens and tree nodes.

`src/loader.cpp` owns per-invocation canonical-path caches, bounded dependency
loading, explicit import bindings and deterministic identifier lowering. The
automatic prelude is demand-loaded; explicit imports retain their definitions.
Namespaces disappear before the v1 PPX wire boundary, preserving the existing
type, ownership and C11 pipeline. See [the module contract](modules.md).

`stdlib/mulib/housekeeping.py` is shared by the native build helper and Exotract.
It joins MANIFEST kernel dependencies with explicit `library/` fixture recipes,
separates kernel implementation placement from companion C translation units,
and obtains include provenance from the C compiler. `std.mulib` reads generated
immutable metadata and byte tables through an exact scalar C ABI. Typed header
discovery remains Exotract/ExoML; build recipes do not admit general foreign
pointer, record or ownership types.

The standard-library `_bridge/mulib_bridge.c` is a declaration/direct-call
adapter TU, not a second kernel implementation. The helper enables only the
required `ON1X_BRIDGE_*` groups and adds their kernels to the shared plan.
Operation-local scalar status and immutable bounded snapshots support source
tuples without exporting foreign pointers/resources through the raw ABI.
Usage remains outside general ExoML/Chisel safe admission.

The metaprogramming pass runs before type inference. `grammar/PPX.schema.json`
is embedded by CMake; generated builds additionally use the real ASDL-generated
`PPX.asdl` wire tree for arena-backed structural checks. Macro syntax marks are
not backend identifiers: caller and introduced contexts resolve by lexical
binding before PPX serialization. External output cannot inject checked HIR,
execution registrations or compiler proofs. It must lower all extension nodes
and pass the ordinary checker. PPX execution is authorized, bounded and shell-free,
but not sandboxed; AST rollback does not roll back process side effects.
See [the protocol, hygiene and trust manual](metaprogramming.md).

`src/macro_rules.cpp` matches recursive structural patterns using ranked capture
paths and repetition widths, including empty dimensions. Template expansion
checks ranks and equal driver widths. Matching/backtracking and copied metadata
have independent budgets; caller/introduced syntax contexts remain distinct.

`src/macros.cpp` also owns lexical `let_syntax`/`letrec_syntax` groups. Definition
records retain immutable value/syntax environment snapshots; recursive syntax
references use stable pool indices, not shared ownership cycles. Context marks
identify the defining transformer, while source and introduced value binders
become distinct collision-checked names. Nonrecursive groups see outer syntax;
recursive groups see the whole group. `SyntaxLet` and its definitions are erased
transactionally before the wire boundary, with no runtime environment or ABI change.

`src/staging.cpp` runs only after PPX source reconstruction. Lexical bindings
carry quotation-world identity as well as depth, preventing scope extrusion when
a splice re-enters a different quotation at the same depth. Quotes become typed
factories/thunks; splice/lift operands become factory arguments. `Type::Code`
remains distinct through inference/checking but uses exact zero-argument closure
structs in C11. Lexical quotation regions are parser-lowered to an existing
result-inferred lambda call, retaining statement/tail locations and the Unit-v1
wire schema. No runtime compiler or AST evaluator is linked. See [MSP](staging.md).

Native authoring libraries in `stdlib` implement codec, JSON/schema, visitor and
processor logic in On1x. `src/primitives.cpp` declares the small byte-buffer,
persistent S-expression and process-I/O substrate; semantic/codegen support
retains/releases managed references through the existing iterative environment
queue. Dynamic strings preserve the C-string ABI with a bounded normal-exit
snapshot pool, not unchecked borrowed buffer pointers. Managed foreign ABIs are
rejected. See [the self-hosting/data contract](selfhosting.md).

The internal C scanner receives an explicit byte length, not an unbounded
NUL-terminated source. Its C++ entry point enforces the per-file 2 MiB limit and
rejects NUL before entering either scanner. Callback text is temporary and is
copied into C++ storage before arena destruction. API tests exercise bounded
non-terminated buffers, retained tokens, error recovery across calls, and
concurrent entry through the serialized wrapper.

The recursive-descent parser bounds active block/expression/type/pattern nesting and HIR
expression depth at 256 levels. Tracking expression depth while constructing
the tree also bounds later checker/backend traversal and unique_ptr destruction
for flat left-associative chains; a source-size limit alone does not do that.
Depth accounting is parser-local and does not alter resource ownership or the
generated program's C ABI.

`src/lexer_fallback.cpp` is a bootstrap implementation of the same token
callback API, not the default when the LibStone generator scripts exist.

## P1 function-value representation and closure conversion

`include/on1x/type.hpp` / `src/type.cpp` provide immutable structural monotypes.
Function parameter/result and tuple-element graphs share owned C++ storage, with structural
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
tracks owned function/tuple temporaries separately from lexical JSON/value locals,
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
and tuple constraints and an iterative occurs check. Composite roots are not prematurely
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
inference. Immutable nonexpansive lets, including pattern leaves, have bounded
local schemes. Globals generalize after shared strongly connected component
inference and recursive components specialize together. Ordinary parameters are
monomorphic per instance; explicit universal annotations use rigid introduction
checking and predicative demand bundles. Effect inference is not implemented.
Generic prototypes are checked
with scoped quantified identities but not emitted. Weak environment/domain
variables are shared across instances. See [the elaboration contract](polymorphism.md).

## P1 product types and irrefutable local patterns

`Type::Tuple` is distinct from `Type::Function`: shape/arity, structural equality,
canonical inference keys and diagnostics preserve that distinction. Immutable
type shapes cache hole presence and whether values contain closure references.
Unification/occurs checks traverse both constructors, and iterative grounding
checks depth/expanded size including tuple elements and declaration signatures.

Tuple expressions evaluate fields left-to-right. Local `let` patterns are
C++-owned name/tuple trees with located leaves; the existing parser nesting
budget bounds them. An initializer is checked before introducing any leaf
bindings. A pattern constrains an unknown monotype to its exact tuple shape;
it neither matches dynamically nor generalizes field types. Ordinary function
and lambda parameters accept the same patterns, projecting borrowed arguments
into owned immutable locals in the entry prologue. Unlexable internal argument
names avoid collisions and are mapped to valid unique C names. Extern parameter
patterns explicitly flatten scalar leaves, not aggregate layouts. Mutable patterns,
resource/void elements and raw aggregate extern signatures are rejected.

The C11 backend emits deduplicated, dependency-ordered record typedefs. Function
and tuple records can nest in either direction, but type cycles are rejected.
Tuples are copied by value without separate allocation. Only tuples containing
functions need generated retain/release operations, which operate through const
pointers and call child helpers rather than expanding shared types inline.
Intermediate pattern projections also use borrowed typed pointers: access paths
are linear in pattern size, not copied in full into every leaf. Each resulting
local retains any nested environments before the initializer temporary drops.
Parameters borrow, returns own references, and assignment acquires the new
product before releasing the old one. Closure factories/destructors apply the
same operations to tuple capture fields. The environment runtime ABI stays at
version 1; aggregate foreign interoperability is **not** enabled. See the
[tuple ownership/lowering manual](tuples.md).

## Kernel discovery

- Gate A: ExoML CDecl scanner and ABI/target evidence.
- Gate B: strict full ExoML contract validation and drift check. Independent
  accountable review is an additional requirement, not a flag-derived proof.
- Gate C (not yet implemented): C11 ABI adapters for pointers, callbacks,
  error conventions, records, strings, conditional ownership transfers.
- Gate D (not yet implemented): Chisel proof obligations, Wetstone call models,
  borrowed lifetimes and invalidations, and effect summaries.
- Gate E (not yet implemented): idiomatic On1x `Option`/`Result`/`Owned` facades.

The optional `on1x-bind` tool performs real extraction, lossless public-CDecl
transport, pin/check/explain and reviewed-metadata raw scalar adapters. The
source-backed JSON/Vec/Parsec profiles remain annotated/unreviewed; their locked
workflow checks source/toolchain/artifact identities and full contracts without
using the lossy legacy ownership projection. See [contracts](contracts.md).

The scalar emitter provides exact C11 signature/width/signedness/range checks, **not**
Gate C/D/E safe imports. Functions outside that scalar subset fail closed by
being skipped. Every file that uses an `extern fn` is treated as untrusted raw
interop in the current frontend. It is not permissible to represent an unowned
`void*` as a safe owning `json` value.

## Resource model in vertical slice

Static MuLIB build profiles now live in `contracts-exotract`, backed by the
canonical MANIFEST and real Exotract/ExoML declaration discovery. The generator
produces typed import metadata and a single implementation-TU archive; existing
JSON C11 facade calls can link that archive. Semantic sidecars and their audit
workflow live in `contracts-exoml`. General foreign-handle ABI lowering remains
an independent compiler obligation.

An optional [Nerotypical structural checker](type-safety.md) independently
validates concrete HIR type obligations after elaboration and before emission.
It uses the public structural solver with a separately prepared Satie source SDK,
including an explicitly identified upstream CMake patch. It does not replace
inference or the resource analyses below.

The opt-in [Chisel adapter](memory-safety.md) lowers the scalar/JSON HIR subset to
resource CFGs and checks ownership, loans and lexical exit cleanup with the actual
Moonstone solver. It adds all-path loan and may-live checks around the upstream
checker. Its public event API supports modeled external protocols; general
contract-driven foreign-handle lowering remains open.

Separately, the mandatory canonical value verifier implements the bounded
[On1x place-borrow profile](core-references.md). Shared/exclusive parameter and
lexical modes remain metadata over the referent type. The verifier links each
loan to a lexical binding identity and root, composes conflicts with canonical
move/partial-move state, and rejects retention or suspension. Direct C11 lowers
that admitted slice to private `const T *`/`T *` calls; owner payload access is
size-checked through the unique runtime. Chisel, Nerotypical, PPX, MIR,
public/external SDK, and foreign adapters reject this metadata until they gain
equivalent place, loan, invalidation, and effect representations. The private
built-in C11 bridge invokes that same canonical emitter and is not an
independent admission. Advisory Wetstone translation skips such functions and
supplies no safety admission.

- `json` can only originate from `json_parse_or_die` bound to `let`.
- `json` and `json_view` cannot be declared as function arguments/returns.
- Resource assignment, copying, or reinitialization is prohibited.
- JSON owners/views cannot be captured, even by a nonescaping lambda.
- JSON owners/views cannot be tuple elements, at any nesting depth.
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
The contract tool now implements explicit manual-backed JSON, Vec and core
Parsec plans; optional owned-parser switches are never silently added. It is
not a general resolver for every MuLIB profile.
