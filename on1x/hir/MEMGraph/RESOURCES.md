# MEMGraph resource speculation and private scratch placement

`on1xc --check --memgraph-resources warn file.on1x` looks for two possible
problems in the **legacy raw C string ABI**. `error` rejects on a finding,
and `off` (the default) does not run the analysis.

- A direct raw `string` argument may be retained if there is no established
  static storage or a no-retain contract. MEMGraph follows edges from checked
  local reads to simple `let` initializers and through value-region results.
  A C string literal at the end of the chain has static storage. An exclusive
  loan of a `let` invalidates that root's initializer throughout the projection.
  Mutable bindings, parameters, calls, tuple projections, closure captures and
  unmodeled flow are not treated as evidence of static storage.
- A direct raw `string` result may leak **if** the C function transfers an
  allocation to its caller. Neither a `const char *` representation nor the
  ability to read that pointer tells the compiler who owns it or how to
  release it. No destructor is inferred, and no returned pointer is relocated.

The graph refers to the existing verified HIR; it has no editable program or
authority to change admission. It covers direct raw calls, not flattened tuple
parameters, interprocedural alias analysis, other resource families, or
whether a particular raw return really allocates. It does not consume Chisel's
separate admitted-extern sidecar. A clean run is not a safety claim. The
projection is bounded to 16,384 alias/invalidation facts, 4,096 boundaries and
131,072 provenance-walk steps. Exhaustion discards partial findings and
produces one incomplete-analysis diagnostic at the selected severity.

## Storage

Use `--memgraph-stack-bytes N` (0–65,536) on `--emit-c` or `--emit-ir-c`;
omitting it preserves the existing emitters' storage choices. This budgets
only eligible scalar **payload bytes**, not stack-frame size, temporary
descriptors, recursive depth, or other stack allocations.

- **Direct C11:** the FFIHandler allowlist covers only scratch `int64_t` output
  cells the compiler creates for the existing unsafe framed IPC `roundtrip`
  and `poll` runtime adapters. The reviewed bodies in
  [`on1x_kernel_ipc.c`](../../runtime/on1x_kernel_ipc.c) only write the address
  at the end of the synchronous call and do not forward/store it; even the
  callback does not receive this address. On1x's canonical verifier separately
  checks the adapter identity, signature, move/loan and unsafe region. The
  caller reads the cell only for status zero and frees any heap scratch on
  both normal status paths. Eight bytes suffice for stack placement per call
  site. Pair/Session allocation and callback retention are not affected.
- **Verified MIR to C11:** used, non-addressable `i64` slots are selected in
  stable declaration order per function. The canonical MIR verifier validates
  all `Load`/`Store` targets, definite initialization, CFG, and that slot IDs
  cannot be passed or otherwise escape. This backend uses the graph of slot
  uses and verified return edges to materialize placement; a heap cell is
  allocated only when its first store executes, reused on later stores, and
  freed at every normal return. Serialized input is verified again; the
  placement decisions are visible in generated C. Other slot types are
  unchanged. The MIR evaluator, VM and native SDK backends do not implement
  this optional storage policy.

MIR v4 can additionally carry a function-scoped `local-memory` hint. This
advisory profile looks for a used `i64` scratch slot whose loads and stores are
all in the same block without a call or I/O between them. It does not prescribe
a storage location, claim that every slot in the function is local scratch, or
carry any conclusion about pointers or ownership. In particular, raw string
speculation never generates a placement hint.

Stack placement uses a typed C cell. Heap placement uses a TU-private
pointer-plus-bounds descriptor with a checked accessor. A C11 layout assertion
checks the size and alignment of eligible cells. The only FFI wire value is
the existing **thin** `int64_t *`; no fat pointer crosses the C ABI. This
profile assumes normal returns; fatal runtime traps, including heap allocation
failure (status 72), terminate the process rather than unwinding scratch.

## Cephyr evidence boundary and remaining pipeline

At LibStone `e10ac69eed7baf91b06d15c4e0d58360097cb5c4`:

- `cephyr_emit_interface` / `--emit-interface=sexp` gives a target-specific
  C11Interface v1 declaration projection (layout, prototype, calling
  convention, linkage); it does **not** lower bodies or run MIR hooks.
- `cephyr_parse_c11` gives syntax. `CEPHYR_SDK_ANALYZE` gives an ephemeral
  Archon MIR text snapshot, not an ownership or no-retain result handle.
- FactIR's `flow`, `sink`, `dependency` and `observable_input` are bounded
  intraprocedural SSA relationships; they are not memory alias/retention
  proofs. `known_bits_i64` is scalar evidence. None supplies allocation
  origin, deallocator, successful output initialization or callback scope.

Do not substitute any of these outputs for a resource contract. In particular
there is no resource result query in the pinned `Cephyr.h` / `CephyrSDK.h`,
and FFIHandler's executable v2 descriptors are scalar-only. No Cephyr
pointer-resource artifact is consumed by this patch. General integration
requires separately validated resource facts bound to the exact Cephyr
declaration identity, target, preprocessing/header/implementation digests and
reviewed authority. The contract must state origin (caller, foreign, static
or borrowed), escape effects (including transitive callbacks), layout,
initialization on success and failure, and matching release/transfer. A
body-derived producer would also need bounded interprocedural alias/effect
analysis with an explicit incomplete state. FFIHandler must recheck the
fact/descriptor binding and call obligations before MEMGraph can consume
them. Typed MIR foreign-call, resource, cleanup and layout operations, plus
verifier and evaluator/backend support, are still needed to execute the
**same** framed IPC or future admitted resource call from verified MIR.
