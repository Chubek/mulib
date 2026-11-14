# Scalar Core/MIR v1

This is the first executable **B1 subset** from `BACKEND.md`. The implementation
is in `include/on1x/backend/mir.hpp` and `src/backend/`. It consumes the existing
checked, specialized HIR after imports, macros, PPX and staging admission.
The default `--emit-c` continues to use the established HIR emitter.

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

| Feature | Scalar MIR v1 |
|---|---|
| Values | `i64`, `bool`, `void` results |
| Arithmetic | Checked add/subtract/multiply/divide/modulo/negation; comparisons |
| Control | If/else, while, early return, nested lexical scopes, short-circuit logic |
| Calls | Direct ordinary On1x calls, recursion, concrete scalar specializations |
| Observable effects | Scalar `println`, `assert`, checked arithmetic failure, local loads/stores, conservative call barriers |
| Serialization | Versioned canonical S-expression; checked readback and exact structural comparison |
| Provenance | Existing file/byte-offset/line/byte-column locations on definitions and terminators |
| Diagnostics | `ON1X-MIR-VERIFY`, `-DECODE`, `-LOWER`, `-UNSUPPORTED`, `-EVAL`, `-C11` in the existing located diagnostic path |
| Unsupported | Strings, floats, aggregates, first-class functions/closures, staged values, resources/borrows, raw/safe foreign calls |

Unsupported constructs reject the complete requested MIR artifact, including
unused concrete definitions and raw extern declarations. They are not silently
dropped or routed through the HIR emitter. Source syntax has not changed.

Optional [Novostone relational analyses](backend-analysis.md) now expose MIR
metadata, CFG reachability, transitive calls, conservative effects and liveness
through `--emit-ir-facts` / `--analyze-ir` in an enabled build.

This does not complete B1's closure/resource/foreign descriptors, source spans
and expansion stacks, cleanup/error edges or full type/layout catalog. It does
not implement Moonstone diagnostic/Syren adapters, Sequent optimization,
a pass manager, or a native ABI.

## Representation and invariants

Every module, function, block, parameter, slot, instruction, terminator and effect
definition has a unique nonzero 64-bit ID within the module. Lowering allocates
IDs deterministically in traversal order; no address enters output. Identity is
stable for the same elaborated input, **not** stable across arbitrary source edits.
Specialized function names are retained from HIR. Original locations are carried
unchanged; v1 has source points, not reconstructed end spans or fabricated ranges.

Entry block parameters are the function signature. Other block parameters are
SSA phi values: every incoming edge supplies the exact number and types of
arguments. Transfers are simultaneous, including loop permutations. Operands
must be defined earlier in their block or in a dominating block. Every block
is reachable, entry has no incoming edges, and every block has one terminator.

Mutable locals use typed, function-local slots with explicit stores and loads.
Their addresses cannot be obtained, passed, captured or returned. A forward
must-initialization fixed point rejects any load that may precede a store;
loop backedges cannot justify initialization on the first iteration. This is
scalar definite assignment, **not a foreign borrow/ownership proof**.

Each block starts with an effect parameter. Potentially trapping arithmetic,
assertions, loads, stores, printing and all calls consume the current token and
define its successor. The terminator consumes the final token; its selected
edge implicitly passes it to the destination's effect parameter. Pure operations
must have no effect tokens. Unknown opcodes and unsupported types fail closed.
This validates ordering dependencies, not equivalence of arbitrary rewrites.

`verify` runs after lowering and deserialization and before serialization,
evaluation, graph emission and C11 emission. Lowering builds a separate candidate;
failure leaves HIR and an existing CLI output file intact. There are no optimizer
passes yet. Checked arithmetic traps are terminal, matching current C11 behavior;
no cleanup or recoverable error semantics are claimed for this scalar subset.

## Format, budgets and testing

The wire root is `(on1x.mir 1 MODULE_ID (FUNCTIONS...))`. Fixed-shape tagged
records describe functions, blocks, instructions, exits, edges, values and `at`
locations. Operands/references are IDs; immediates preserve all signed 64 bits.
The existing bounded S-expression codec is reused, including ASDL-backed tree
validation in generated builds. Unknown fields/tags/versions, oversized input,
out-of-range integers and malformed IR are rejected. `serialize(deserialize(x))`
normalizes accepted whitespace/spelling. Canonical serialization includes source
paths, so relocation changes artifact hashes. No cache/admission scheme is implied.

Default verification limits: 65,536 identities and 4,194,304 work units, charging
source-path bytes, operand checks, graph traversals and initialization analysis.
Wire limits: 4 MiB, 262,144 syntax nodes, 256 levels. Evaluation defaults to
1,000,000 steps; call depth is capped at 256, potential live frame entries at
262,144, and printed output at 4 MiB. API callers may lower the step/depth limits.
Exhaustion never reports successful evaluation or verification.

`on1x.mir_hir` tests malformed IR, phi loops, round-trips, deterministic IDs,
source preservation and independent budgets. `on1x.mir` executes the same corpus
through HIR C11, MIR C11 and the evaluator, including saved artifacts, 120 seeded
arithmetic/comparison cases, ten trap programs, ordered effects, early exits,
imports, macros, PPX, specializations and unsupported-operation rejection. An
independently authored phi permutation is compiled and executed as C11.
See [verification](../VERIFICATION.md) for actual toolchains and results.
