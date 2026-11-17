# Core/MIR: scalar v1, numeric/product v2 and complete scalar numeric v3

This is the first executable **B1 subset** from `BACKEND.md`. The implementation
is in `include/on1x/backend/mir.hpp` and `src/backend/`. It consumes the existing
checked, specialized HIR after imports, macros, PPX and staging admission.
The default `--emit-c` continues to use the established HIR emitter.

Intrinsic [statement loops](core-syntax.md) now lower through explicit
preheader/header/body/latch/exit edges, including labeled break/continue and
once-evaluated i64 ranges. No new MIR opcodes/format version are needed. CFG,
effect, slot-initialization and dominance verification remain mandatory; the
reference evaluator/round-trips/transactional passes execute the same edges.
Resource-bearing loops stay on HIR→C11 and are not approximated as scalar MIR.

Guarded [structural matches](core-syntax.md) lower to verified typed decision
regions before MIR. Numeric/bool values and trivial tuples use existing slots,
extracts, branches and jumps in v1–v3, with mandatory initialization verification
and differential tests. Continues nested in expression regions still execute the
target latch. Records/sums/arrays/slices, strings and managed/foreign resources
remain unsupported MIR profiles; C11 matching does not widen those boundaries.

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
| Observable effects | Scalar `println`, `assert`, checked arithmetic failure, local loads/stores, conservative call barriers |
| Serialization | Versioned canonical S-expression; checked readback and exact structural comparison |
| Provenance | Existing file/byte-offset/line/byte-column locations on definitions and terminators |
| Diagnostics | `ON1X-MIR-VERIFY`, `-DECODE`, `-LOWER`, `-UNSUPPORTED`, `-EVAL`, `-C11` in the existing located diagnostic path |
| Unsupported | Strings, managed/resource-bearing aggregates, records/ADTs/arrays/slices, first-class functions/closures, staged values, resources/borrows, raw/safe foreign calls, async effects |

Unsupported constructs reject the complete requested MIR artifact, including
unused concrete definitions and raw extern declarations. They are not silently
dropped or routed through the HIR emitter. The additive decimal float syntax is
specified in [numeric semantics](numerics.md).

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

The wire root is `(on1x.mir VERSION MODULE_ID (FUNCTIONS...))`, VERSION 1 or 2.
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
traversals and initialization analysis. MIR type nesting is limited to 32;
equality results for independently decoded aggregate shapes are memoized within
one verification invocation, with no pointer identity in persistent artifacts.
Wire limits: 4 MiB, 262,144 syntax nodes, 256 levels. Evaluation defaults to
1,000,000 steps; call depth is capped at 256, potential live frame entries at
262,144, and printed output at 4 MiB. Aggregate frame accounting includes every
nested field, not just root SSA values. Aggregate copies also consume evaluation
work. API callers may lower the step/depth limits.
Exhaustion never reports successful evaluation or verification.

`on1x.mir_hir` tests malformed IR, phi loops, round-trips, deterministic IDs,
source preservation and independent budgets. `on1x.mir` executes the same corpus
through HIR C11, ordinary/optimized MIR C11 and both evaluator modes, including
saved artifacts, 120 seeded
arithmetic/comparison cases, ten trap programs, ordered effects, early exits,
imports, macros, PPX, specializations and unsupported-operation rejection. An
independently authored phi permutation is compiled and executed as C11.
See [verification](../VERIFICATION.md) for actual toolchains and results.
