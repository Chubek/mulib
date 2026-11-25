# Core/MIR: scalar, product and directive formats

MIR v4 carries conditional directives alongside the ordinary verified functions.
It extends v3; programs without directives can still use the earlier wire
formats. A directive names a backend, target, action, optional function ID
and whether applying it is speculative. Empty selectors match any product.
The verifier rejects unknown actions and dangling function IDs. A directive
cannot supply an instruction, shell argument, library path or foreign symbol.

`--emit-ir` preserves these clauses. When a product is selected, only matching
directives apply. `--optimize-ir` permits speculative rewrite candidates; a
matching pattern still has to establish that its change preserves values,
traps and effects. The pattern language is in
[`mir/MIR.llsynth`](../mir/MIR.llsynth) and its database is in
[`mir/rewrite`](../mir/rewrite). Use `--mir-target TRIPLE` with `--emit-ir-c`,
`--emit-vm`, `--run-vm`, `--run-ir`, or `--ir-cfg` to select directives for an
explicit target. The option does not infer a host target or change the product's
ABI. `kernel-hot` requests
hot function placement, and `private-code` can hide a helper from ELF dynamic
symbol export. `cold-code` requests cold-function optimization and placement;
`no-inline` prevents the downstream C compiler from inlining the function.
These two hints require an existing function ID, not module scope, and affect
only `mir-c11` when backend, target and speculation settings match. Each emits
its C attribute only if the downstream compiler advertises support. A matching
`cold-code` hint takes precedence over `kernel-hot` for C placement only, not
for MIR rewrites. These hints do not change the C call signature, effects, or
admission to native code.

`rewrite memgraph-local` and `rewrite meta-canonical` are function-scoped
MIR v4 optimization requests. Their speculation bits select the safe or
speculative pattern sets respectively; the latter also require `--optimize-ir`
or another explicit compiler-side opt-in. Unknown or missing proof, a missing
target for a target-specific request, or an unmatched product leaves a request
inactive. See the [scoped rewrite contract](../mir/README.md#scoped-rewrite-actions-for-hir-producers).

The [foreign interface guide](ffi-migration.md) shows how a standard MuLIB
kernel becomes an ordinary callable MIR function.

The [defer/Result C11 profile](defer-errors.md) does not widen MIR. Dynamic
registrations require cleanup/effect descriptors; nominal results require
aggregate descriptors. Requested MIR emission/evaluation rejects both rather
than dropping deferred effects or approximating an error value as a scalar.

The later [nonrecursive nominal-data profile](nominal-adts.md) retains nominal
identity on the existing bounded record/sum HIR representation. It executes only
through HIR→C11. MIR ingress validates catalog/layout consistency, then rejects
unsupported aggregate values rather than erasing identity to a scalar. There is
no new opcode, serialization version or native backend in this increment.

The [recursive unique-owner profile](recursive-owning-adts.md) is also
HIR→C11-only. MIR ingress explicitly rejects `own<Named>` and any containing
type until the IR has typed move/drop/place operations and cleanup edges.

This is the first executable **B1 subset** from `BACKEND.md`. The implementation
is in `include/on1x/backend/mir.hpp` and `src/backend/`.
[`hir/lower_mir.cpp`](../hir/lower_mir.cpp) lowers checked, specialized HIR after
imports, macros, PPX and staging admission.
The default `--emit-c` continues to use the established HIR emitter.

Intrinsic [statement loops](core-syntax.md) now lower through explicit
preheader/header/body/latch/exit edges, including labeled break/continue and
once-evaluated i64 ranges. No new MIR opcodes/format version are needed. CFG,
effect, slot-initialization and dominance verification remain mandatory; the
reference evaluator/round-trips/transactional passes execute the same edges.
Resource-bearing loops stay on HIR→C11 and are not approximated as scalar MIR.
For a canonical `for` step, MIR creates a latch only when a lowered body edge
can enter it (fallthrough or a `continue` to that loop). In particular, a
`continue` inside an omitted inner step does not create an outer latch. Latches
may themselves transfer to an enclosing loop; the entered steps execute in
source order. MIR still checks omitted steps for unsupported types, ownership
and effects, but emits no edges or instructions for them. Region exit joins
likewise require an emitted incoming exit rather than a syntactic target alone.

### Scalar lexical loans

Checked `let r = borrow x` and `let r = borrow mut x` can lower when the
referent is a bool or a supported numeric scalar. This includes ordinary
by-value parameters as roots and nested reborrows. MIR uses a single typed
function-local slot for the root; every read of the root or a live alias uses
that slot. Loan acquisition has no runtime operation and does not copy the
referent. `r := value` evaluates `value` once, then writes the same slot if
evaluation completes. If evaluation exits or traps, it does not write the old
slot. An ordinary by-value read of an alias still takes a scalar snapshot.
Scalar `ref_replace(r, value)` evaluates the RHS once before loading the old
value and storing the replacement. If the RHS transfers control or traps, it
neither loads nor overwrites the old value.

The MEMGraph lexical slot plan is derived from canonical HIR verification of
loan conflicts, scope and control flow. Its root identities refer to specific
declarations rather than identifier spellings.
MIR sees only existing typed, effect-ordered `load`/`store` instructions with
normal slot-initialization verification. There is no pointer ABI, executable
loan object, cleanup opcode, or wire format change. The same instructions run
through serialized MIR, the evaluator, MIR-C11, VM, and Archon where configured.
This does not admit borrowed *function* parameters/calls, pattern-bound roots,
aggregate or unique owner payload loans, managed values, or loan operations
in non-entered loop latches. VM closure conversion cannot
turn an unsupported source referent into an admitted scalar; converted HIR
gets its own verified plan.

[Value loops](value-loops.md) additionally use the existing region/slot bridge:
typed break operands store a result and leave a checked join. Numeric/scalar
tuples run through ordinary MIR slots/edges and optimization/serialization without
a new format. Managed/aggregate loop results remain HIR→C11-only. No bottom type,
resource transfer, exception unwind or concurrency opcode is implied.

Guarded [structural matches](core-syntax.md) lower to verified typed decision
regions before MIR. Numeric/bool values and trivial tuples use existing slots,
extracts, branches and jumps in v1–v3, with mandatory initialization verification
and differential tests. Continues nested in expression regions still execute the
target latch. Records/sums/arrays/slices, strings and managed/foreign resources
remain unsupported MIR profiles; C11 matching does not widen those boundaries.

### Canonical boolean decision lowering

After ordinary canonical verification, `hir/lower_mir.cpp` recognizes this
narrow, typed value-region shape:

```text
Region<bool>(
  Storage<bool> result;
  If(condition, Initialize(result, yes), Initialize(result, no));
  Local<bool>(result))
```

It emits a bool block parameter (phi), rather than a temporary slot and its
stores/load. Eligibility is structural: no generated-name prefix grants
authority. A bounded scan requires the condition and both initializer subtrees
to contain no reference or rebinding to that temporary; even a legal nested
shadowing of the spelling conservatively uses ordinary storage lowering.
Additional statements, non-bool results, unsafe outer regions, patterns and
retained prototype/template evidence do not enter this fast path.

The condition executes once before the selected initializer. Initializers still
use ordinary expression lowering, including nested decisions, lexical mutable
state, calls and checked traps. A literal `true` on the true edge or `false` on
the false edge forwards the already evaluated condition directly to the join,
avoiding a redundant block. Other arms supply their own result; terminating
arms supply no edge or phi argument. Captured operations retain their source
locations and the phi carries the original result location.

Recognition has a shared allowance of `min(Limits.work, 262144)` visits and
depth 128. Exhaustion declines the optimization; normal node/analysis limits
and all source/candidate/MIR checks still apply. There is no full mem2reg,
resource/drop promotion, or relaxation of LocalSlots predecessor restrictions.
The `.o1m` rules still execute and their verified canonical HIR remains intact.
`tests/test_mir_decision.cpp` checks structural eligibility, malformed input,
early-return initialization, budgets and round-trips; `test_mir_decision.py`
compares effect/trap traces through HIR→C11, MIR→C11, evaluation and configured
native code, with MIR optimization both off and on. Its `call_values`,
`call_first_fault`, and `call_second_fault` fixtures all exit 70: the printed
argument/callee trace, result and diagnostic distinguish normal return from
division or modulo traps and check that execution stops at the first fault.

Focused verification (2026-11-21, GCC 16.2.1, Linux x86_64):

```sh
ctest --test-dir /mnt/andromeda/tempdir/opencode/on1x-opt2-build \
  -R '^on1x[.]mir_decision(_hir)?$' --output-on-failure
c++ -std=c++20 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Ion1x/include on1x/src/backend/lower.cpp on1x/tests/test_mir_decision.cpp \
  /mnt/andromeda/tempdir/opencode/on1x-opt2-build/libon1x_frontend.a \
  /mnt/andromeda/tempdir/opencode/on1x-opt2-build/libon1x_scanner.a \
  -o /mnt/andromeda/tempdir/opencode/on1x-opt2-decision-sanitized
/mnt/andromeda/tempdir/opencode/on1x-opt2-decision-sanitized
```

Both CTests passed, including 21 differential executions with the configured
native bridge; the focused ASan/UBSan executable passed. The sanitizer command
instruments the changed lowerer and its test, not the linked frontend/scanner
archives. The same tests against the preserved `on1x-meta-build` baseline fail
their temporary-storage regression gates (the Python execution comparisons
pass before that gate). Performance evidence is recorded separately in the
[processor benchmark feedback](../benchmark/PROCESSOR_FEEDBACK.md).
The pass suite retains its lazy-source semantic checks. Its later-round
LocalSlots→CFG regression uses explicit mutable boolean storage, since direct
decision SSA no longer needs that feedback; a pipeline excluding LocalSlots
must retain the unreachable division, while the full pipeline must remove it
and commit CFG simplification in a later round.

The v2 continuation adds f64 literals/operations and **trivially copied tuples**.
Existing scalar source still lowers to v1, and existing v1 artifacts continue to
decode and round-trip as v1. Lowering selects v2 only when extended types/opcodes
are needed. This does not add managed aggregates, general records, foreign
ownership or concurrency effects.

v3 adds u8/u16/u32/u64/f32, nested numeric tuples, every numeric conversion and
wrapping/bit/shift operation. It uses the existing MIR, verifier, evaluator and
C11 emitter, not a second IR. Old v1/v2 lowering and wire shapes are preserved.

## Use

From the MuLIB root, with a built `on1xc`:

```sh
on1x/build/on1xc --emit-ir on1x/examples/mir.on1x -o /tmp/opencode/example.mir
on1x/build/on1xc --ir-input --ir-cfg /tmp/opencode/example.mir -o /tmp/opencode/example.dot
on1x/build/on1xc --ir-input --run-ir /tmp/opencode/example.mir
on1x/build/on1xc --ir-input --emit-ir-c /tmp/opencode/example.mir -o /tmp/opencode/example.c
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Ion1x/runtime \
  /tmp/opencode/example.c on1x/runtime/on1x_runtime.c -o /tmp/opencode/example
/tmp/opencode/example

python3 on1x/tools/on1x-build.py on1x/examples/mir.on1x --via-mir \
  --on1xc on1x/build/on1xc -o /tmp/opencode/mir-example
/tmp/opencode/mir-example
```

The example prints `1, 1, 2, 6, 24, 120, false`, on separate lines. `--run-ir`
executes the bounded reference evaluator; it is not a VM/native code generator.
Runtime traps preserve the preceding printed trace, report the source location,
and return 70. Evaluation exhaustion returns 75, distinct from a program trap.
An ordinary `i64` main result follows the existing C11 main exit convention.

## Supported boundary

| Feature | Admitted profile |
|---|---|
| Values | v1: i64/bool/void results; v2 adds f64/numeric tuples; v3 adds u8/u16/u32/u64/f32 and tuples containing them |
| Arithmetic | Checked i64; dedicated f64 operations in v2; all remaining typed numeric operations/conversions in v3 |
| Control | If/else, while, early return, nested lexical scopes, short-circuit logic |
| Calls | Direct ordinary On1x calls, recursion, concrete admitted specializations; tuple arguments/results and parameter destructuring in v2 |
| Observable effects | Scalar `println`, `assert`, checked arithmetic failure, local loads/stores (including verified scalar lexical aliases), conservative call barriers |
| Serialization | Versioned canonical S-expression; checked readback and exact structural comparison |
| Provenance | Existing file/byte-offset/line/byte-column locations on definitions and terminators |
| Diagnostics | `ON1X-MIR-VERIFY`, `-DECODE`, `-LOWER`, `-UNSUPPORTED`, `-EVAL`, `-C11` in the existing located diagnostic path |
| Unsupported | Strings, managed/resource-bearing aggregates, records/ADTs/arrays/slices, first-class functions/closures, staged values, non-scalar loans and borrowed calls, raw/safe foreign calls, async effects |

Unsupported constructs reject the complete requested MIR artifact, including
unused concrete definitions, raw extern declarations and non-entered canonical
loop steps. They are not silently dropped or routed through the HIR emitter.
Retained non-executable generic prototypes are not MIR definitions. The additive
decimal float syntax is specified in [numeric semantics](numerics.md).

This is not equivalence for every HIR program. For example, HIR→C11 can return
and project a nominal `record Point{x:i64}` or match an `Option<i64>`; MIR lacks
nominal layout and tagged-payload descriptors. It also lacks addressable places
and runtime loans, unique moves/drop, defer registration and recoverable cleanup edges.
An omitted latch does not change these boundaries. Within the admitted subset,
compare printed traces and the returned/trapped outcome; the reference
evaluator adds location and resource limits, and its exhaustion is not a C11
program outcome. Native checked arithmetic/assertions use the existing runtime
diagnostics rather than the evaluator's located diagnostic format.

Optional [Novostone relational analyses](backend-analysis.md) now expose MIR
metadata, CFG reachability, transitive calls, conservative effects and liveness
through `--emit-ir-facts` / `--analyze-ir` in an enabled build.
That mapping remains **v1-only**: v2/v3 artifacts reject explicitly until the new
descriptors have a complete relational mapping. No float payload is narrowed to
Novostone's i32 execution dialect.

This does not complete B1's closure/resource/foreign descriptors, source spans
and expansion stacks, cleanup/error edges or full type/layout catalog. It does
not implement Moonstone diagnostic/Syren adapters, Sequent optimization,
general ownership-aware passes, or a native ABI. The optional
[scalar pass pipeline](backend-passes.md) implements checked folding, CFG cleanup,
local-slot forwarding/overwritten-store removal and dead pure-value elimination
with transactional verification.

## Numeric/product v2 contract

- `f64.add/sub/mul/div/neg`, comparisons, i64 conversions, bit transport and
  classification are distinct opcodes with exact signatures. Integer opcodes
  remain i64-only. Floating arithmetic, comparisons and numeric conversions have
  trap effects because their required floating environment is checked; casts
  also check ranges. Bit transport/classification are pure. Both execution paths
  implement the [same binary64 policy](numerics.md), with independent C++ and C
  implementations plus an external Python oracle.
- `tuple` constructs ordered fields, and `extract` projects a statically checked
  index. Types, field count, index bounds and result type are verified. Tuples
  pass/return by value; local slots and simultaneous edge transfers can hold
  tuples. Copies are independent snapshots, not aliases. No fields need a drop
  action in this admitted MIR profile. There is no foreign aggregate ABI claim.
- CLI program entry still requires an argument-free i64/void `main`, including
  saved MIR. API evaluation of another function can return a tuple. Use
  `evaluate_values` with `RuntimeValue` for aggregate arguments and inspect
  `Evaluation.data`. A scalar is an i64 number, bool 0/1, or **typed f64 IEEE bits**;
  a tuple has zero scalar payload and ordered `elements`. Noncanonical NaN
  arguments and malformed tuple shapes reject. The legacy `evaluate` scalar
  argument vector and `Evaluation.value` remain available; float scalar values
  are bit payloads, never silently converted/truncated integers.
- C11 emits dependency-ordered private tuple records and exactly typed calls.
  Float code has the versioned f64 runtime/target guard; the evaluator likewise
  rejects excess-precision/x87 execution instead of double-rounding. Neither reference
  evaluation nor invoking a C compiler is a direct native/VM backend.

Run `examples/float_tuples.on1x` with `--run-ir`, `--emit-ir-c` or the build helper
with `--via-mir` (optionally `--optimize-ir`). It prints 0.5, 0.125 and 4, with
the floats in exact hexadecimal notation.

## Complete scalar numeric v3 contract

- Run `examples/numeric_literals.on1x` through `--run-ir`, `--emit-ir-c`, or
  `on1x-build.py --via-mir` (optionally `--optimize-ir`). Unsigned results retain
  all bits, including u64 values above INT64_MAX; binary32 results retain actual
  single-precision behavior, including special values and signed zero.
- A `numeric` instruction adds one quoted descriptor field after its location.
  Descriptors are fixed allowlisted names (at most 64 bytes), such as `add_u8`,
  `u64_wrapping_mul`, `f32_from_f64`; they are not arbitrary C/foreign symbols.
  The verifier requires the descriptor's exact parameter/result signature,
  canonical payloads, SSA/dominance/CFG invariants and a complete effect chain.
  Other instructions retain their v1/v2 field count and reject descriptor fields.
- v3 numeric instructions conservatively carry trap effects, even for pure
  wrapping/bit operations. No pass may erase/reorder them as pure arithmetic.
  Existing load/slot/phi/CFG passes operate transactionally; new numeric
  arithmetic is not host-folded or guessed to obey signed-i64 rules.
- `RuntimeValue.scalar` is a typed 64-bit storage payload. u64 uses all bits;
  f32 uses only the low 32 bits and canonical NaNs; smaller unsigned types reject
  high bits. The evaluator decodes unsigned values before arithmetic, never
  treats a high-bit u64 as a negative integer, and implements binary32 independently
  from the C11 runtime. External evaluator arguments receive the same validation.
- Scalar-type support does not add records/ADTs/arrays, closure/staging values,
  resources, foreign calls or concurrency to MIR. Floating execution retains
  the guarded, tested Linux x86_64 environment profile, not cross-target certification.

## Representation and invariants

Every module, function, block, parameter, slot, instruction, terminator and effect
definition has a unique nonzero 64-bit ID within the module. Lowering allocates
IDs deterministically in traversal order; no address enters output. Identity is
stable for the same elaborated input, **not** stable across arbitrary source edits.
Specialized function names are retained from HIR. Original locations are carried
unchanged; both profiles have source points, not reconstructed end spans or fabricated ranges.

Entry block parameters are the function signature. Other block parameters are
SSA phi values: every incoming edge supplies the exact number and types of
arguments. Transfers are simultaneous, including loop permutations. Operands
must be defined earlier in their block or in a dominating block. Every block
is reachable, entry has no incoming edges, and every block has one terminator.

Mutable locals use typed, function-local slots with explicit stores and loads.
Their addresses cannot be obtained, passed, captured or returned. A forward
must-initialization fixed point rejects any load that may precede a store;
loop backedges cannot justify initialization on the first iteration. This is
value definite assignment, **not a foreign borrow/ownership proof**.

Each block starts with an effect parameter. Potentially trapping arithmetic,
assertions, loads, stores, printing and all calls consume the current token and
define its successor. The terminator consumes the final token; its selected
edge implicitly passes it to the destination's effect parameter. Pure operations
must have no effect tokens. Unknown opcodes and unsupported types fail closed.
This validates ordering dependencies, not equivalence of arbitrary rewrites.

`verify` runs after lowering and deserialization and before serialization,
evaluation, graph emission and C11 emission. Lowering builds a separate candidate;
failure leaves HIR and an existing CLI output file intact. Opt-in scalar passes
also verify each transition and its preserved analyses. Checked arithmetic traps
are terminal, matching current C11 behavior;
no cleanup or recoverable error semantics are claimed for this scalar subset.

## Format, budgets and testing

The wire root is `(on1x.mir VERSION MODULE_ID (FUNCTIONS...))`, VERSION 1, 2 or 3.
Fixed-shape tagged
records describe functions, blocks, instructions, exits, edges, values and `at`
locations. Operands/references are IDs; immediates preserve all signed 64 bits.
In v2, tuple type fields use `(tuple (TYPE...))`; an f64 `const` immediate is the
signed two's-complement encoding of its IEEE bits. NaNs must use the canonical
positive quiet payload. `extract` uses its immediate as a nonnegative field
index. v1 rejects extended types/opcodes; unknown versions remain errors. Old
readers must reject v2 rather than reinterpret its payloads.
The existing bounded S-expression codec is reused, including ASDL-backed tree
validation in generated builds. Unknown fields/tags/versions, oversized input,
out-of-range integers and malformed IR are rejected. `serialize(deserialize(x))`
normalizes accepted whitespace/spelling. Canonical serialization includes source
paths, so relocation changes artifact hashes. No cache/admission scheme is implied.

Default verification limits: 65,536 identities and 4,194,304 work units, charging
source-path bytes, recursive type admission/equality, operand checks, graph
traversals and initialization analysis. Definite assignment charges construction
and copying of slot sets, predecessor intersections and fixed-point comparisons.
An artifact may require a larger work allowance without changing its MIR.
MIR type nesting is limited to 32; equality results for independently decoded
aggregate shapes are memoized within one verification invocation, with no pointer
identity in persistent artifacts.
Wire limits: 4 MiB, 262,144 syntax nodes, 256 levels. Evaluation defaults to
1,000,000 steps; call depth is capped at 256, potential live frame entries at
262,144, and printed output at 4 MiB. Aggregate frame accounting includes every
nested field, not just root SSA values. Aggregate copies also consume evaluation
work. API callers may lower the step/depth limits.
Exhaustion never reports successful evaluation or verification.

`on1x.mir_hir` tests malformed IR, including a join with a store missing on one
incoming edge, phi loops, round-trips, deterministic IDs, source preservation
and independent budgets. `on1x.mir` executes the same corpus
through HIR C11, ordinary/optimized MIR C11 and both evaluator modes, including
saved artifacts, 120 seeded
arithmetic/comparison cases, ten trap programs, ordered effects, early exits,
imports, macros, PPX, specializations and unsupported-operation rejection. An
independently authored phi permutation is compiled and executed as C11.
Loop regressions also compile and execute C11 from verified saved MIR (plain and
optimized), and reject a forged unreachable block without replacing the output.
See [verification](../VERIFICATION.md) for actual toolchains and results.
